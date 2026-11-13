# Tamagotchi Vaiwu — Developer Handoff

Complete context dump so another model/developer can continue the project.
Read fully before making changes.

Current state: **v4.9** CONFIRMED WORKING on real hardware by the user -
so everything up to and including idle dimming, dungeon runs, mana potions
and equipment selling runs on the device. White-screen memory bug was fixed
in v2.9.

---

## 1. What this is

A "tamagotchi waifu" virtual-pet + AI-assistant firmware for an **ESP32-S3**
board with 2.8" ILI9341 SPI TFT (320x240 landscape), FT6336 capacitive touch,
ES8311 audio codec (speaker + mic), Wi-Fi, 16 MB flash, **8 MB octal PSRAM**
(Freenove ESP32-S3 2.8" display / xiaozhi `freenove-esp32s3-display-2.8-lcd`).
ESP-IDF **v5.5.4**, C++20, LVGL 9.5.

The pet has stats (hunger/happiness/energy/discipline/weight/coins), can get
sick, die, sleep, be fed/played/washed/talked to. There is an AI chat +
assistant (weather, timer, pomodoro, jokes, unit conversion, alarms), voice
input (Whisper STT), optional TTS, gold-coin economy, a shop, mini-games.

The ESP-IDF HAL drivers (display, touch, codec, Wi-Fi, NVS, SNTP clock,
weather) follow the xiaozhi-esp32 board profile
freenove-esp32s3-display-2.8-lcd and are proven on real hardware.

---

## 2. Where things live

- The ESP-IDF project root is this `firmware/` folder.
- Repository root `../` also holds `server/` (self-hosted STT server,
  whisper.cpp + Caddy) and `hardware/` (3D-print case, board pinout).
- For full host checks you need real LVGL headers: point VAIWU_LVGL_DIR /
  VAIWU_LVGL_PORT_DIR at any ESP-IDF project whose managed_components
  contain lvgl__lvgl and espressif__esp_lvgl_port.

---

## 3. How to verify (no hardware, no idf.py in sandbox)

NEW in v3.7: when VAIWU_LVGL_DIR is NOT set, the harness now syntax-checks
VaiwuUi.cpp/DisplayDriver.cpp ESP branches against a minimal local stub
(`tools/host_stubs/lvgl_stub/`) instead of skipping them. The stub mirrors
LVGL 9 signatures; still run against real LVGL when VAIWU_LVGL_DIR is set, and
extend the stub whenever you use a new LVGL API.

The sandbox **cannot run `idf.py build`** (Espressif servers blocked), no
serial/ELF. Use the host harness:
```
VAIWU_LVGL_DIR=<idf-project>/managed_components/lvgl__lvgl \
VAIWU_LVGL_PORT_DIR=<idf-project>/managed_components/espressif__esp_lvgl_port/include \
python3 tools/run_host_checks.py
```
Check the EXIT CODE, not just the tail of the output (a missed non-zero
exit is exactly how the broken v4.6/v4.7 tags shipped).
It: (1) compiles the pure game core with g++ -Werror and RUNS unit tests
(`tests/test_pet.cpp`, `test_alarm.cpp`, `test_wav.cpp`); (2) compiles host
`#else` branches; (3) syntax-checks the ESP `#ifdef ESP_PLATFORM` branches
against IDF stubs in `tools/host_stubs/` and, if the LVGL dirs are set, against
real LVGL headers. Green bar = "HOST CHECKS PASSED". Run after every edit. When
you use a new ESP-only API, add a stub (e.g. new `CONFIG_VAIWU_*` -> add to
`tools/host_stubs/sdkconfig.h`). Not a real build, but catches ~all compile
errors and all logic bugs.

---

## 4. Components

- `components/core` — pure interfaces (ILogger/ITouch/IStorage/INetwork/IAudio/
  IClock/IWeatherProvider) + ConsoleLogger. No ESP-IDF.
- `components/game` — **pure C++, host-testable, no ESP-IDF**:
  - `VaiwuPet` — sim: stats, tick/decay, feed/play/wash/toilet/talk, emotion &
    life state, coins()/addCoins/spendCoins/applyGift/cure, serialize/
    deserialize (format `V2`, still loads `V1`).
  - `Characters` — `CharacterId` + `CharacterTraits` table
    (`kCharacterCount=7`: Aoi, Hina, Rin, Yuki, Akane[fox], Unicorn[Uni], Kolobok).
  - `I18n` — `enum class Str` + `tr(Lang, Str)`; parallel `kEn`/`kRu` tables in
    I18n.cpp. **kEn and kRu MUST have identical entry counts == Str::Count_.**
  - `OfflinePhrases` — `[character][context][variant]` canned lines (EN+RU).
  - `Assistant` — `parseIntent(utf8, lang)` -> alarm/reminder/timer/pomodoro/
    joke/time/weather/convert/cancel (local RU+EN keywords); `jokeText()`.
  - `Rpg` — the fantasy-RPG layer (spec: full RPG integration, phase 1 of
    docs/RPG.md): 5 stats (str/vit/agi/mag/luck, per-character base sets
    that all sum to 13), XP curve, level-ups (+3 points), spawnFoe (weak
    monster <= pet level), battleTurn (attack/skill/potion/run + foe AI,
    injected RNG) - unit-tested in tests/test_rpg.cpp. RPG progress lives
    in VaiwuPet::rpg(), persisted in save V3 (V2/V1 still load), regen
    2 HP + 1 MP per minute in tick().
  - `Shop` — `enum ShopItem` (18 items) + `ShopItemInfo` + `buyShopItem`;
    HpPotion is `potion=true` (goes to rpg().potions); the 8 equipment
    items have `equip>=0` (EquipItem index): bought once (duplicates are
    refused BEFORE spending), granted via grantEquip. 5 categories now:
    Food / Medicine / Gifts / Weapons / Gear;
    `ShopCategory` (Food/Medicine/Gifts) + `shopCategoryName/Items` — the
    shop UI pages by category, the FEED food menu reuses the Food list.
  - `SoundSynth` — chiptune `renderSfx` (19 effects) + the babble voice:
    `renderVoice(utf8, VoiceProfile, rate)` turns a phrase into one blip
    per vowel (ASCII + Cyrillic, UTF-8 aware), pitch wobbling
    deterministically around the character's base tone;
    `voiceProfileFor(CharacterId)` gives each character her timbre/tempo
    (Hina bright and fast, Kolobok low, Yuki soft triangle...). Capped at
    kVoiceMaxBlips=14 so one line never exceeds ~1.5 s.
- `components/device_hal` — ESP drivers for the Freenove board: Ft6336Touch, Es8311Audio
  (has readMic), SharedI2cBus, NvsStorage, EspWifiNetwork, EspSntpClock,
  OpenMeteoWeatherProvider, WakeWord (ESP-SR opt-in stub).
- `components/chat` — NeuralChat (OpenRouter + worker task + offline fallback),
  SpeechToText (Whisper multipart + makeWav), TextToSpeech (/audio/speech PCM).
- `components/ui` — DisplayDriver (ILI9341 + esp_lvgl_port; backlight is
  LEDC PWM on GPIO45 -> `setBrightness(0-100)`) and **VaiwuUi**
  (~2500 lines, the whole UI), generated sprites.c/.h, icons.c/.h, fonts/.
- `main` — main.cpp composition root + header-only services (SoundService,
  WifiService, WeatherService, VoiceService, AlarmService), each with a
  FreeRTOS worker + mailbox polled by the UI. Kconfig.projbuild.

### RPG battle UI (v3.7)
Game menu (2x4 grid now) got a 6th game: ADVENTURE -> rpgPanel overlay
(foe sprite + HP bar, pet HP/MP bars, 4 wrapped action buttons, 2-line
battle log). Costs 8 energy via startPlay like other games; victory pays
XP+gold (applyPlayResult(true)), defeat loses some gold and never kills
the pet. Level-up queues pendingAlloc: after the panel closes, statPanel
opens (5 rows with "+" buttons, unspent points persist). Monster sprites:
gen_sprites MONSTERS table -> vaiwu_monsters[3] (order = MonsterKind).

### VaiwuUi
Single `UiState* g`. Widgets created once in `init()` under `lvgl_port_lock`.
Screens: select, main, chat, settings, help(about), wifi-list, wifi-pass,
shop (category pager < FOOD/MEDICINE/GIFTS > + up to 4 rows). Main screen
holds hidden full-screen overlays: dialog, game menu (2x3 grid), guess/hearts/
melody/rps/math panels, melody-difficulty picker, food menu, alarm, death.
Timers: `slowTick` (1s: tick/save/poll) + `fastTick` (100ms: game/anim).
`applyTexts()` re-applies all localized strings (call on lang switch).

---

## 5. Generators (never hand-edit their outputs)

- `tools/gen_sprites.py` -> sprites.c/.h (+ assets/*.png). 7 chars x 7 emotions,
  96x128 **RGB565A8** (transparent background: RGB565 plane + A8 plane,
  stride=W*2), procedural PIL. Pastel "caramel" semi-mature chibi. Add a
  character: add to `CHARS`, add `STYLE` entry, extend hair funcs, regenerate.
  Table `vaiwu_sprites[c][e]`. Kolobok is special-cased in `render()` (round
  bun via `draw_kolobok`, face drawn with `Painter(dy=28)`). Also emits
  the 320x140 RGB565 meadow backdrops behind the pet - FOUR time-of-day
  variants (`vaiwu_bgs[4]`: morning/day/evening/night; sun, clouds, sunset,
  moon+stars; `render_bg(phase)` + `BG_STYLE` palette table). The UI picks
  the phase from the wall clock in `refreshScene()` (morning 5-8, day 9-16,
  evening 17-20, night 21-4; unsynced clock -> day).
  Sprite rodata is now ~1.9 MB; the app partition is 4 MB - keep an eye on it.
- `tools/gen_icons.py` -> icons.c/.h. 32x32 ARGB8888 (8 icons incl. `shop`).
- `tools/gen_font.py` -> fonts/vaiwu_font_{12,14,16,20,24}.c + vaiwu_fonts.h.
  DejaVuSans 4bpp, ASCII + Cyrillic 0x401-0x451, `.fallback` = montserrat of
  same size (so LV_SYMBOL icons render). In UI, `F12..F24` = these fonts.
- Needs Pillow (`pip install pillow --break-system-packages`).

---

## 6. Gotchas (learned the hard way)

- **White screen (fixed v2.9):** LVGL builtin 64 KB pool overflowed by the
  7-screen UI while ~200 KB internal RAM was unused -> main task spins ->
  Task WDT -> white. Fix in sdkconfig.defaults: `CONFIG_LV_USE_BUILTIN_MALLOC=n`
  + `CONFIG_LV_USE_CLIB_MALLOC=y` (LVGL uses whole heap) + enable octal PSRAM
  (SPIRAM=y, MODE_OCT, SPEED_80M, USE_MALLOC, TRY_ALLOCATE_WIFI_LRU,
  IGNORE_NOTFOUND, MALLOC_ALWAYSINTERNAL=256). Keep UI lean. main.cpp logs free
  heap before/after UI init.
- **App partition size:** changing `partitions.csv` IS picked up by a
  plain `idf.py build` (unlike sdkconfig.defaults). After growing it, run
  `idf.py flash` so the new partition table lands on the device too.
- **sdkconfig doesn't auto-refresh:** editing sdkconfig.defaults only affects a
  FRESH sdkconfig -> delete sdkconfig or build in a new folder, then fullclean.
- **Cyrillic in C++:** use `\uXXXX` (compiles to UTF-8) or raw UTF-8, be
  consistent per file. Assistant/OfflinePhrases/I18n use raw UTF-8; VaiwuUi uses
  `\uXXXX` for inline strings.
- **Keyboard:** custom RU YCUKEN via `lv_keyboard_set_map(USER_1/USER_2)` with
  enum casts (KB_W/KB_CTL macros); shared setupRuKeyboard/onKbEvent/onLangToggle
  for chat + name keyboards.
- Action buttons hold an lv_image child (icon) not text (language-independent).
- `lv_event_get_current_target` returns `void*` here -> cast to lv_obj_t*.
- buttonmatrix ctrl-map entries need static_cast to lv_buttonmatrix_ctrl_t.

---

## 7. Config (menuconfig -> Tamagotchi Vaiwu Configuration)

VAIWU_WIFI_SSID/PASSWORD, VAIWU_OPENROUTER_API_KEY/MODEL, VAIWU_STT_API_KEY/URL/
MODEL, VAIWU_TTS_URL/API_KEY/MODEL/VOICE (empty URL=off), VAIWU_WEATHER_CITY/
LAT/LON (Moscow), VAIWU_ENABLE_WAKEWORD (opt-in, unverified). Works fully
offline without keys. Free self-hosted STT kit in the repo-root `server/` (whisper.cpp +
Caddy token gateway + deploy.sh); see its README and docs/VOICE.md.

---

## 8. DONE in v3.4 / PENDING WORK

Done in v3.4 (all six items that were pending in v3.3):
1. Main-screen chip is now a real SHOP button (market-stall icon + gold
   balance, `vaiwu_icon_shop`), opens the shop.
2. Shop pages by category like character-select: < FOOD / MEDICINE / GIFTS >,
   up to 4 fixed rows per page (`kShopPageRows`), rows hidden when unused.
3. "Treat" removed from the dialog overlay (3 buttons now: Hug/Pat/Leave);
   treats live only in the FEED food menu, which pulls items from the shop's
   Food category (single source of truth). `Str::Treat` removed from i18n;
   `VaiwuPet::treat()` kept in the core (still unit-tested).
4. Characters Unicorn ("Uni") and Kolobok added: traits, sprites (7x7),
   OfflinePhrases EN+RU (16x3 each), i18n parity kept, tests extended
   (all chars x all contexts x both langs; shop-category coverage).
5. Settings has a Help/"О программе" row -> scrollable about/how-to screen
   (Str::HelpRow/HelpTitle/HelpBody).
6. Fonts reduced F14->F12 + two-line wrapped labels (`wrapButtonLabel`) in
   settings rows, game menu, food menu, melody difficulty, rps.

Done in v4.9 (dungeon bugfix):
- A level-up inside a dungeon ENDED THE RUN silently (user report: never
  reached the boss). fastTick showed the stat-allocation screen and the
  "continue the run" branch was an `else if`, so it never ran; after DONE
  the player was back home with dungeon.active still set.
  Now the deferral is explicit: `resumeDungeon` is set when the level-up
  interrupts a won room, and `onStatDone()` calls the new
  `advanceDungeonAfterVictory()` (boss -> chest, otherwise the rest
  overlay). Both the death path and finishDungeon clear the flag.
- The cleared-run message and the chest are one bubble now (the second
  showBubble used to overwrite the first).

Done in v4.8 (build hotfix):
- v4.6/v4.7 DO NOT COMPILE for the target: encounterHeadline() and
  finishDungeon() are defined after their first use in VaiwuUi.cpp.
  Added the missing forward declarations. Use v4.8 or later.
- Process lesson: `run_host_checks.py` DID catch this, but the failure was
  missed because the output was read with `| tail`/`| grep` instead of the
  exit code. ALWAYS run it as
  `python3 tools/run_host_checks.py; echo EXIT=$?` and require EXIT=0 plus
  the final "HOST CHECKS PASSED" line before shipping.

Done in v4.7 (economy depth):
- Equipment 8 -> 12 items (moon blade, scale mail, silk cloak, sage ring;
  multi-stat bonuses). Shop categories split: Weapons / Armor / Trinkets
  (+ SuperEnergy moved to Gifts so every item still lives in exactly one
  category - the test enforces that). kShopPageRows 4 -> 5, rows compacted.
- Mana potions: ShopItem::MpPotion -> rpg().mpPotions, new RpgAction
  ::MpPotion restores half the max MP; the battle row is now five buttons
  (attack / skill / potion / run / mana).
- Selling: `sellEquip()` pays equipSellPrice (half, min 1), unequips and
  clears ownership; in the UI a LONG PRESS on an inventory slot sells what
  is worn there (tap still cycles). Hint line under the slots.
- Save V5 = V4 + mpPotions; V4/V3/V2/V1 all still load (tested).

Done in v4.6 (dungeon runs):
- `Rpg`: DungeonRun (3-5 rooms), spawnDungeonFoe (+10% stats per room,
  boss room +80% HP / +30% atk / double rewards / +1 level),
  restBetweenRooms (+25% HP, +20% MP, clears guard), dungeonChestGold.
  Unit-tested (room count, boss detection, scaling, rest caps, chest).
- UI: 7th game-menu entry DUNGEON; the battle log gets a "Room x/y" or
  "BOSS ROOM!" headline; after a won room a rest overlay offers DEEPER or
  GO HOME (leaving keeps everything earned); the boss opens the chest and
  shows "Dungeon cleared". Fainting or running ends the run, loot kept.
  Random encounters are suppressed while a run is in progress.

Done in v4.5 (power saving):
- Idle dimming: `tickIdleDimming()` (slowTick, 1 Hz) drops the backlight to
  brightness/5 (min 5%) after the chosen timeout; `wakeScreen()` in
  indevRead restores it on the first touch - the waking tap still works
  normally. The panel never goes fully dark, so the device never looks
  bricked.
- Settings row "Screen dims: never / 30 s / 1 min / 5 min" (NVS key "dim",
  default 1 min). The brightness slider updates the remembered level.

Done in v4.4 (the pet speaks):
- Offline babble voice (see SoundSynth above) - no assets, no network.
  SoundService gained a 1-slot voice queue + `babble(text, character)`;
  the worker prefers effects (instant tap feedback) and plays a phrase
  when no effect is pending. Services::babble wired in main.cpp.
- VaiwuUi::voicePhrase() is the single place the pet is voiced; called
  from showBubble(), so EVERY line she shows is spoken (greetings, offline
  phrases, battle/food/shop reactions, AI chat replies).
- Settings row "Her voice: off / babble / speech" (NVS key "voice",
  default babble). "speech" only appears when TTS is configured
  (services.speak); loading a saved "2" without TTS falls back to babble.
  Switching the row plays a sample line immediately.
- Assistant answers (weather/jokes/time) still use the old say() path -
  they are the assistant talking, not the pet.

Done in v4.3 (hotfix):
- partitions.csv: factory 4M -> 8M. The v4.2 build failed to link into the
  old 4 MB app partition. NVS offset/size untouched -> saves survive.
  Re-flash writes the new partition table (idf.py flash does it), no
  fullclean or sdkconfig deletion needed: partitions.csv is not sdkconfig.

Done in v4.2:
- Personal battle skills: SkillKind per character (skillFor/skillNameFor in
  Rpg): Blast (Aoi), Flurry two-hit (Hina), Focus guaranteed crit (Rin),
  Drain damage+lifesteal (Yuki), Lucky trick that can snag gold (Akane),
  Heal (Uni), Guard halving the next hit via RpgProgress::guardTurns
  (Kolobok). The SKILL button shows the character's own move; the battle
  log narrates second hits / drain / guard / snagged gold. Tests cover
  every kind + the guard damage reduction.
- Fantasy phrase pass: PhraseContext +Victory/+Fainted (kContexts 16->18),
  3 variants x 7 characters x EN+RU (84 new lines) shown after a fight
  instead of the generic "played" line (g->battlePhrase).
- Random encounters: maybeEncounter() rolls once a second from slowTick
  (~4%, 4-minute cooldown, only at home, awake, >50% HP, energy >= 25) and
  shows a FIGHT / IGNORE prompt - nothing ever starts a battle by itself.
  startAdventure() takes an optional pre-rolled foe.
- Battle sounds: Sfx Hit / Crit / Magic / Hurt / Encounter (chiptune, 19
  effects total) wired into onRpgAction.

Done in v4.1:
- Character select shows each character's base RPG stats (selectStats
  label, short names, filled in updateSelectView; name label F20->F16 and
  rows compacted to make room).
- Bestiary 3->6: fox (lvl 3+), ghost (5+), golem (7+) - spawnFoe gates
  kinds by pet level and applies per-kind percent modifiers to HP/atk/
  rewards (golem tanky, ghost hits hard, fox pays better). Sprites in
  gen_sprites MONSTERS (order = MonsterKind!), names in i18n, tests for
  gating and modifiers.

Done in v4.0 (RPG -> life-sim feedback):
- Invested RPG points (allocated + gear, i.e. totalStats MINUS the
  character base - a fresh pet is bit-identical to pre-RPG behavior)
  now modulate the sim in VaiwuPet::applyDecay and the sickness
  thresholds: STR slows hunger decay, VIT slows energy decay (the spec
  example), MAG slows happiness decay - all via statRelief()
  (100/(100+5*pts), floor x0.55); AGI speeds sleep regen (+3%/pt, cap
  +50%); LUCK widens the healthy window (hunger threshold -0.4/pt min 3,
  weight +0.4/pt max 92, shared by refreshDerivedState and cure()).
- Tests: twin-pet comparisons for vit/str/mag, luck threshold shift via
  V4-save injection. All old decay tests pass untouched (base unchanged).

Done in v3.9 (mini-RPG phase 3: home + stats window):
- Home interior backdrops `vaiwu_homes[4]` (render_home: plank floor,
  fireplace, plant, rug, and a WINDOW whose sky follows the time-of-day
  phase) - the main screen scene now shows the home; the meadow
  `vaiwu_bgs[4]` became the battle scenery (rpgScene image in rpgPanel,
  phase picked on startAdventure). Net flash cost ~ +360 KB (homes),
  meadows reused.
- Tap on the stat-bar area (invisible catcher at y24..58) opens
  statsInfoPanel: bar percentages, weight/day/coins, HP/MP/XP/potions,
  effective 5 stats and worn gear; buttons GEAR (jumps to the inventory
  screen; its BACK returns to main via invFromShop=false) and BACK.
- i18n: WeightFmt/DayWordFmt.

Done in v3.8 (mini-RPG phase 2: equipment):
- Rpg: EquipSlot (weapon/armor/accessory) + EquipItem (8 items, flat
  RpgStats bonuses, multi-stat allowed), ownership bitmask + equipped[3]
  in RpgProgress, grantEquip/setEquipped/equipBonus; totalStats includes
  gear, so battles/HP/MP pick it up automatically. Save V4 (V3/V2/V1 load).
- Shop: Weapons + Gear category pages; rows show the bonus and a "*" when
  owned; buying auto-equips; "Already owned!" status for duplicates.
- Inventory screen (button "GEAR"/"ВЕЩИ" next to BACK in the shop): three
  slot rows - tap cycles owned items of that slot then "- none -"; live
  total-stats + HP/MP summary. normalizeRpg sanitizes gear from saves.
- tests: equipment suite in test_rpg.cpp (own/equip/swap/wrong-slot/
  bonuses/V4 roundtrip/V3 compat/shop dedup).

Done in v3.7 (mini-RPG phase 1 of docs/RPG.md):
- game core `Rpg` + tests, save V3, HP/MP regen, HpPotion in the shop;
- ADVENTURE in the game menu -> turn-based battle overlay (3 monster kinds,
  crits/dodges/escape, gentle defeat), level-up stat allocation screen;
- local LVGL stub so the UI ESP branch is now compile-checked in the
  sandbox (it never was before - v3.4..v3.6 UI code passed too).
Next RPG phases (docs/RPG.md): equipment slots + shop category, home
interior + "gates" scene, fantasy reskin of texts.

Done in v3.6:
- Scene backdrop now follows the time of day: 4 generated variants
  (vaiwu_bgs[4]), switched in refreshScene() off the SNTP clock; day is the
  default before the first sync.
- Games cost 8 energy instead of 15 (VaiwuPet::startPlay; test pins it).
- Main screen left column: pet info block (InfoFmt is now "weight name /
  day+sick" on two lines - weight FIRST by request) sits above the VOICE
  button, which moved down to y=100 (92x92).
- docs/RPG.md: full feasibility plan for the requested mini-RPG (verdict:
  yes; battle core as pure C++ + tests, RGB565A8 monster sprites, overlay
  battle UI, save V3, potions via the shop). Ask the user the open
  questions at the bottom of that doc before starting.

Done in v3.5:
- Settings: volume + brightness sliders (live preview while dragging,
  persisted to NVS "vol"/"bri" on release, re-applied at boot in main.cpp;
  brightness slider bottoms out at 10% so the screen can't go black).
  Services gained setVolume/setBrightness; DisplayDriver drives the
  backlight with LEDC PWM (10-bit @ 5 kHz, GPIO45, falls back to on/off).
  Settings became a scrollable list (6 rows) with BACK fixed at the bottom.
- Scene backdrop behind the pet: `vaiwu_bg` (320x140 night meadow with moon,
  stars, hills and a rug) at (0,56); character sprites switched from RGB565
  to RGB565A8 with transparent background so the scene shows through.
- Main screen top bar: clock + wifi + pet info (LV_LABEL_LONG_DOT, width
  108) + shop chip + wider gear all in one row at the top; stat bars moved
  down to y=26/42; rows below freed for the scene.

PENDING (next):
- Run the checks once against REAL LVGL headers (VAIWU_LVGL_DIR=
  <idf-project>/managed_components/lvgl__lvgl) - the sandbox only has the local stub.
- Flash budget: the v4.2 binary was 4.26 MB and BROKE the 4 MB partition
  ("app partition is too small ... overflow 0x10c50"); v4.3 grew factory
  to 8 MB in partitions.csv. Art is ~2.6 MB of that. Next time it gets
  tight, shrink the art (palette/RLE, fewer emotions) rather than growing
  again - `idf.py size` / `idf.py size-components` show the split.
- RPG ideas not started: zones/bosses beyond level 10, equipment for the
  shop's Gear page beyond 8 items, quests from the assistant.
- Make TTS sample-rate robust (still untested on HW).
- More assistant skills (stopwatch, notes, counters).
- De-dup: consider whether the FEED menu should simply open the shop's Food
  page (currently they share data but are separate overlays).

---

## 9. Build & flash (user)

```
cd vaiwu/firmware
idf.py set-target esp32s3
idf.py menuconfig     # optional
idf.py build flash monitor
```
If memory/PSRAM config changed: delete sdkconfig, then idf.py fullclean &&
idf.py build. Monitor should end with "Tamagotchi Vaiwu is running" and show
"Free heap before/after UI" lines.

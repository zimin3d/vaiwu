# Mini-RPG design note (feasibility: YES)

STATUS UPDATE (v3.7): phase 1 SHIPPED - stats/levels/battles/potions are
in (components/game/Rpg.*, tests/test_rpg.cpp, ADVENTURE in the game menu).
User answers to the open questions: keep the tamagotchi sim alongside the
RPG (energy/hunger stay relevant); 5 stats including Luck; battles entered
from the game menu. Phase 2 (equipment) SHIPPED in v3.8: 8 items / 3 slots, shop pages
Weapons+Gear, inventory screen, save V4.
Phase 3 (home + scenes + stats window) SHIPPED in v3.9: home interior on
the main screen (window sky follows the clock), meadow scenery in battle,
tap-on-bars detailed stats window with gear access.
Phase 4 SHIPPED in v4.2: personal skills per character, victory/faint
phrases (EN+RU), random encounters with a FIGHT/IGNORE prompt, battle
sound effects. v4.1 added 3 more monsters with level gating.
Remaining ideas: zones/bosses past level 10, more equipment, quests
handed out by the assistant.

Requested: a small RPG inside the tamagotchi - turn-based battles, an
inventory, HP/mana bars. Verdict after sizing it against this codebase and
hardware: **entirely feasible**, and it fits the existing architecture with
no new dependencies. This doc is the implementation plan for whoever picks
it up.

## Why it fits

- The pattern already exists. Every mini-game is a hidden full-screen
  overlay on the main screen driven by `fastTick`/button callbacks, with a
  pure-C++ rules core in `components/game` covered by host unit tests.
  A battle screen is "just" the biggest one of those.
- Resources are a non-issue: a battle UI is a handful of LVGL widgets
  (~KBs of RAM), the rules core is a few KB of code, and 3-4 enemy
  sprites at 96x96 RGB565A8 cost ~110 KB flash total (app partition has
  ~2 MB headroom after the v3.5 sprite set).
- Persistence: the save format is versioned (`V2` now, loader keeps old
  versions working) - adding RPG fields is a routine `V3` bump.

## v1 scope (one battle screen, no map)

Turn-based duel: pet vs one monster. Player actions per turn:
ATTACK (free), SKILL (costs MP, character-flavored: Rin crits, Uni heals,
Kolobok taunts...), ITEM (potion from inventory), RUN (ends battle, small
happiness penalty). Enemy then acts (simple weighted-random AI).
Victory: coins + XP; level-ups raise max HP/MP. Defeat: NOT pet death -
just lost coins and a sad phrase (keep the tamagotchi contract gentle).

## Architecture (mirror the existing layout)

1. `components/game/Rpg.hpp/.cpp` - pure C++, no LVGL/ESP:
   - `struct Fighter { int hp, hpMax, mp, mpMax, atk, def, level, xp; }`
   - `struct BattleState { Fighter pet, foe; int foeId; Outcome outcome; }`
   - `enum class RpgAction { Attack, Skill, Item, Run }`
   - `step(BattleState&, RpgAction, uint32_t rng) -> TurnLog` - damage
     formulas, MP costs, AI move; RNG passed in so unit tests are exact.
   - enemy table (slime / bat / mushroom / fox-spirit) with per-level
     scaling; `xpForLevel()`, `applyVictory()`.
   - Unit tests in `tests/test_rpg.cpp`, wired into run_host_checks.py.
2. `VaiwuPet` integration: RPG stats persist in save `V3`
   (level/xp/hp/mp/potion count). Battles cost pet energy like other games
   (8 via `startPlay()`), win feeds `applyPlayResult(true)` + coins.
3. Inventory: v1 = potions only, bought in the shop (new items in the
   Medicine category: `HpPotion`, `MpPotion` -> stored counters in the pet,
   not instant effects - first "real" inventory, still one source of truth).
4. UI: `rpgPanel` overlay (like guess/hearts/melody) on the main screen:
   - top: foe sprite (96x96) + foe HP bar; bottom-left: pet portrait
     (reuse existing sprite, scaled) + HP bar (green) + MP bar (blue) -
     `lv_bar` exactly like the stat bars;
   - 2x2 action buttons (F12, wrapped labels - reuse `wrapButtonLabel`);
   - one-line battle log label (localized via new `Str::Rpg*` entries;
     keep kEn/kRu parity!);
   - entry: 6th slot in the game menu ("DUNGEON"), replacing nothing.
5. `tools/gen_sprites.py`: `draw_monster(kind, pose)` - 2 poses
   (idle/hit), same RGB565A8 pipeline; goes into its own `vaiwu_monsters`
   table so the character table stays [7][7].

## Effort estimate (in "one handoff iteration" units)

- rules core + tests + save V3: ~1
- monster sprites + i18n strings (EN+RU): ~1
- battle UI + shop potions + balancing pass: ~1-2
Total: 3-4 iterations, each independently shippable (core can land with
tests before any UI exists).

## Open questions for the user (ask before starting)

- Battles on demand only (game menu), or also random encounters?
- Should defeat cost anything beyond coins (e.g. energy)?
- Per-character skills or one shared skill for v1?

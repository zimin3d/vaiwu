#include "vaiwu/ui/VaiwuUi.hpp"

#include "vaiwu/game/Assistant.hpp"
#include "vaiwu/game/Shop.hpp"
#include "vaiwu/game/OfflinePhrases.hpp"
#include "vaiwu/game/Rpg.hpp"

#ifdef ESP_PLATFORM

#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "vaiwu/ui/icons.h"
#include "vaiwu/ui/sprites.h"
#include "vaiwu/ui/vaiwu_fonts.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace vaiwu::ui {

namespace {

using game::CharacterId;
using game::Emotion;
using game::Lang;
using game::PhraseContext;
using game::Sfx;
using game::Str;

// ---- palette ----------------------------------------------------------------
constexpr std::uint32_t kBgHex = 0x18122B; // must match gen_sprites.py BG
constexpr std::uint32_t kPanelHex = 0x241B3D;
constexpr std::uint32_t kPanelBorderHex = 0x3A2F5C;
constexpr std::uint32_t kTextHex = 0xF2E6FF;
constexpr std::uint32_t kDimHex = 0x9C8FC7;
constexpr std::uint32_t kDangerHex = 0xE04452;
constexpr std::uint32_t kOkHex = 0x37E08D;
constexpr std::uint32_t kBtnHex = 0x322659;
constexpr std::uint32_t kBtnPressedHex = 0x4A3A80;
constexpr std::uint32_t kHeartHex = 0xF2588A;

constexpr std::uint32_t kBarColors[4] = {0xF2A03D, 0xFF6FA5, 0x37C7E0, 0x8FE037};
constexpr std::uint32_t kPadColors[4] = {0xD9484F, 0x3D9FE0, 0x53C267, 0xE0C837};

constexpr int kMiniGameTicks = 30; // guess: 30 x 100 ms = 3 s (spec)
constexpr int kHeartsTicks = 80;   // hearts: 8 s
constexpr int kHeartsTarget = 5;
constexpr int kMelodyMaxLen = 8; // longest melody (Hard difficulty)
constexpr int kMelodyInputTicks = 100; // 10 s to repeat
constexpr int kResultTicks = 14;
constexpr int kBubbleSeconds = 6;
constexpr std::uint32_t kAutosaveSeconds = 60;

// Generated DejaVu fonts (ASCII + Cyrillic) - see tools/gen_font.py.
#define F12 (&vaiwu_font_12)
#define F14 (&vaiwu_font_14)
#define F16 (&vaiwu_font_16)
#define F20 (&vaiwu_font_20)
#define F24 (&vaiwu_font_24)

constexpr int kCharCount = game::kCharacterCount; // number of selectable characters
constexpr int kShopPageRows = 5; // max item rows on one shop category page

enum class GameKind : int { None = 0, Guess, Hearts, Melody, Rps, Math, Rpg };
constexpr std::uint32_t kWinReward = 2; // base coin reward for a win

struct UiState {
    VaiwuUi::Services svc{};
    Lang lang{Lang::En};

    // screens
    lv_obj_t* selectScreen{nullptr};
    lv_obj_t* mainScreen{nullptr};
    lv_obj_t* settingsScreen{nullptr};
    lv_obj_t* wifiListScreen{nullptr};
    lv_obj_t* wifiPassScreen{nullptr};

    // select screen (one character at a time, paged with < > buttons)
    lv_obj_t* selectTitle{nullptr};
    lv_obj_t* selectPortrait{nullptr};
    lv_obj_t* selectName{nullptr};
    lv_obj_t* selectLikes{nullptr};
    lv_obj_t* selectStats{nullptr}; // base characteristics of the shown character
    lv_obj_t* selectPrevBtn{nullptr};
    lv_obj_t* selectNextBtn{nullptr};
    lv_obj_t* nameCaption{nullptr};
    lv_obj_t* nameHint{nullptr};
    lv_obj_t* nameArea{nullptr};
    lv_obj_t* keyboard{nullptr};
    lv_obj_t* nameLangBtn{nullptr};
    lv_obj_t* nameLangLabel{nullptr};
    lv_obj_t* startLabel{nullptr};
    int selectedChar{0};

    // main screen
    lv_obj_t* bars[4]{};
    lv_obj_t* barCaptions[4]{};
    lv_obj_t* infoLabel{nullptr};
    lv_obj_t* clockLabel{nullptr};
    lv_obj_t* wifiLabel{nullptr};
    lv_obj_t* coinLabel{nullptr};
    lv_obj_t* voiceBtnLabel{nullptr};
    lv_obj_t* sprite{nullptr};
    lv_obj_t* scene{nullptr};
    int scenePhase{-1};
    lv_obj_t* bubblePanel{nullptr};
    lv_obj_t* bubbleLabel{nullptr};

    // dialog overlay
    lv_obj_t* dialogPanel{nullptr};
    lv_obj_t* dialogLabel{nullptr};
    lv_obj_t* dialogBtnLabels[3]{};

    // game-select overlay
    lv_obj_t* menuPanel{nullptr};
    lv_obj_t* menuTitle{nullptr};
    lv_obj_t* menuBtnLabels[8]{}; // 5 games + adventure + dungeon + cancel

    // guess-emotion overlay
    lv_obj_t* guessPanel{nullptr};
    lv_obj_t* guessTitle{nullptr};
    lv_obj_t* guessSprite{nullptr};
    lv_obj_t* guessBar{nullptr};
    lv_obj_t* guessBtnLabels[4]{};

    // catch-hearts overlay
    lv_obj_t* heartsPanel{nullptr};
    lv_obj_t* heartsTitle{nullptr};
    lv_obj_t* heartsCount{nullptr};
    lv_obj_t* heartBtn{nullptr};
    lv_obj_t* heartsBar{nullptr};

    // repeat-melody overlay
    lv_obj_t* melodyPanel{nullptr};
    lv_obj_t* melodyTitle{nullptr};
    lv_obj_t* pads[4]{};

    // food selection overlay (free dish + paid treats)
    lv_obj_t* foodPanel{nullptr};
    lv_obj_t* foodTitle{nullptr};
    lv_obj_t* foodBtnLabels[5]{}; // rice(free) + tasty + cake + sushi + cancel

    // rock-paper-scissors overlay
    lv_obj_t* rpsPanel{nullptr};
    lv_obj_t* rpsTitle{nullptr};
    lv_obj_t* rpsBtnLabels[3]{};

    // quick-math overlay
    lv_obj_t* mathPanel{nullptr};
    lv_obj_t* mathTitle{nullptr};
    lv_obj_t* mathQuestion{nullptr};
    lv_obj_t* mathBtnLabels[4]{};
    int mathAnswer{0};
    int mathOptions[4]{};

    // RPG battle overlay ("adventure outside the gates")
    lv_obj_t* rpgPanel{nullptr};
    lv_obj_t* rpgLog{nullptr};
    lv_obj_t* rpgFoeName{nullptr};
    lv_obj_t* rpgFoeBar{nullptr};
    lv_obj_t* rpgFoeImg{nullptr};
    lv_obj_t* rpgLvlLabel{nullptr};
    lv_obj_t* rpgHpLabel{nullptr};
    lv_obj_t* rpgMpLabel{nullptr};
    lv_obj_t* rpgHpBar{nullptr};
    lv_obj_t* rpgMpBar{nullptr};
    lv_obj_t* rpgBtnLabels[5]{}; // attack / skill / potion / mana / run
    // random encounter prompt ("something is prowling outside")
    lv_obj_t* encounterPanel{nullptr};
    lv_obj_t* encounterLabel{nullptr};
    lv_obj_t* encounterFightLabel{nullptr};
    lv_obj_t* encounterIgnoreLabel{nullptr};
    game::Foe encounterFoe{};
    int encounterCooldown{300}; // seconds before the first possible ambush

    lv_obj_t* rpgScene{nullptr}; // meadow strip behind the fight
    PhraseContext battlePhrase{PhraseContext::Played}; // shown after the panel closes
    game::Foe rpgFoe{};
    bool rpgBattleOver{false};
    game::DungeonRun dungeon{}; // inactive for a single random skirmish

    // between-rooms rest overlay
    lv_obj_t* restPanel{nullptr};
    lv_obj_t* restLabel{nullptr};
    lv_obj_t* restNextLabel{nullptr};
    lv_obj_t* restLeaveLabel{nullptr};
    bool pendingAlloc{false};   // open the stat screen once the panel closes
    bool resumeDungeon{false};  // ...and continue the run after that screen

    // level-up stat allocation overlay
    lv_obj_t* statPanel{nullptr};
    lv_obj_t* statTitle{nullptr};
    lv_obj_t* statPoints{nullptr};
    lv_obj_t* statRowLabels[5]{};
    lv_obj_t* statDoneLabel{nullptr};

    // melody difficulty picker
    lv_obj_t* melodyDiffPanel{nullptr};
    lv_obj_t* melodyDiffTitle{nullptr};
    lv_obj_t* melodyDiffBtnLabels[4]{}; // easy / medium / hard / cancel

    // shop screen (paged by category like the character select)
    lv_obj_t* shopScreen{nullptr};
    lv_obj_t* shopTitle{nullptr};
    lv_obj_t* shopCoins{nullptr};
    lv_obj_t* shopStatus{nullptr};
    lv_obj_t* shopBackLabel{nullptr};
    lv_obj_t* shopCatLabel{nullptr};
    lv_obj_t* shopRows[kShopPageRows]{};
    lv_obj_t* shopRowName[kShopPageRows]{};
    lv_obj_t* shopBuyLabels[kShopPageRows]{};
    int shopCategory{0};
    lv_obj_t* shopInvLabel{nullptr};

    // inventory / equipment screen (reached from the shop)
    lv_obj_t* invScreen{nullptr};
    lv_obj_t* invTitle{nullptr};
    lv_obj_t* invSlotLabels[3]{};
    lv_obj_t* invStats{nullptr};
    lv_obj_t* invBackLabel{nullptr};
    lv_obj_t* invHint{nullptr};
    lv_obj_t* invSlotBtns[3]{};
    bool invFromShop{true}; // where BACK returns to (shop or main screen)

    // detailed stats window (opened by tapping the stat bars)
    lv_obj_t* statsInfoPanel{nullptr};
    lv_obj_t* statsInfoTitle{nullptr};
    lv_obj_t* statsInfoLeft{nullptr};
    lv_obj_t* statsInfoRight{nullptr};
    lv_obj_t* statsInfoBottom{nullptr};
    lv_obj_t* statsInfoGearLabel{nullptr};
    lv_obj_t* statsInfoCloseLabel{nullptr};

    // death overlay
    lv_obj_t* deadPanel{nullptr};
    lv_obj_t* deadMsg{nullptr};
    lv_obj_t* restartLabel{nullptr};

    // settings screen
    lv_obj_t* settingsTitle{nullptr};
    lv_obj_t* langLabel{nullptr};
    lv_obj_t* wifiRowLabel{nullptr};
    lv_obj_t* helpRowLabel{nullptr};
    lv_obj_t* voiceRowLabel{nullptr};
    lv_obj_t* otaRowLabel{nullptr};
    lv_obj_t* otaPanel{nullptr};
    lv_obj_t* otaTitle{nullptr};
    lv_obj_t* otaStatus{nullptr};
    lv_obj_t* otaBar{nullptr};
    lv_obj_t* otaUpdateBtn{nullptr};
    lv_obj_t* otaUpdateLabel{nullptr};
    lv_obj_t* otaRebootBtn{nullptr};
    lv_obj_t* otaRebootLabel{nullptr};
    lv_obj_t* otaCloseLabel{nullptr};
    lv_obj_t* dimRowLabel{nullptr};
    // Idle dimming: index into kDimTimeouts, seconds since the last touch,
    // the user's chosen brightness and whether we are currently dimmed.
    int dimMode{2};
    int idleSeconds{0};
    int brightnessPercent{100};
    bool dimmed{false};
    // 0 = silent, 1 = babble (offline chirping), 2 = real speech via TTS
    // (falls back to babble whenever TTS is not configured/reachable).
    int voiceMode{1};
    lv_obj_t* newGameLabel{nullptr};
    lv_obj_t* settingsBackLabel{nullptr};

    // settings sliders (volume / brightness)
    lv_obj_t* volCaption{nullptr};
    lv_obj_t* briCaption{nullptr};
    lv_obj_t* volSlider{nullptr};
    lv_obj_t* briSlider{nullptr};

    // help / about screen
    lv_obj_t* helpScreen{nullptr};
    lv_obj_t* helpTitle{nullptr};
    lv_obj_t* helpBody{nullptr};
    lv_obj_t* helpBackLabel{nullptr};

    // wifi list screen
    lv_obj_t* wifiListTitle{nullptr};
    lv_obj_t* wifiStatus{nullptr};
    lv_obj_t* wifiRows[5]{};
    lv_obj_t* wifiRowTexts[5]{};
    char wifiSsids[5][33]{};
    lv_obj_t* wifiRescanLabel{nullptr};
    lv_obj_t* wifiBackLabel{nullptr};

    // chat (assistant) screen
    lv_obj_t* chatScreen{nullptr};
    lv_obj_t* chatTitle{nullptr};
    lv_obj_t* chatLog{nullptr};
    lv_obj_t* chatArea{nullptr};
    lv_obj_t* chatKeyboard{nullptr};
    lv_obj_t* chatSendBtn{nullptr};
    lv_obj_t* chatSendLabel{nullptr};
    lv_obj_t* chatBackLabel{nullptr};
    lv_obj_t* chatMicBtn{nullptr};
    lv_obj_t* chatMicLabel{nullptr};
    lv_obj_t* chatModeBtn{nullptr};
    lv_obj_t* chatModeLabel{nullptr};
    lv_obj_t* chatLangBtn{nullptr};
    lv_obj_t* chatLangLabel{nullptr};
    lv_obj_t* chatStatusLabel{nullptr};
    bool awaitingAssistant{false};
    bool awaitingWeather{false};
    bool chatTextMode{false};
    int lastVoiceState{0};

    // alarm overlay (on the main screen, above everything)
    lv_obj_t* alarmPanel{nullptr};
    lv_obj_t* alarmTitleLabel{nullptr};
    lv_obj_t* alarmDismissLabel{nullptr};
    int alarmRingTicks{0};

    // wifi password screen
    lv_obj_t* passTitle{nullptr};
    lv_obj_t* passArea{nullptr};
    lv_obj_t* passKeyboard{nullptr};
    lv_obj_t* passConnectLabel{nullptr};
    lv_obj_t* passStatus{nullptr};
    lv_obj_t* passBackLabel{nullptr};
    char targetSsid[33]{};

    // timers / transient state
    lv_timer_t* slowTimer{nullptr};
    lv_timer_t* fastTimer{nullptr};
    GameKind game{GameKind::None};
    int resultTicksLeft{0};
    lv_obj_t* activeGamePanel{nullptr};
    lv_obj_t* activeGameTitle{nullptr};
    // guess
    int guessAnswer{0};
    int guessTicksLeft{0};
    // hearts
    int heartsCaught{0};
    int heartsTicksLeft{0};
    // melody
    int melodySeq[kMelodyMaxLen]{};
    int melodyLen{4};
    std::uint32_t currentReward{kWinReward}; // coins for winning the active game
    int melodyPhase{0}; // 0 playback, 1 input
    int melodyPlayIndex{0};
    int melodyInputIndex{0};
    int melodyTick{0};
    int melodyFlashPad{-1};
    int melodyInputTicksLeft{0};

    int bubbleSecondsLeft{0};
    bool awaitingChatReply{false};
    std::uint32_t saveCountdown{kAutosaveSeconds};
    std::uint32_t salt{1};
    int breathCounter{0};
    bool wasDead{false};
    bool wasSick{false};
    bool wasSleeping{false};
    int lastWifiState{0};
    bool touchPressed{false};
    std::int16_t touchX{0};
    std::int16_t touchY{0};
};

UiState* g = nullptr;

// Forward declarations for callbacks referenced before their definition.
void onOpenChat(lv_event_t* e);
void onOpenShop(lv_event_t* e);
void refreshShop();
void startAdventure(const game::Foe* prerolled = nullptr);
void startDungeonRun();
void encounterHeadline(char* out, std::size_t n);
void finishDungeon(bool cleared);
void advanceDungeonAfterVictory();
void refreshStatAlloc();
void refreshInventory();
void refreshStatsWindow();
void equipBonusText(game::EquipItem item, char* out, std::size_t n);
void startRpsGame();
void startMathGame();
void startMelodyGame(int length);

// ---- small helpers ----------------------------------------------------------

const char* tr_(Str id)
{
    return game::tr(g->lang, id);
}

const lv_image_dsc_t* spriteFor(CharacterId character, Emotion emotion)
{
    unsigned c = static_cast<unsigned>(character);
    unsigned e = static_cast<unsigned>(emotion);
    if (c >= static_cast<unsigned>(game::kCharacterCount)) c = 0U;
    if (e >= 7U) e = 0U;
    return vaiwu_sprites[c][e];
}

lv_obj_t* mkLabel(lv_obj_t* parent, const lv_font_t* font, std::uint32_t colorHex, const char* text)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(colorHex), 0);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t* mkPanel(lv_obj_t* parent, int x, int y, int w, int h)
{
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, lv_color_hex(kPanelHex), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(kPanelBorderHex), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

// Returns the button; *outLabel receives the centered label for retexting.
lv_obj_t* mkButton(lv_obj_t* parent, int x, int y, int w, int h, const char* text,
    const lv_font_t* font, lv_event_cb_t cb, int index, lv_obj_t** outLabel = nullptr)
{
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_hex(kBtnHex), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(kBtnPressedHex), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t* label = mkLabel(btn, font, kTextHex, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED,
        reinterpret_cast<void*>(static_cast<std::intptr_t>(index)));
    if (outLabel != nullptr) {
        *outLabel = label;
    }
    return btn;
}

// Let a button label wrap to two centered lines (long Russian strings would
// otherwise overflow the button frame).
void wrapButtonLabel(lv_obj_t* label, int width)
{
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
}

// Themed settings slider; cb receives both VALUE_CHANGED (live preview)
// and RELEASED (persist).
lv_obj_t* mkSlider(lv_obj_t* parent, int min, int max, int value, lv_event_cb_t cb)
{
    lv_obj_t* slider = lv_slider_create(parent);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kPanelBorderHex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kHeartHex), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kTextHex), LV_PART_KNOB);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_RELEASED, nullptr);
    return slider;
}

void mkScreenBg(lv_obj_t* screen)
{
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBgHex), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

std::uint32_t nextSalt()
{
    return ++g->salt;
}

// Speak a reply aloud if TTS is configured (no-op otherwise).
void say(const char* text)
{
    if (g->svc.speak && text != nullptr && text[0] != '\0') {
        g->svc.speak(text, static_cast<int>(g->lang));
    }
}

// Everything the pet "says" goes through here: silent, babble or real
// speech, depending on the Settings row. TTS is only used when a URL is
// configured (svc.speak is wired) - otherwise mode 2 babbles instead of
// going quiet.
void voicePhrase(const char* text)
{
    if (g->voiceMode == 0 || text == nullptr || text[0] == '\0') {
        return;
    }
    if (g->voiceMode == 2 && g->svc.speak) {
        g->svc.speak(text, static_cast<int>(g->lang));
        return;
    }
    if (g->svc.babble) {
        g->svc.babble(text);
    }
}

void showBubble(const char* text, int seconds = kBubbleSeconds)
{
    lv_label_set_text(g->bubbleLabel, text);
    lv_obj_remove_flag(g->bubblePanel, LV_OBJ_FLAG_HIDDEN);
    g->bubbleSecondsLeft = seconds;
    voicePhrase(text); // she says her lines out loud
}

void hideBubble()
{
    lv_obj_add_flag(g->bubblePanel, LV_OBJ_FLAG_HIDDEN);
    g->bubbleSecondsLeft = 0;
}

void showPetPhrase(PhraseContext context)
{
    showBubble(game::offlinePhrase(g->svc.pet->character(), context, nextSalt(), g->lang));
}

// Read a persisted 0-100 setting ("vol"/"bri"); clamps and falls back.
int readPercentSetting(const char* key, int fallback)
{
    if (const auto stored = g->svc.storage->readText(key); stored.has_value() && !stored->empty()) {
        const int value = std::atoi(stored->c_str());
        return value < 0 ? 0 : (value > 100 ? 100 : value);
    }
    return fallback;
}

// 0 = never, then 30 s / 1 min / 5 min (spec: save power on battery).
constexpr int kDimTimeouts[4] = {0, 30, 60, 300};

// Any touch wakes the screen back to the user's brightness.
void wakeScreen()
{
    g->idleSeconds = 0;
    if (!g->dimmed) {
        return;
    }
    g->dimmed = false;
    if (g->svc.setBrightness) {
        g->svc.setBrightness(g->brightnessPercent);
    }
}

// Called once per second: dims the backlight (never fully off - the panel
// stays readable and one touch restores it) after the chosen timeout.
void tickIdleDimming()
{
    const int timeout = kDimTimeouts[g->dimMode >= 0 && g->dimMode < 4 ? g->dimMode : 0];
    if (timeout <= 0 || g->dimmed) {
        return;
    }
    if (++g->idleSeconds < timeout) {
        return;
    }
    g->dimmed = true;
    if (g->svc.setBrightness) {
        const int low = g->brightnessPercent / 5;
        g->svc.setBrightness(low < 5 ? 5 : low);
    }
}

void saveNow()
{
    const std::string data = g->svc.pet->serialize();
    if (!data.empty() && !g->svc.storage->writeText("pet", data)) {
        g->svc.logger->warn("VaiwuUi: autosave failed");
    }
    g->svc.pet->clearDirty();
    g->saveCountdown = kAutosaveSeconds;
}

// Fill the character + full tracked state that both prompt modes share.
void fillPetState(chat::PromptContext& ctx)
{
    const game::VaiwuPet& pet = *g->svc.pet;
    const game::PetStats& stats = pet.stats();
    ctx.character = pet.character();
    std::snprintf(ctx.petName, sizeof(ctx.petName), "%s", pet.name().c_str());
    ctx.hunger = static_cast<int>(stats.hunger);
    ctx.happiness = static_cast<int>(stats.happiness);
    ctx.energy = static_cast<int>(stats.energy);
    ctx.discipline = static_cast<int>(stats.discipline);
    ctx.weightKg = static_cast<int>(stats.weightKg);
    ctx.sick = pet.isSick();
    ctx.sleeping = pet.life() == game::LifeState::Sleeping;
    ctx.emotion = pet.emotion();
    ctx.lang = g->lang;
}

void requestChat(const char* action, PhraseContext fallback)
{
    chat::PromptContext ctx;
    fillPetState(ctx);
    std::snprintf(ctx.action, sizeof(ctx.action), "%s", action);
    ctx.fallback = fallback;
    g->awaitingChatReply = true;
    g->svc.chat->requestReply(ctx);
}

void refreshStats()
{
    const game::PetStats& stats = g->svc.pet->stats();
    const float values[4] = {stats.hunger, stats.happiness, stats.energy, stats.discipline};
    for (int i = 0; i < 4; ++i) {
        lv_bar_set_value(g->bars[i], static_cast<std::int32_t>(values[i]), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(g->bars[i],
            lv_color_hex(values[i] < 20.0f ? kDangerHex : kBarColors[i]), LV_PART_INDICATOR);
    }

    char info[96];
    std::snprintf(info, sizeof(info), tr_(Str::InfoFmt),
        static_cast<int>(stats.weightKg),
        g->svc.pet->name().c_str(),
        static_cast<unsigned>(stats.ageSeconds / 86400U + 1U),
        g->svc.pet->isSick() ? tr_(Str::SickTag) : "");
    lv_label_set_text(g->infoLabel, info);
    lv_obj_set_style_text_color(g->infoLabel,
        lv_color_hex(g->svc.pet->isSick() ? kDangerHex : kDimHex), 0);

    lv_image_set_src(g->sprite, spriteFor(g->svc.pet->character(), g->svc.pet->emotion()));

    if (g->coinLabel != nullptr) {
        char coins[16];
        std::snprintf(coins, sizeof(coins), "%u", static_cast<unsigned>(g->svc.pet->coins()));
        lv_label_set_text(g->coinLabel, coins);
    }

    // Body shape: stretch the sprite horizontally with weight (256 = 100%).
    static const std::uint32_t kShapeScaleX[4] = {224, 256, 300, 344};
    const auto shape = static_cast<unsigned>(g->svc.pet->bodyShape());
    lv_image_set_scale_x(g->sprite, kShapeScaleX[shape < 4U ? shape : 1U]);
    lv_image_set_scale_y(g->sprite, 256);
}

void refreshWifiBadge()
{
    const bool connected = g->svc.network->status().status == NetworkStatus::Connected;
    lv_obj_set_style_text_color(g->wifiLabel, lv_color_hex(connected ? kOkHex : kPanelBorderHex), 0);
}

// The scene backdrop follows the wall clock: morning 5-8, day 9-16,
// evening 17-20, night 21-4. Before the first SNTP sync (no Wi-Fi yet)
// the friendly default is the day scene.
void refreshScene(bool synced, int hour)
{
    int phase = 1; // day
    if (synced) {
        phase = (hour >= 5 && hour < 9) ? 0
            : (hour >= 9 && hour < 17)  ? 1
            : (hour >= 17 && hour < 21) ? 2
                                        : 3;
    }
    if (phase != g->scenePhase) {
        g->scenePhase = phase;
        lv_image_set_src(g->scene, vaiwu_homes[phase]); // she lives indoors now
    }
}

void refreshClock()
{
    if (g->svc.clock == nullptr) {
        return;
    }
    const ClockReading reading = g->svc.clock->now();
    char text[8];
    if (reading.synced) {
        std::snprintf(text, sizeof(text), "%02d:%02d", reading.hour, reading.minute);
    } else {
        std::snprintf(text, sizeof(text), "--:--");
    }
    lv_label_set_text(g->clockLabel, text);
    refreshScene(reading.synced, reading.hour);
}

void updateSelectView()
{
    const auto id = static_cast<CharacterId>(g->selectedChar);
    const auto& traits = game::traitsFor(id);
    lv_image_set_src(g->selectPortrait, spriteFor(id, Emotion::Idle));
    lv_label_set_text(g->selectName, traits.name);
    lv_obj_set_style_text_color(g->selectName, lv_color_hex(traits.accentColor), 0);
    char likes[48];
    std::snprintf(likes, sizeof(likes), "%s %s", tr_(Str::Likes), traits.favoriteFood);
    lv_label_set_text(g->selectLikes, likes);

    // Base RPG characteristics, so the build is part of the choice.
    const game::RpgStats& base = game::baseStatsFor(id);
    char stats[96];
    std::snprintf(stats, sizeof(stats), "%s %d  %s %d  %s %d  %s %d  %s %d",
        tr_(Str::StatShortStr), base.str, tr_(Str::StatShortVit), base.vit,
        tr_(Str::StatShortAgi), base.agi, tr_(Str::StatShortMag), base.mag,
        tr_(Str::StatShortLuck), base.luck);
    lv_label_set_text(g->selectStats, stats);
}

// Re-apply every localized string (called at build time and on language switch).
void applyTexts()
{
    lv_label_set_text(g->selectTitle, tr_(Str::ChooseTitle));
    lv_label_set_text(g->nameCaption, tr_(Str::NameLabel));
    lv_label_set_text(g->nameHint, tr_(Str::NameHint));
    lv_textarea_set_placeholder_text(g->nameArea, tr_(Str::NamePlaceholder));
    lv_label_set_text(g->startLabel, tr_(Str::Start));
    updateSelectView();

    static const Str kBarStrs[4] = {Str::BarFood, Str::BarMood, Str::BarEnergy, Str::BarDisc};
    for (int i = 0; i < 4; ++i) {
        lv_label_set_text(g->barCaptions[i], tr_(kBarStrs[i]));
    }
    static const Str kDialogStrs[3] = {Str::Hug, Str::Pat, Str::Leave};
    for (int i = 0; i < 3; ++i) {
        lv_label_set_text(g->dialogBtnLabels[i], tr_(kDialogStrs[i]));
    }
    static const Str kMenuStrs[8] = {Str::GameGuess, Str::GameHearts, Str::GameMelody,
        Str::GameRps, Str::GameMath, Str::GameAdventure, Str::GameDungeon, Str::Cancel};
    lv_label_set_text(g->menuTitle, tr_(Str::GamesTitle));
    for (int i = 0; i < 8; ++i) {
        lv_label_set_text(g->menuBtnLabels[i], tr_(kMenuStrs[i]));
    }
    lv_label_set_text(g->rpgBtnLabels[0], tr_(Str::RpgAttack));
    // The skill button carries the character's own move (Heal, Flurry...).
    lv_label_set_text(g->rpgBtnLabels[1], tr_(game::skillNameFor(g->svc.pet->character())));
    lv_label_set_text(g->rpgBtnLabels[3], tr_(Str::RpgRun));
    {
        char potion[32];
        std::snprintf(potion, sizeof(potion), tr_(Str::RpgPotionFmt), g->svc.pet->rpg().potions);
        lv_label_set_text(g->rpgBtnLabels[2], potion);
        std::snprintf(potion, sizeof(potion), tr_(Str::RpgMpPotionFmt), g->svc.pet->rpg().mpPotions);
        lv_label_set_text(g->rpgBtnLabels[4], potion);
    }
    lv_label_set_text(g->restNextLabel, tr_(Str::DungeonNext));
    lv_label_set_text(g->restLeaveLabel, tr_(Str::DungeonLeave));
    lv_label_set_text(g->encounterFightLabel, tr_(Str::EncounterFight));
    lv_label_set_text(g->encounterIgnoreLabel, tr_(Str::EncounterIgnore));
    lv_label_set_text(g->statDoneLabel, tr_(Str::RpgDone));
    refreshStatAlloc();
    lv_label_set_text(g->shopInvLabel, tr_(Str::Inventory));
    lv_label_set_text(g->invTitle, tr_(Str::InventoryTitle));
    lv_label_set_text(g->invBackLabel, tr_(Str::Back));
    refreshInventory();
    lv_label_set_text(g->statsInfoGearLabel, tr_(Str::Inventory));
    lv_label_set_text(g->statsInfoCloseLabel, tr_(Str::Back));
    refreshStatsWindow();
    static const Str kRpsStrs[3] = {Str::RpsRock, Str::RpsPaper, Str::RpsScissors};
    for (int i = 0; i < 3; ++i) {
        lv_label_set_text(g->rpsBtnLabels[i], tr_(kRpsStrs[i]));
    }
    static const Str kDiffStrs[4] = {Str::DiffEasy, Str::DiffMedium, Str::DiffHard, Str::Cancel};
    lv_label_set_text(g->melodyDiffTitle, tr_(Str::DifficultyTitle));
    for (int i = 0; i < 4; ++i) {
        lv_label_set_text(g->melodyDiffBtnLabels[i], tr_(kDiffStrs[i]));
    }
    lv_label_set_text(g->foodTitle, tr_(Str::FoodTitle));
    {
        char rice[48];
        std::snprintf(rice, sizeof(rice), "%s (%s)", tr_(Str::FoodRice), tr_(Str::FoodFree));
        lv_label_set_text(g->foodBtnLabels[0], rice);
        int foodCount = 0;
        const game::ShopItem* food = game::shopCategoryItems(game::ShopCategory::Food, foodCount);
        for (int i = 0; i < 3 && i < foodCount; ++i) {
            const auto& info = game::shopItem(food[i]);
            char row[48];
            std::snprintf(row, sizeof(row), "%s - %u", tr_(info.nameId),
                static_cast<unsigned>(info.price));
            lv_label_set_text(g->foodBtnLabels[i + 1], row);
        }
        lv_label_set_text(g->foodBtnLabels[4], tr_(Str::Cancel));
    }
    lv_label_set_text(g->shopTitle, tr_(Str::ShopTitle));
    lv_label_set_text(g->shopBackLabel, tr_(Str::Back));
    for (int i = 0; i < kShopPageRows; ++i) {
        lv_label_set_text(g->shopBuyLabels[i], tr_(Str::Buy));
    }
    refreshShop(); // category header + item rows are localized too
    static const Str kEmoStrs[4] = {Str::EmoHappy, Str::EmoSad, Str::EmoAngry, Str::EmoSleepy};
    for (int i = 0; i < 4; ++i) {
        lv_label_set_text(g->guessBtnLabels[i], tr_(kEmoStrs[i]));
    }

    lv_label_set_text(g->deadMsg, tr_(Str::GoneMsg));
    lv_label_set_text(g->restartLabel, tr_(Str::Restart));

    lv_label_set_text(g->settingsTitle, tr_(Str::Settings));
    char lang[64];
    std::snprintf(lang, sizeof(lang), "%s: %s", tr_(Str::Language), tr_(Str::LanguageValue));
    lv_label_set_text(g->langLabel, lang);
    lv_label_set_text(g->wifiRowLabel, tr_(Str::WifiNetworks));
    {
        static const Str kVoiceModes[3] = {Str::VoiceOff, Str::VoiceBabble, Str::VoiceSpeech};
        char row[64];
        std::snprintf(row, sizeof(row), tr_(Str::VoiceRow),
            tr_(kVoiceModes[g->voiceMode >= 0 && g->voiceMode < 3 ? g->voiceMode : 1]));
        lv_label_set_text(g->voiceRowLabel, row);
    }
    {
        static const Str kDimNames[4] = {Str::DimOff, Str::Dim30s, Str::Dim1m, Str::Dim5m};
        char row[64];
        std::snprintf(row, sizeof(row), tr_(Str::DimRow),
            tr_(kDimNames[g->dimMode >= 0 && g->dimMode < 4 ? g->dimMode : 0]));
        lv_label_set_text(g->dimRowLabel, row);
    }
    lv_label_set_text(g->helpRowLabel, tr_(Str::HelpRow));
    lv_label_set_text(g->otaRowLabel, tr_(Str::OtaRow));
    lv_label_set_text(g->otaTitle, tr_(Str::OtaTitle));
    lv_label_set_text(g->otaUpdateLabel, tr_(Str::OtaUpdateBtn));
    lv_label_set_text(g->otaRebootLabel, tr_(Str::OtaRebootBtn));
    lv_label_set_text(g->otaCloseLabel, tr_(Str::Back));
    lv_label_set_text(g->helpTitle, tr_(Str::HelpTitle));
    lv_label_set_text(g->helpBody, tr_(Str::HelpBody));
    lv_label_set_text(g->helpBackLabel, tr_(Str::Back));
    lv_label_set_text(g->volCaption, tr_(Str::Volume));
    lv_label_set_text(g->briCaption, tr_(Str::Brightness));
    char reset[96];
    std::snprintf(reset, sizeof(reset), "%s %s", tr_(Str::NewGame), tr_(Str::NewGameHint));
    lv_label_set_text(g->newGameLabel, reset);
    lv_label_set_text(g->settingsBackLabel, tr_(Str::Back));

    lv_label_set_text(g->wifiListTitle, tr_(Str::WifiTitle));
    lv_label_set_text(g->wifiRescanLabel, tr_(Str::Rescan));
    lv_label_set_text(g->wifiBackLabel, tr_(Str::Back));
    lv_label_set_text(g->passConnectLabel, tr_(Str::Connect));
    lv_label_set_text(g->passBackLabel, tr_(Str::Back));

    lv_label_set_text(g->chatTitle, tr_(Str::ChatTitle));
    lv_label_set_text(g->voiceBtnLabel, tr_(Str::VoiceBtn));
    lv_textarea_set_placeholder_text(g->chatArea, tr_(Str::ChatPlaceholder));
    lv_label_set_text(g->chatSendLabel, tr_(Str::Send));
    lv_label_set_text(g->chatBackLabel, tr_(Str::Back));
    lv_label_set_text(g->chatStatusLabel, tr_(Str::VoiceHint));
    lv_label_set_text(g->alarmTitleLabel, tr_(Str::AlarmTitle));
    lv_label_set_text(g->alarmDismissLabel, tr_(Str::Ok));

    refreshStats();
}

void showMainScreen()
{
    refreshStats();
    refreshWifiBadge();
    refreshClock();
    hideBubble();
    lv_obj_add_flag(g->dialogPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->menuPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->guessPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->heartsPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->melodyPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->rpsPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->mathPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->melodyDiffPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->foodPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->rpgPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->statPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->statsInfoPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->encounterPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->restPanel, LV_OBJ_FLAG_HIDDEN);
    if (g->svc.pet->life() == game::LifeState::Dead) {
        lv_obj_remove_flag(g->deadPanel, LV_OBJ_FLAG_HIDDEN);
        g->wasDead = true;
    } else {
        lv_obj_add_flag(g->deadPanel, LV_OBJ_FLAG_HIDDEN);
        g->wasDead = false;
    }
    lv_screen_load(g->mainScreen);
    showPetPhrase(PhraseContext::Greeting);
}

// ---- mini-games ---------------------------------------------------------------

// outcome: 1 win (coins), 0 draw (no penalty), -1 lose.
void finishGameOutcome(int outcome, Str banner)
{
    g->game = GameKind::None;
    g->resultTicksLeft = kResultTicks;
    if (g->activeGameTitle != nullptr) {
        lv_label_set_text(g->activeGameTitle, tr_(banner));
    }
    if (outcome > 0) {
        g->svc.pet->applyPlayResult(true);
        g->svc.pet->addCoins(g->currentReward);
        if (g->activeGameTitle != nullptr) {
            char won[64];
            std::snprintf(won, sizeof(won), "%s  +%u", tr_(banner),
                static_cast<unsigned>(g->currentReward));
            lv_label_set_text(g->activeGameTitle, won);
        }
        g->svc.playSfx(Sfx::Win);
    } else if (outcome < 0) {
        g->svc.pet->applyPlayResult(false);
        g->svc.playSfx(Sfx::Lose);
    } else {
        g->svc.pet->giveAffection(3.0f); // draw: small mood, no penalty
        g->svc.playSfx(Sfx::Tap);
    }
    refreshStats();
    saveNow();
}

void finishGame(bool win, Str banner)
{
    finishGameOutcome(win ? 1 : -1, banner);
}

void startGuessGame()
{
    g->game = GameKind::Guess;
    g->currentReward = 2;
    g->activeGamePanel = g->guessPanel;
    g->activeGameTitle = g->guessTitle;
    g->guessAnswer = std::rand() % 4;
    g->guessTicksLeft = kMiniGameTicks;
    static const Emotion kGameEmotions[4] = {Emotion::Happy, Emotion::Sad, Emotion::Angry, Emotion::Sleepy};
    lv_image_set_src(g->guessSprite, spriteFor(g->svc.pet->character(), kGameEmotions[g->guessAnswer]));
    lv_label_set_text(g->guessTitle, tr_(Str::GuessTitle));
    lv_bar_set_value(g->guessBar, kMiniGameTicks, LV_ANIM_OFF);
    lv_obj_remove_flag(g->guessPanel, LV_OBJ_FLAG_HIDDEN);
}

void moveHeart()
{
    const int x = 8 + std::rand() % 244;  // heart is 56 px wide, panel 320
    const int y = 40 + std::rand() % 130; // below the title, above the bar
    lv_obj_set_pos(g->heartBtn, x, y);
}

void startHeartsGame()
{
    g->game = GameKind::Hearts;
    g->currentReward = 2;
    g->activeGamePanel = g->heartsPanel;
    g->activeGameTitle = g->heartsTitle;
    g->heartsCaught = 0;
    g->heartsTicksLeft = kHeartsTicks;
    lv_label_set_text(g->heartsTitle, tr_(Str::HeartsTitle));
    char count[16];
    std::snprintf(count, sizeof(count), "0/%d", kHeartsTarget);
    lv_label_set_text(g->heartsCount, count);
    lv_bar_set_value(g->heartsBar, kHeartsTicks, LV_ANIM_OFF);
    moveHeart();
    lv_obj_remove_flag(g->heartsPanel, LV_OBJ_FLAG_HIDDEN);
}

void flashPad(int index, bool on)
{
    for (int i = 0; i < 4; ++i) {
        const bool lit = on && i == index;
        lv_obj_set_style_bg_color(g->pads[i],
            lv_color_hex(lit ? 0xFFFFFF : kPadColors[i]), 0);
    }
}

void startMelodyGame(int length)
{
    g->melodyLen = length < 1 ? 1 : (length > kMelodyMaxLen ? kMelodyMaxLen : length);
    // Longer melody -> bigger reward: 4->2, 6->3, 8->4 coins.
    g->currentReward = static_cast<std::uint32_t>(2 + (g->melodyLen - 4) / 2);
    g->game = GameKind::Melody;
    g->activeGamePanel = g->melodyPanel;
    g->activeGameTitle = g->melodyTitle;
    for (int i = 0; i < g->melodyLen; ++i) {
        g->melodySeq[i] = std::rand() % 4;
    }
    g->melodyPhase = 0;
    g->melodyPlayIndex = 0;
    g->melodyInputIndex = 0;
    g->melodyTick = 0;
    g->melodyFlashPad = -1;
    g->melodyInputTicksLeft = kMelodyInputTicks;
    flashPad(-1, false);
    lv_label_set_text(g->melodyTitle, tr_(Str::MelodyWatch));
    lv_obj_remove_flag(g->melodyPanel, LV_OBJ_FLAG_HIDDEN);
}

// ---- event callbacks ------------------------------------------------------------

void onActionBtn(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    game::VaiwuPet& pet = *g->svc.pet;
    if (!pet.alive() || g->game != GameKind::None) {
        return;
    }
    g->svc.playSfx(Sfx::Tap);

    using game::ActionResult;
    switch (index) {
    case 0: { // FEED -> choose a dish (free food or a paid treat)
        if (pet.life() == game::LifeState::Sleeping) {
            showPetPhrase(PhraseContext::Sleeping);
        } else {
            lv_obj_remove_flag(g->foodPanel, LV_OBJ_FLAG_HIDDEN);
        }
        break;
    }
    case 1: { // PLAY -> game menu (energy is spent when a game is picked)
        if (pet.life() == game::LifeState::Sleeping) {
            showPetPhrase(PhraseContext::Sleeping);
        } else if (pet.stats().energy < 20.0f) {
            showBubble(tr_(Str::TooTired));
        } else {
            lv_obj_remove_flag(g->menuPanel, LV_OBJ_FLAG_HIDDEN);
        }
        break;
    }
    case 2: { // WASH
        if (pet.wash() == ActionResult::Ok) {
            g->svc.playSfx(Sfx::Wash);
            showPetPhrase(PhraseContext::Washed);
            saveNow();
        } else {
            showPetPhrase(PhraseContext::Sleeping);
        }
        break;
    }
    case 3: { // WC
        if (pet.toilet() == ActionResult::Ok) {
            g->svc.playSfx(Sfx::Toilet);
            showPetPhrase(PhraseContext::Toilet);
            saveNow();
        } else {
            showPetPhrase(PhraseContext::Sleeping);
        }
        break;
    }
    case 4: { // TALK
        if (pet.talk() == ActionResult::Ok) {
            g->svc.playSfx(Sfx::Talk);
            showBubble(tr_(Str::Dots), 10);
            requestChat("talked to you", PhraseContext::Talked);
            saveNow();
        } else {
            showPetPhrase(PhraseContext::Sleeping);
        }
        break;
    }
    default:
        break;
    }
    refreshStats();
}

void onFeedFood(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    lv_obj_add_flag(g->foodPanel, LV_OBJ_FLAG_HIDDEN);
    game::VaiwuPet& pet = *g->svc.pet;
    if (index >= 4) { // cancel
        g->svc.playSfx(Sfx::Tap);
        return;
    }
    if (index == 0) { // free basic food (rice)
        switch (pet.feed()) {
        case game::ActionResult::Ok:
            g->svc.playSfx(Sfx::Feed);
            showPetPhrase(PhraseContext::Fed);
            saveNow();
            break;
        case game::ActionResult::RefusedCaprice:
            showPetPhrase(PhraseContext::RefusedFood);
            break;
        case game::ActionResult::NotHungry:
            showBubble(tr_(Str::FullAlready));
            break;
        default:
            break;
        }
        refreshStats();
        return;
    }
    // Paid treats spend gold (same items and prices as the shop FOOD page).
    int foodCount = 0;
    const game::ShopItem* food = game::shopCategoryItems(game::ShopCategory::Food, foodCount);
    if (index - 1 < foodCount && game::buyShopItem(pet, food[index - 1])) {
        g->svc.playSfx(Sfx::Feed);
        showPetPhrase(PhraseContext::Fed);
        saveNow();
    } else {
        showBubble(tr_(Str::NotEnoughCoins));
    }
    refreshStats();
}

void onMenuBtn(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    lv_obj_add_flag(g->menuPanel, LV_OBJ_FLAG_HIDDEN);
    g->svc.playSfx(Sfx::Tap);
    if (index >= 7) { // cancel
        return;
    }
    if (index == 2) { // melody -> pick difficulty first (energy spent on start)
        lv_obj_remove_flag(g->melodyDiffPanel, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (g->svc.pet->startPlay() != game::ActionResult::Ok) {
        showBubble(tr_(Str::TooTired));
        return;
    }
    g->svc.playSfx(Sfx::PlayStart);
    switch (index) {
    case 0: startGuessGame(); break;
    case 1: startHeartsGame(); break;
    case 3: startRpsGame(); break;
    case 4: startMathGame(); break;
    case 5: startAdventure(nullptr); break; // out of the gates, into a fight
    case 6: startDungeonRun(); break;      // a chain of rooms with a boss
    default: break;
    }
    refreshStats();
}

void onMelodyDiff(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    lv_obj_add_flag(g->melodyDiffPanel, LV_OBJ_FLAG_HIDDEN);
    g->svc.playSfx(Sfx::Tap);
    if (index >= 3) { // cancel
        return;
    }
    if (g->svc.pet->startPlay() != game::ActionResult::Ok) {
        showBubble(tr_(Str::TooTired));
        return;
    }
    g->svc.playSfx(Sfx::PlayStart);
    static const int kLengths[3] = {4, 6, 8}; // easy / medium / hard
    startMelodyGame(kLengths[index]);
    refreshStats();
}

void onGuessBtn(lv_event_t* e)
{
    if (g->game != GameKind::Guess) {
        return;
    }
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    finishGame(index == g->guessAnswer, index == g->guessAnswer ? Str::WinBanner : Str::LoseBanner);
}

void onHeartBtn(lv_event_t* e)
{
    static_cast<void>(e);
    if (g->game != GameKind::Hearts) {
        return;
    }
    g->svc.playSfx(Sfx::Tap);
    ++g->heartsCaught;
    char count[16];
    std::snprintf(count, sizeof(count), "%d/%d", g->heartsCaught, kHeartsTarget);
    lv_label_set_text(g->heartsCount, count);
    if (g->heartsCaught >= kHeartsTarget) {
        finishGame(true, Str::WinBanner);
    } else {
        moveHeart();
    }
}

void onPadBtn(lv_event_t* e)
{
    if (g->game != GameKind::Melody || g->melodyPhase != 1) {
        return;
    }
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    static const Sfx kNotes[4] = {Sfx::Note1, Sfx::Note2, Sfx::Note3, Sfx::Note4};
    g->svc.playSfx(kNotes[index]);
    if (index != g->melodySeq[g->melodyInputIndex]) {
        finishGame(false, Str::LoseBanner);
        return;
    }
    if (++g->melodyInputIndex >= g->melodyLen) {
        finishGame(true, Str::WinBanner);
    }
}

void startRpsGame()
{
    g->game = GameKind::Rps;
    g->currentReward = 2;
    g->activeGamePanel = g->rpsPanel;
    g->activeGameTitle = g->rpsTitle;
    lv_label_set_text(g->rpsTitle, tr_(Str::GameRps));
    lv_obj_remove_flag(g->rpsPanel, LV_OBJ_FLAG_HIDDEN);
}

void onRpsBtn(lv_event_t* e)
{
    if (g->game != GameKind::Rps) {
        return;
    }
    const int user = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    const int pet = std::rand() % 3; // 0 rock, 1 paper, 2 scissors
    // user wins when (user - pet) mod 3 == 1.
    const int diff = (user - pet + 3) % 3;
    if (diff == 0) {
        finishGameOutcome(0, Str::RpsDraw);
    } else if (diff == 1) {
        finishGameOutcome(1, Str::RpsWin);
    } else {
        finishGameOutcome(-1, Str::RpsLose);
    }
}

void startMathGame()
{
    g->game = GameKind::Math;
    g->currentReward = 3; // arithmetic is harder -> pays more
    g->activeGamePanel = g->mathPanel;
    g->activeGameTitle = g->mathTitle;
    lv_label_set_text(g->mathTitle, tr_(Str::MathTitle));

    const int a = std::rand() % 12 + 1;
    const int b = std::rand() % 12 + 1;
    const int op = std::rand() % 3; // 0 +, 1 -, 2 *
    int answer = 0;
    char q[24];
    if (op == 0) {
        answer = a + b;
        std::snprintf(q, sizeof(q), "%d + %d = ?", a, b);
    } else if (op == 1) {
        const int hi = a > b ? a : b;
        const int lo = a > b ? b : a;
        answer = hi - lo;
        std::snprintf(q, sizeof(q), "%d - %d = ?", hi, lo);
    } else {
        answer = a * b;
        std::snprintf(q, sizeof(q), "%d x %d = ?", a, b);
    }
    lv_label_set_text(g->mathQuestion, q);
    g->mathAnswer = answer;

    // Place the correct answer in a random slot; fill others with near misses.
    const int correctSlot = std::rand() % 4;
    for (int i = 0; i < 4; ++i) {
        if (i == correctSlot) {
            g->mathOptions[i] = answer;
        } else {
            int wrong = 0;
            bool unique = false;
            while (!unique) {
                const int delta = std::rand() % 11 - 5; // -5..+5
                wrong = answer + (delta == 0 ? 1 : delta);
                if (wrong < 0) {
                    wrong = answer + 1 + std::rand() % 5;
                }
                unique = true;
                for (int j = 0; j < i; ++j) {
                    if (g->mathOptions[j] == wrong) {
                        unique = false;
                        break;
                    }
                }
                if (wrong == answer) {
                    unique = false;
                }
            }
            g->mathOptions[i] = wrong;
        }
        char opt[12];
        std::snprintf(opt, sizeof(opt), "%d", g->mathOptions[i]);
        lv_label_set_text(g->mathBtnLabels[i], opt);
    }
    lv_obj_remove_flag(g->mathPanel, LV_OBJ_FLAG_HIDDEN);
}

void onMathBtn(lv_event_t* e)
{
    if (g->game != GameKind::Math) {
        return;
    }
    const int i = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    finishGameOutcome(g->mathOptions[i] == g->mathAnswer ? 1 : -1,
        g->mathOptions[i] == g->mathAnswer ? Str::WinBanner : Str::LoseBanner);
}

// ---- RPG battle ("adventure outside the gates") -----------------------------

Str monsterNameId(game::MonsterKind kind)
{
    switch (kind) {
    case game::MonsterKind::Bat: return Str::MonBat;
    case game::MonsterKind::Shroom: return Str::MonShroom;
    case game::MonsterKind::Fox: return Str::MonFox;
    case game::MonsterKind::Ghost: return Str::MonGhost;
    case game::MonsterKind::Golem: return Str::MonGolem;
    case game::MonsterKind::Slime:
    default: return Str::MonSlime;
    }
}

// Bars, HP/MP/level captions and the potion-count button label.
void refreshBattleHud()
{
    game::VaiwuPet& pet = *g->svc.pet;
    game::RpgProgress& p = pet.rpg();
    const game::CharacterId id = pet.character();
    game::normalizeRpg(id, p);

    char text[48];
    lv_bar_set_range(g->rpgHpBar, 0, game::hpMaxFor(id, p));
    lv_bar_set_value(g->rpgHpBar, p.hp, LV_ANIM_OFF);
    std::snprintf(text, sizeof(text), "HP %d/%d", p.hp, game::hpMaxFor(id, p));
    lv_label_set_text(g->rpgHpLabel, text);

    lv_bar_set_range(g->rpgMpBar, 0, game::mpMaxFor(id, p));
    lv_bar_set_value(g->rpgMpBar, p.mp, LV_ANIM_OFF);
    std::snprintf(text, sizeof(text), "MP %d/%d", p.mp, game::mpMaxFor(id, p));
    lv_label_set_text(g->rpgMpLabel, text);

    std::snprintf(text, sizeof(text), "Lv.%d  XP %d/%d", p.level, p.xp, game::xpForNext(p.level));
    lv_label_set_text(g->rpgLvlLabel, text);

    lv_bar_set_range(g->rpgFoeBar, 0, g->rpgFoe.hpMax);
    lv_bar_set_value(g->rpgFoeBar, g->rpgFoe.hp, LV_ANIM_OFF);
    std::snprintf(text, sizeof(text), "%s  Lv.%d", tr_(monsterNameId(g->rpgFoe.kind)), g->rpgFoe.level);
    lv_label_set_text(g->rpgFoeName, text);

    std::snprintf(text, sizeof(text), tr_(Str::RpgPotionFmt), p.potions);
    lv_label_set_text(g->rpgBtnLabels[2], text);
    std::snprintf(text, sizeof(text), tr_(Str::RpgMpPotionFmt), p.mpPotions);
    lv_label_set_text(g->rpgBtnLabels[4], text);
    lv_label_set_text(g->rpgBtnLabels[1], tr_(game::skillNameFor(pet.character())));
}

void startAdventure(const game::Foe* prerolled)
{
    game::VaiwuPet& pet = *g->svc.pet;
    g->game = GameKind::Rpg;
    g->rpgBattleOver = false;
    g->activeGamePanel = g->rpgPanel;
    g->activeGameTitle = nullptr; // the battle log narrates instead
    g->rpgFoe = prerolled != nullptr
        ? *prerolled
        : game::spawnFoe(pet.rpg().level,
            static_cast<std::uint32_t>(std::rand()) * 2654435761U ^ nextSalt());
    g->dungeon = game::DungeonRun{}; // a lone skirmish, not a run
    lv_image_set_src(g->rpgFoeImg, vaiwu_monsters[static_cast<int>(g->rpgFoe.kind)]);
    // Step outside: the meadow matches the current time of day.
    lv_image_set_src(g->rpgScene, vaiwu_bgs[g->scenePhase >= 0 ? g->scenePhase : 1]);
    char enc[160];
    encounterHeadline(enc, sizeof(enc));
    lv_label_set_text(g->rpgLog, enc);
    refreshBattleHud();
    lv_obj_remove_flag(g->rpgPanel, LV_OBJ_FLAG_HIDDEN);
}

// Puts the room counter in front of the encounter line during a run.
void encounterHeadline(char* out, std::size_t n)
{
    char enc[96];
    std::snprintf(enc, sizeof(enc), tr_(Str::RpgEncounterFmt),
        tr_(monsterNameId(g->rpgFoe.kind)), g->rpgFoe.level);
    if (!g->dungeon.active) {
        std::snprintf(out, n, "%s", enc);
        return;
    }
    char room[48];
    if (game::isBossRoom(g->dungeon)) {
        std::snprintf(room, sizeof(room), "%s", tr_(Str::DungeonBossRoom));
    } else {
        std::snprintf(room, sizeof(room), tr_(Str::DungeonRoomFmt),
            g->dungeon.room + 1, g->dungeon.rooms);
    }
    std::snprintf(out, n, "%s\n%s", room, enc);
}

// Opens the fight for the current dungeon room (or the next one).
void enterDungeonRoom()
{
    game::VaiwuPet& pet = *g->svc.pet;
    g->game = GameKind::Rpg;
    g->rpgBattleOver = false;
    g->activeGamePanel = g->rpgPanel;
    g->activeGameTitle = nullptr;
    g->rpgFoe = game::spawnDungeonFoe(g->dungeon, pet.rpg().level,
        static_cast<std::uint32_t>(std::rand()) * 2654435761U ^ nextSalt());
    lv_image_set_src(g->rpgFoeImg, vaiwu_monsters[static_cast<int>(g->rpgFoe.kind)]);
    lv_image_set_src(g->rpgScene, vaiwu_bgs[g->scenePhase >= 0 ? g->scenePhase : 1]);
    char headline[160];
    encounterHeadline(headline, sizeof(headline));
    lv_label_set_text(g->rpgLog, headline);
    refreshBattleHud();
    lv_obj_remove_flag(g->rpgPanel, LV_OBJ_FLAG_HIDDEN);
}

void startDungeonRun()
{
    g->dungeon = game::startDungeon(g->svc.pet->rpg().level,
        static_cast<std::uint32_t>(std::rand()) ^ nextSalt());
    enterDungeonRoom();
}

// The fight ended (any outcome): keep the panel briefly so the final log
// line can be read, then fastTick hides it (and opens the stat screen if a
// level-up granted points).
void endBattle()
{
    g->rpgBattleOver = true;
    g->game = GameKind::None;
    g->resultTicksLeft = kResultTicks * 2;
    saveNow();
    refreshStats();
}

void onRpgAction(lv_event_t* e)
{
    if (g->game != GameKind::Rpg || g->rpgBattleOver) {
        return;
    }
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    game::VaiwuPet& pet = *g->svc.pet;
    const game::CharacterId id = pet.character();
    const game::TurnResult r = game::battleTurn(id, pet.rpg(), g->rpgFoe,
        static_cast<game::RpgAction>(index), static_cast<std::uint32_t>(std::rand()));

    // Narrate the turn on (up to) two lines.
    char line1[112] = "";
    char line2[112] = "";
    if (r.playerStunned) {
        std::snprintf(line1, sizeof(line1), "%s", tr_(Str::RpgYouStunned));
    } else if (r.noMp) {
        std::snprintf(line1, sizeof(line1), "%s", tr_(Str::RpgNoMp));
    } else if (r.noPotion) {
        std::snprintf(line1, sizeof(line1), "%s", tr_(Str::RpgNoPotion));
    } else if (r.escaped) {
        std::snprintf(line1, sizeof(line1), "%s", tr_(Str::RpgRunOk));
    } else {
        if (r.mpRestored > 0) {
            std::snprintf(line1, sizeof(line1), tr_(Str::RpgMpRestoredFmt), r.mpRestored);
        } else if (r.healed > 0) {
            std::snprintf(line1, sizeof(line1), tr_(Str::RpgHealedFmt), r.healed);
        } else if (r.runFailed) {
            std::snprintf(line1, sizeof(line1), "%s", tr_(Str::RpgRunFail));
        } else if (r.skillUsed && r.playerDmg == 0 && r.drained > 0) {
            // Uni's heal: no damage, just her own magic
            std::snprintf(line1, sizeof(line1), tr_(Str::RpgHealedFmt), r.drained);
        } else {
            std::snprintf(line1, sizeof(line1), tr_(Str::RpgYouHitFmt), r.playerDmg);
            if (r.playerCrit) {
                std::strncat(line1, tr_(Str::RpgCrit), sizeof(line1) - std::strlen(line1) - 1);
            }
            char extra[48];
            if (r.extraDmg > 0) { // Flurry's second hit
                std::snprintf(extra, sizeof(extra), tr_(Str::RpgSecondHitFmt), r.extraDmg);
                std::strncat(line1, extra, sizeof(line1) - std::strlen(line1) - 1);
            }
            if (r.drained > 0) { // Drain fed her some HP back
                std::snprintf(extra, sizeof(extra), tr_(Str::RpgDrainFmt), r.drained);
                std::strncat(line1, extra, sizeof(line1) - std::strlen(line1) - 1);
            }
            if (r.guarded > 0) {
                std::strncat(line1, tr_(Str::RpgGuardUp), sizeof(line1) - std::strlen(line1) - 1);
            }
            if (r.snagged > 0) { // Akane's trick pays out immediately
                pet.addCoins(static_cast<std::uint32_t>(r.snagged));
                std::snprintf(extra, sizeof(extra), tr_(Str::RpgSnagFmt), r.snagged);
                std::strncat(line1, extra, sizeof(line1) - std::strlen(line1) - 1);
            }
            if (r.foePoisoned) { // the Wind Dagger's venom kicks in
                std::strncat(line1, tr_(Str::RpgFoePoisoned),
                    sizeof(line1) - std::strlen(line1) - 1);
            }
        }
        if (r.victory) {
            std::snprintf(line2, sizeof(line2), tr_(Str::RpgWinFmt), r.xpGained, r.goldGained);
        } else if (r.defeat) {
            const int lost = std::min(static_cast<int>(pet.coins()), r.goldLostHint);
            std::snprintf(line2, sizeof(line2), tr_(Str::RpgDefeatFmt), lost);
        } else if (r.foeMissed) {
            std::snprintf(line2, sizeof(line2), "%s", tr_(Str::RpgFoeMiss));
        } else if (r.foeDmg > 0) {
            std::snprintf(line2, sizeof(line2), tr_(Str::RpgFoeHitFmt), r.foeDmg);
        }
    }
    // Status events ride along on the second line (they belong to the foe's
    // phase, except the pet's own poison tick - still shown there for space).
    if (!r.noMp && !r.noPotion && !r.escaped) {
        auto append2 = [&](const char* text) {
            std::strncat(line2, text, sizeof(line2) - std::strlen(line2) - 1);
        };
        char part[48];
        if (r.poisoned) { // the slime got through her guard
            append2(tr_(Str::RpgYouPoisoned));
        }
        if (r.poisonDmg > 0) {
            std::snprintf(part, sizeof(part), tr_(Str::RpgPoisonTickFmt), r.poisonDmg);
            append2(part);
        }
        if (r.foePoisonDmg > 0) {
            std::snprintf(part, sizeof(part), tr_(Str::RpgFoePoisonTickFmt), r.foePoisonDmg);
            append2(part);
        }
        if (r.foeRegen > 0) {
            std::snprintf(part, sizeof(part), tr_(Str::RpgFoeRegenFmt), r.foeRegen);
            append2(part);
        }
        if (r.goldStolen > 0) { // the fox pickpockets mid-fight
            static_cast<void>(pet.spendCoins(static_cast<std::uint32_t>(
                std::min(static_cast<int>(pet.coins()), r.goldStolen))));
            std::snprintf(part, sizeof(part), tr_(Str::RpgFoeStealFmt), r.goldStolen);
            append2(part);
        }
    }

    char log[288];
    std::snprintf(log, sizeof(log), line2[0] != '\0' ? "%s\n%s" : "%s", line1, line2);
    lv_label_set_text(g->rpgLog, log);

    // Apply the outcome to the pet and pick a sound.
    if (r.victory) {
        pet.addCoins(static_cast<std::uint32_t>(r.goldGained));
        pet.applyPlayResult(true);
        g->battlePhrase = PhraseContext::Victory;
        if (g->dungeon.active) {
            g->dungeon.goldBanked += r.goldGained;
            g->dungeon.xpBanked += r.xpGained;
        }
        if (r.levelsGained > 0) {
            g->pendingAlloc = true;
        }
        g->svc.playSfx(Sfx::Win);
        endBattle();
    } else if (r.defeat) {
        static_cast<void>(pet.spendCoins(static_cast<std::uint32_t>(
            std::min(static_cast<int>(pet.coins()), r.goldLostHint))));
        pet.applyPlayResult(false);
        g->battlePhrase = PhraseContext::Fainted;
        if (g->dungeon.active) {
            finishDungeon(false); // fainting ends the run, loot is kept
        }
        g->svc.playSfx(Sfx::Lose);
        endBattle();
    } else if (r.escaped) {
        if (g->dungeon.active) {
            finishDungeon(false); // running away ends the run
        }
        g->svc.playSfx(Sfx::Tap);
        endBattle();
    } else if (r.mpRestored > 0) {
        g->svc.playSfx(Sfx::Magic);
    } else if (r.foeDmg > 0) {
        g->svc.playSfx(Sfx::Hurt);
    } else if (r.skillUsed) {
        g->svc.playSfx(Sfx::Magic);
    } else if (r.playerCrit) {
        g->svc.playSfx(Sfx::Crit);
    } else if (r.playerDmg > 0) {
        g->svc.playSfx(Sfx::Hit);
    } else {
        g->svc.playSfx(Sfx::Talk);
    }
    refreshBattleHud();
}

// ---- dungeon: rest between rooms ----------------------------------------------

void showRestPanel()
{
    game::VaiwuPet& pet = *g->svc.pet;
    game::RpgProgress& p = pet.rpg();
    const int hpBefore = p.hp;
    const int mpBefore = p.mp;
    game::restBetweenRooms(pet.character(), p);
    char text[128];
    char rest[64];
    std::snprintf(rest, sizeof(rest), tr_(Str::DungeonRestFmt),
        p.hp - hpBefore, p.mp - mpBefore);
    std::snprintf(text, sizeof(text), tr_(Str::DungeonRoomFmt),
        g->dungeon.room + 1, g->dungeon.rooms);
    std::strncat(text, "\n", sizeof(text) - std::strlen(text) - 1);
    std::strncat(text, rest, sizeof(text) - std::strlen(text) - 1);
    lv_label_set_text(g->restLabel, text);
    lv_obj_remove_flag(g->restPanel, LV_OBJ_FLAG_HIDDEN);
    saveNow();
}

// The run is over: pay the chest if it was cleared, then go back home.
void finishDungeon(bool cleared)
{
    game::VaiwuPet& pet = *g->svc.pet;
    if (cleared) {
        const int gold = game::dungeonChestGold(g->dungeon,
            static_cast<std::uint32_t>(std::rand()) ^ nextSalt());
        pet.addCoins(static_cast<std::uint32_t>(gold));
        char text[160];
        char chest[96];
        std::snprintf(text, sizeof(text), tr_(Str::DungeonClearedFmt), g->dungeon.rooms);
        std::snprintf(chest, sizeof(chest), tr_(Str::DungeonChestFmt), gold);
        std::strncat(text, "\n", sizeof(text) - std::strlen(text) - 1);
        std::strncat(text, chest, sizeof(text) - std::strlen(text) - 1);
        showBubble(text, 8);
        g->svc.playSfx(Sfx::Win);
    }
    g->dungeon = game::DungeonRun{};
    g->resumeDungeon = false;
    saveNow();
    refreshStats();
}

// After a won room: the boss ends the run (chest), anything else offers a
// breather. Called both right after the fight and after the level-up
// screen - a level-up must never silently abandon the run.
void advanceDungeonAfterVictory()
{
    if (!g->dungeon.active) {
        return;
    }
    if (game::isBossRoom(g->dungeon)) {
        finishDungeon(true);
    } else {
        showRestPanel();
    }
}

void onRestBtn(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    lv_obj_add_flag(g->restPanel, LV_OBJ_FLAG_HIDDEN);
    g->svc.playSfx(Sfx::Tap);
    if (index != 0) { // go home, keeping everything earned so far
        finishDungeon(false);
        showPetPhrase(PhraseContext::Victory);
        return;
    }
    ++g->dungeon.room;
    enterDungeonRoom();
}

// ---- random encounters -------------------------------------------------------

// Rolled once per simulated minute from slowTick: a monster wanders close
// to the house. The player always chooses - nothing starts a fight behind
// their back.
void maybeEncounter()
{
    game::VaiwuPet& pet = *g->svc.pet;
    if (g->encounterCooldown > 0) {
        --g->encounterCooldown;
        return;
    }
    if (!pet.alive() || g->game != GameKind::None || g->dungeon.active
        || pet.life() == game::LifeState::Sleeping
        || lv_screen_active() != g->mainScreen) {
        return;
    }
    // Only offer a fight she could plausibly take: above half HP and rested.
    game::RpgProgress& p = pet.rpg();
    if (p.hp * 2 < game::hpMaxFor(pet.character(), p) || pet.stats().energy < 25.0f) {
        return;
    }
    if (std::rand() % 100 >= 4) { // ~4% per second while idle at home
        return;
    }
    g->encounterCooldown = 240; // at most one prompt every 4 minutes
    g->encounterFoe = game::spawnFoe(p.level,
        static_cast<std::uint32_t>(std::rand()) * 2246822519U ^ nextSalt());
    char text[96];
    std::snprintf(text, sizeof(text), tr_(Str::EncounterFmt),
        tr_(monsterNameId(g->encounterFoe.kind)));
    lv_label_set_text(g->encounterLabel, text);
    lv_obj_remove_flag(g->encounterPanel, LV_OBJ_FLAG_HIDDEN);
    g->svc.playSfx(Sfx::Encounter);
}

void onEncounterBtn(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    lv_obj_add_flag(g->encounterPanel, LV_OBJ_FLAG_HIDDEN);
    g->svc.playSfx(Sfx::Tap);
    if (index != 0) {
        return; // ignored: she stays inside
    }
    if (g->svc.pet->startPlay() != game::ActionResult::Ok) {
        showBubble(tr_(Str::TooTired));
        return;
    }
    g->svc.playSfx(Sfx::PlayStart);
    startAdventure(&g->encounterFoe);
    refreshStats();
}

// ---- level-up stat allocation ------------------------------------------------

void refreshStatAlloc()
{
    game::VaiwuPet& pet = *g->svc.pet;
    game::RpgProgress& p = pet.rpg();
    const game::RpgStats total = game::totalStats(pet.character(), p);
    const int values[5] = {total.str, total.vit, total.agi, total.mag, total.luck};
    static const Str kNames[5] = {Str::StatStr, Str::StatVit, Str::StatAgi, Str::StatMag, Str::StatLuck};
    for (int i = 0; i < 5; ++i) {
        char row[48];
        std::snprintf(row, sizeof(row), "%s: %d", tr_(kNames[i]), values[i]);
        lv_label_set_text(g->statRowLabels[i], row);
    }
    char text[48];
    std::snprintf(text, sizeof(text), tr_(Str::RpgLevelUpTitleFmt), p.level);
    lv_label_set_text(g->statTitle, text);
    std::snprintf(text, sizeof(text), tr_(Str::RpgPointsFmt), p.statPoints);
    lv_label_set_text(g->statPoints, text);
}

void onStatPlus(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    game::VaiwuPet& pet = *g->svc.pet;
    game::RpgProgress& p = pet.rpg();
    if (p.statPoints <= 0) {
        return;
    }
    --p.statPoints;
    switch (index) {
    case 0: ++p.alloc.str; break;
    case 1: ++p.alloc.vit; break;
    case 2: ++p.alloc.agi; break;
    case 3: ++p.alloc.mag; break;
    case 4:
    default: ++p.alloc.luck; break;
    }
    game::normalizeRpg(pet.character(), p);
    g->svc.playSfx(Sfx::Tap);
    refreshStatAlloc();
}

void onStatDone(lv_event_t* e)
{
    static_cast<void>(e);
    // Unspent points are kept for later (the screen reopens on next level-up).
    lv_obj_add_flag(g->statPanel, LV_OBJ_FLAG_HIDDEN);
    g->svc.playSfx(Sfx::Win);
    saveNow();
    refreshStats();
    if (g->resumeDungeon) { // the level-up interrupted a dungeon run
        g->resumeDungeon = false;
        advanceDungeonAfterVictory();
    }
}

void onDialogBtn(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    game::VaiwuPet& pet = *g->svc.pet;
    g->svc.playSfx(Sfx::Tap);

    switch (index) {
    case 0: // HUG
        pet.giveAffection(3.0f);
        requestChat("hugged you", PhraseContext::Hugged);
        break;
    case 1: // PAT
        pet.giveAffection(2.0f);
        requestChat("patted your head", PhraseContext::Patted);
        break;
    case 2: // LEAVE (treats moved to the FEED food menu)
    default:
        lv_obj_add_flag(g->dialogPanel, LV_OBJ_FLAG_HIDDEN);
        hideBubble();
        saveNow();
        refreshStats();
        return;
    }
    lv_label_set_text(g->dialogLabel, tr_(Str::Dots));
    refreshStats();
    saveNow();
}

void onSpriteTap(lv_event_t* e)
{
    static_cast<void>(e);
    game::VaiwuPet& pet = *g->svc.pet;
    if (!pet.alive() || g->game != GameKind::None) {
        return;
    }
    if (pet.wake()) {
        // Woken up early: a sleepy, slightly grumpy reaction.
        g->svc.playSfx(Sfx::Talk);
        showPetPhrase(PhraseContext::Woken);
        saveNow();
    } else {
        g->svc.playSfx(Sfx::Tap);
        pet.giveAffection(1.0f);
        showPetPhrase(PhraseContext::Poked);
    }
    refreshStats();
}

void onSelectPage(lv_event_t* e)
{
    const int delta = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    g->selectedChar = (g->selectedChar + delta + kCharCount) % kCharCount;
    g->svc.playSfx(Sfx::Tap);
    updateSelectView();
}

void onStartBtn(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Win);
    const char* name = lv_textarea_get_text(g->nameArea);
    g->svc.pet->startNew(static_cast<CharacterId>(g->selectedChar),
        name != nullptr ? name : "", g->svc.nowMs());
    saveNow();
    showMainScreen();
}

void onRestartBtn(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    static_cast<void>(g->svc.storage->writeText("pet", ""));
    lv_obj_add_flag(g->deadPanel, LV_OBJ_FLAG_HIDDEN);
    g->wasDead = false;
    lv_textarea_set_text(g->nameArea, "");
    lv_screen_load(g->selectScreen);
}

void onNameFocus(lv_event_t* e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        // Lift the name field above the keyboard so what's typed stays visible,
        // and hide the portrait/arrows while the keyboard is up.
        lv_obj_set_pos(g->nameArea, 56, 84);
        lv_obj_set_pos(g->nameCaption, 8, 90);
        lv_obj_add_flag(g->selectPortrait, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->selectPrevBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->selectNextBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->selectName, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->selectLikes, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(g->keyboard, g->nameArea);
        lv_obj_remove_flag(g->keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->nameLangBtn, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(g->keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->nameLangBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(g->nameArea, 56, 200);
        lv_obj_set_pos(g->nameCaption, 8, 206);
        lv_obj_remove_flag(g->selectPortrait, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->selectPrevBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->selectNextBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->selectName, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->selectLikes, LV_OBJ_FLAG_HIDDEN);
    }
}

// -- chat / assistant --

// Russian YCUKEN on-screen layout (LVGL USER_1 = lowercase, USER_2 = upper).
// Cyrillic glyphs come from the generated vaiwu fonts. Control buttons:
// "1#" -> Latin digits/symbols, shift toggles case, "ENG" -> back to Latin.
static const char* const kRuKbLower[] = {
    "1#", "\u0439", "\u0446", "\u0443", "\u043A", "\u0435", "\u043D", "\u0433", "\u0448", "\u0449", "\u0437", "\u0445", LV_SYMBOL_BACKSPACE, "\n",
    "\u0410\u0411\u0412", "\u0444", "\u044B", "\u0432", "\u0430", "\u043F", "\u0440", "\u043E", "\u043B", "\u0434", "\u0436", "\u044D", LV_SYMBOL_NEW_LINE, "\n",
    "ENG", "\u044F", "\u0447", "\u0441", "\u043C", "\u0438", "\u0442", "\u044C", "\u0431", "\u044E", "\u0451", "\u044A", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};
static const char* const kRuKbUpper[] = {
    "1#", "\u0419", "\u0426", "\u0423", "\u041A", "\u0415", "\u041D", "\u0413", "\u0428", "\u0429", "\u0417", "\u0425", LV_SYMBOL_BACKSPACE, "\n",
    "\u0430\u0431\u0432", "\u0424", "\u042B", "\u0412", "\u0410", "\u041F", "\u0420", "\u041E", "\u041B", "\u0414", "\u0416", "\u042D", LV_SYMBOL_NEW_LINE, "\n",
    "ENG", "\u042F", "\u0427", "\u0421", "\u041C", "\u0418", "\u0422", "\u042C", "\u0411", "\u042E", "\u0401", "\u042A", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};
// C++ needs explicit enum casts (LVGL's own C maps rely on implicit int->enum).
#define KB_W(w) static_cast<lv_buttonmatrix_ctrl_t>(w)
#define KB_CTL(w) static_cast<lv_buttonmatrix_ctrl_t>(LV_KEYBOARD_CTRL_BUTTON_FLAGS | (w))
static const lv_buttonmatrix_ctrl_t kRuKbCtrl[] = {
    KB_CTL(3), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_CTL(3),
    KB_CTL(3), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_CTL(3),
    KB_CTL(3), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2), KB_W(2),
    KB_CTL(2), KB_W(2), KB_W(6), KB_W(2), KB_CTL(2)
};
#undef KB_W
#undef KB_CTL

// Custom keyboard handler: intercept the Cyrillic shift ("АБВ"/"абв") and the
// "ENG" button (LVGL only knows its own Latin mode buttons), delegate the rest
// to the stock behavior.
// Generic handler shared by every RU-capable keyboard. The keyboard's
// user_data points at its language button label so the label can follow the
// Cyrillic shift / ENG control keys. Non-toggle keys fall through to LVGL.
void onKbEvent(lv_event_t* e)
{
    lv_obj_t* kb = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const std::uint32_t id = lv_buttonmatrix_get_selected_button(kb);
    const char* txt = id == LV_BUTTONMATRIX_BUTTON_NONE ? nullptr
                                                        : lv_buttonmatrix_get_button_text(kb, id);
    if (txt != nullptr) {
        if (std::strcmp(txt, "\u0410\u0411\u0412") == 0) { // АБВ -> upper
            lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_2);
            return;
        }
        if (std::strcmp(txt, "\u0430\u0431\u0432") == 0) { // абв -> lower
            lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
            return;
        }
        if (std::strcmp(txt, "ENG") == 0) {
            lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
            auto* label = static_cast<lv_obj_t*>(lv_obj_get_user_data(kb));
            if (label != nullptr) {
                lv_label_set_text(label, "\u0420\u0423\u0421"); // РУС
            }
            return;
        }
    }
    lv_keyboard_def_event_cb(e); // stock typing / abc / ABC / 1# / OK / backspace
}

// Generic language toggle: the button's user_data is its keyboard; state is
// read from the keyboard's current mode (no extra bookkeeping).
void onLangToggle(lv_event_t* e)
{
    auto* btn = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    auto* kb = static_cast<lv_obj_t*>(lv_obj_get_user_data(btn));
    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (kb == nullptr) {
        return;
    }
    g->svc.playSfx(Sfx::Tap);
    const lv_keyboard_mode_t mode = lv_keyboard_get_mode(kb);
    const bool ru = mode == LV_KEYBOARD_MODE_USER_1 || mode == LV_KEYBOARD_MODE_USER_2;
    if (ru) {
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
        if (label != nullptr) lv_label_set_text(label, "\u0420\u0423\u0421"); // РУС
    } else {
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
        if (label != nullptr) lv_label_set_text(label, "ENG");
    }
}

// Install the Russian YCUKEN layout + Cyrillic font on a keyboard, and point
// it at the language button label to keep in sync.
void setupRuKeyboard(lv_obj_t* kb, lv_obj_t* langLabel)
{
    lv_obj_set_style_text_font(kb, F14, 0);
    lv_keyboard_set_popovers(kb, false);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kRuKbLower, kRuKbCtrl);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_2, kRuKbUpper, kRuKbCtrl);
    lv_obj_remove_event_cb(kb, lv_keyboard_def_event_cb);
    lv_obj_add_event_cb(kb, onKbEvent, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_user_data(kb, langLabel);
}

// Switch the chat screen between voice (default) and text input. Voice mode
// shows a big hold-to-talk mic and hides the keyboard; text mode shows the
// textarea + on-screen keyboard and hides the mic (so the two never overlap).
void setChatMode(bool textMode)
{
    g->chatTextMode = textMode;
    if (textMode) {
        lv_obj_set_size(g->chatLog, 308, 60);
        lv_obj_add_flag(g->chatMicBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->chatArea, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->chatSendBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->chatKeyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g->chatLangBtn, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(g->chatKeyboard, g->chatArea);
        lv_label_set_text(g->chatModeLabel, LV_SYMBOL_AUDIO); // tap -> back to voice
        lv_obj_set_pos(g->chatModeBtn, 270, 108);
        lv_obj_set_size(g->chatModeBtn, 44, 34);
        lv_label_set_text(g->chatStatusLabel, "");
    } else {
        lv_obj_set_size(g->chatLog, 308, 118);
        lv_obj_remove_flag(g->chatMicBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->chatArea, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->chatSendBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->chatKeyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->chatLangBtn, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g->chatModeLabel, LV_SYMBOL_KEYBOARD); // tap -> text
        lv_obj_set_pos(g->chatModeBtn, 262, 168);
        lv_obj_set_size(g->chatModeBtn, 52, 64);
        lv_label_set_text(g->chatStatusLabel, tr_(Str::VoiceHint));
    }
}

void onChatModeToggle(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    setChatMode(!g->chatTextMode);
}

void appendChat(const char* who, const char* text, std::uint32_t colorHex)
{
    // lv_label with growing text; the log lives in a scrollable container.
    lv_obj_t* line = lv_label_create(g->chatLog);
    lv_label_set_long_mode(line, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(line, 296);
    lv_obj_set_style_text_font(line, F14, 0);
    lv_obj_set_style_text_color(line, lv_color_hex(colorHex), 0);
    lv_obj_set_style_pad_bottom(line, 4, 0);
    char buf[360];
    std::snprintf(buf, sizeof(buf), "%s%s", who, text);
    lv_label_set_text(line, buf);
    lv_obj_scroll_to_view(line, LV_ANIM_ON);
}

// Map the provider's English WMO condition label to the UI language.
const char* localizedCondition(const char* en)
{
    if (g->lang != Lang::En) {
        struct Pair { const char* en; const char* ru; };
        static const Pair kMap[] = {
            {"CLEAR", "\u044F\u0441\u043D\u043E"},
            {"PARTLY CLOUDY", "\u043F\u0435\u0440\u0435\u043C\u0435\u043D\u043D\u0430\u044F \u043E\u0431\u043B\u0430\u0447\u043D\u043E\u0441\u0442\u044C"},
            {"OVERCAST", "\u043F\u0430\u0441\u043C\u0443\u0440\u043D\u043E"},
            {"FOG", "\u0442\u0443\u043C\u0430\u043D"},
            {"DRIZZLE", "\u043C\u043E\u0440\u043E\u0441\u044C"},
            {"RAIN", "\u0434\u043E\u0436\u0434\u044C"},
            {"SNOW", "\u0441\u043D\u0435\u0433"},
            {"SHOWERS", "\u043B\u0438\u0432\u0435\u043D\u044C"},
            {"SNOW SHOWERS", "\u0441\u043D\u0435\u0433\u043E\u043F\u0430\u0434"},
            {"STORM", "\u0433\u0440\u043E\u0437\u0430"},
        };
        for (const auto& pair : kMap) {
            if (std::strcmp(en, pair.en) == 0) {
                return pair.ru;
            }
        }
    }
    return en;
}

void sendToAssistant(const char* query)
{
    chat::PromptContext ctx;
    fillPetState(ctx);
    ctx.assistantMode = true;
    std::snprintf(ctx.query, sizeof(ctx.query), "%s", query);
    g->awaitingAssistant = true;
    g->svc.chat->requestReply(ctx);
}

// Handle a locally-recognized assistant intent. Returns true if handled
// entirely offline (no need to call the AI).
bool handleIntent(const game::Intent& intent)
{
    char msg[96];
    switch (intent.kind) {
    case game::IntentKind::SetAlarm:
        if (g->svc.setAlarmAt && g->svc.setAlarmAt(intent.hour, intent.minute)) {
            std::snprintf(msg, sizeof(msg), tr_(Str::AlarmSetFmt), intent.hour, intent.minute);
        } else {
            std::snprintf(msg, sizeof(msg), "%s", tr_(Str::TooManyAlarms));
        }
        appendChat("", msg, kOkHex);
        g->svc.playSfx(Sfx::Win);
        return true;
    case game::IntentKind::SetReminderIn:
        if (g->svc.setReminderAfter && g->svc.setReminderAfter(intent.afterMinutes)) {
            std::snprintf(msg, sizeof(msg), tr_(Str::ReminderSetFmt), intent.afterMinutes);
        } else {
            std::snprintf(msg, sizeof(msg), "%s", tr_(Str::TooManyAlarms));
        }
        appendChat("", msg, kOkHex);
        g->svc.playSfx(Sfx::Win);
        return true;
    case game::IntentKind::AlarmMissingTime:
        appendChat("", tr_(Str::AlarmNeedTime), kDangerHex);
        return true;
    case game::IntentKind::CancelAlarms:
        if (g->svc.cancelAlarms) {
            g->svc.cancelAlarms();
        }
        appendChat("", tr_(Str::AlarmsCancelled), kOkHex);
        g->svc.playSfx(Sfx::Tap);
        return true;
    case game::IntentKind::Joke: {
        const char* joke = game::jokeText(g->lang, nextSalt());
        appendChat("", joke, kTextHex);
        g->svc.playSfx(Sfx::Talk);
        say(joke);
        return true;
    }
    case game::IntentKind::Pomodoro:
        if (g->svc.setReminderAfter && g->svc.setReminderAfter(intent.afterMinutes)) {
            appendChat("", tr_(Str::PomodoroStarted), kOkHex);
            g->svc.playSfx(Sfx::Win);
        } else {
            appendChat("", tr_(Str::TooManyAlarms), kDangerHex);
        }
        return true;
    case game::IntentKind::Convert:
        appendChat("", intent.convertResult, kTextHex);
        g->svc.playSfx(Sfx::Talk);
        say(intent.convertResult);
        return true;
    case game::IntentKind::AskWeather:
        if (g->svc.weatherRequest) {
            appendChat("", tr_(Str::WeatherFetching), kDimHex);
            g->awaitingWeather = true;
            g->svc.weatherRequest();
        } else {
            appendChat("", tr_(Str::WeatherFailed), kDangerHex);
        }
        return true;
    case game::IntentKind::AskTime: {
        const ClockReading reading = g->svc.clock != nullptr ? g->svc.clock->now() : ClockReading{};
        if (reading.synced) {
            std::snprintf(msg, sizeof(msg), tr_(Str::TimeFmt), reading.hour, reading.minute);
            appendChat("", msg, kTextHex);
            say(msg);
        } else {
            appendChat("", tr_(Str::TimeUnknown), kDimHex);
        }
        return true;
    }
    case game::IntentKind::None:
    default:
        return false;
    }
}

// Route one user utterance (typed or transcribed) through the assistant:
// local intents first, otherwise the AI.
void submitText(const char* text)
{
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    appendChat("> ", text, kDimHex);
    const game::Intent intent = game::parseIntent(text, g->lang);
    if (!handleIntent(intent)) {
        appendChat("", tr_(Str::Dots), kDimHex);
        sendToAssistant(text);
    }
}

void onChatSend(lv_event_t* e)
{
    static_cast<void>(e);
    const char* text = lv_textarea_get_text(g->chatArea);
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    g->svc.playSfx(Sfx::Tap);
    submitText(text);
    lv_textarea_set_text(g->chatArea, "");
}

void onMicPressed(lv_event_t* e)
{
    static_cast<void>(e);
    if (!g->svc.voiceStart) {
        return;
    }
    g->svc.playSfx(Sfx::Tap);
    lv_label_set_text(g->chatStatusLabel, tr_(Str::Listening));
    g->svc.voiceStart(static_cast<int>(g->lang));
}

void onMicReleased(lv_event_t* e)
{
    static_cast<void>(e);
    if (g->svc.voiceStop) {
        g->svc.voiceStop();
    }
}

void onOpenChat(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    const bool voice = g->svc.voiceAvailable && g->svc.voiceAvailable();
    if (voice) {
        // Voice input by default; the keyboard-toggle button is available.
        lv_obj_remove_flag(g->chatModeBtn, LV_OBJ_FLAG_HIDDEN);
        setChatMode(false);
    } else {
        // No STT key: text-only, hide the mode toggle entirely.
        lv_obj_add_flag(g->chatModeBtn, LV_OBJ_FLAG_HIDDEN);
        setChatMode(true);
    }
    lv_screen_load(g->chatScreen);
}

void onChatBack(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_screen_load(g->mainScreen);
}

void onAlarmDismiss(lv_event_t* e)
{
    static_cast<void>(e);
    g->alarmRingTicks = 0;
    lv_obj_add_flag(g->alarmPanel, LV_OBJ_FLAG_HIDDEN);
    g->svc.playSfx(Sfx::Tap);
}

// -- settings & wifi callbacks --

void refreshShop()
{
    char coins[24];
    std::snprintf(coins, sizeof(coins), tr_(Str::CoinsFmt), static_cast<unsigned>(g->svc.pet->coins()));
    lv_label_set_text(g->shopCoins, coins);

    const auto category = static_cast<game::ShopCategory>(g->shopCategory);
    lv_label_set_text(g->shopCatLabel, tr_(game::shopCategoryName(category)));
    int count = 0;
    const game::ShopItem* items = game::shopCategoryItems(category, count);
    for (int i = 0; i < kShopPageRows; ++i) {
        if (i >= count) {
            lv_obj_add_flag(g->shopRows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(g->shopRows[i], LV_OBJ_FLAG_HIDDEN);
        const auto& info = game::shopItem(items[i]);
        const bool owned = info.equip >= 0
            && game::ownsEquip(g->svc.pet->rpg(), static_cast<game::EquipItem>(info.equip));
        char row[64];
        if (info.equip >= 0) {
            char bonus[48];
            equipBonusText(static_cast<game::EquipItem>(info.equip), bonus, sizeof(bonus));
            std::snprintf(row, sizeof(row), owned ? "%s %s  *" : "%s %s  -  %u",
                tr_(info.nameId), bonus, static_cast<unsigned>(info.price));
        } else {
            std::snprintf(row, sizeof(row), "%s  -  %u", tr_(info.nameId),
                static_cast<unsigned>(info.price));
        }
        lv_label_set_text(g->shopRowName[i], row);
        const bool affordable = !owned && g->svc.pet->coins() >= info.price;
        lv_obj_set_style_text_color(g->shopRowName[i],
            lv_color_hex(affordable ? kTextHex : 0x6A6080), 0);
    }
}

void onShopBuy(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    int count = 0;
    const game::ShopItem* items = game::shopCategoryItems(
        static_cast<game::ShopCategory>(g->shopCategory), count);
    if (index >= count) {
        return;
    }
    if (game::buyShopItem(*g->svc.pet, items[index])) {
        g->svc.playSfx(Sfx::Feed);
        lv_label_set_text(g->shopStatus, tr_(Str::Bought));
        lv_obj_set_style_text_color(g->shopStatus, lv_color_hex(kOkHex), 0);
        saveNow();
    } else {
        const auto& info = game::shopItem(items[index]);
        const bool owned = info.equip >= 0
            && game::ownsEquip(g->svc.pet->rpg(), static_cast<game::EquipItem>(info.equip));
        g->svc.playSfx(Sfx::Lose);
        lv_label_set_text(g->shopStatus, tr_(owned ? Str::AlreadyOwned : Str::NotEnoughCoins));
        lv_obj_set_style_text_color(g->shopStatus, lv_color_hex(kDangerHex), 0);
    }
    refreshShop();
}

// Short bonus text for an equipment item, e.g. "+4 STR" / "+2 MAG +2 LCK".
void equipBonusText(game::EquipItem item, char* out, std::size_t n)
{
    static const Str kShort[5] = {Str::StatShortStr, Str::StatShortVit,
        Str::StatShortAgi, Str::StatShortMag, Str::StatShortLuck};
    const game::RpgStats& b = game::equipInfo(item).bonus;
    const int values[5] = {b.str, b.vit, b.agi, b.mag, b.luck};
    out[0] = '\0';
    for (int i = 0; i < 5; ++i) {
        if (values[i] <= 0) {
            continue;
        }
        char part[24];
        std::snprintf(part, sizeof(part), "%s+%d %s", out[0] != '\0' ? " " : "",
            values[i], tr_(kShort[i]));
        std::strncat(out, part, n - std::strlen(out) - 1);
    }
}

void refreshInventory()
{
    game::VaiwuPet& pet = *g->svc.pet;
    game::RpgProgress& p = pet.rpg();
    static const Str kSlots[3] = {Str::SlotWeapon, Str::SlotArmor, Str::SlotAccessory};
    for (int slot = 0; slot < 3; ++slot) {
        char row[96];
        const int itemIndex = p.equipped[slot];
        if (itemIndex < 0) {
            std::snprintf(row, sizeof(row), "%s: %s", tr_(kSlots[slot]), tr_(Str::EqNone));
        } else {
            const auto item = static_cast<game::EquipItem>(itemIndex);
            char bonus[48];
            equipBonusText(item, bonus, sizeof(bonus));
            std::snprintf(row, sizeof(row), "%s: %s  (%s)", tr_(kSlots[slot]),
                tr_(game::equipInfo(item).nameId), bonus);
        }
        lv_label_set_text(g->invSlotLabels[slot], row);
    }
    const game::RpgStats total = game::totalStats(pet.character(), p);
    char summary[128];
    std::snprintf(summary, sizeof(summary), "%s %d  %s %d  %s %d\n%s %d  %s %d    HP %d  MP %d",
        tr_(Str::StatShortStr), total.str, tr_(Str::StatShortVit), total.vit,
        tr_(Str::StatShortAgi), total.agi, tr_(Str::StatShortMag), total.mag,
        tr_(Str::StatShortLuck), total.luck,
        game::hpMaxFor(pet.character(), p), game::mpMaxFor(pet.character(), p));
    lv_label_set_text(g->invStats, summary);
    lv_label_set_text(g->invHint, tr_(Str::SellRow));
}

// Tap a slot row: cycle through the owned items of that slot, then "none".
void onInvSlot(lv_event_t* e)
{
    const int slot = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    game::VaiwuPet& pet = *g->svc.pet;
    game::RpgProgress& p = pet.rpg();
    int owned[game::kEquipItemCount];
    int count = 0;
    for (int i = 0; i < game::kEquipItemCount; ++i) {
        const auto item = static_cast<game::EquipItem>(i);
        if (game::ownsEquip(p, item)
            && game::equipInfo(item).slot == static_cast<game::EquipSlot>(slot)) {
            owned[count++] = i;
        }
    }
    if (count == 0) {
        return; // nothing bought for this slot yet
    }
    int pos = -1;
    for (int i = 0; i < count; ++i) {
        if (owned[i] == p.equipped[slot]) {
            pos = i;
            break;
        }
    }
    const int next = (pos < 0) ? owned[0] : (pos + 1 < count ? owned[pos + 1] : -1);
    game::setEquipped(p, static_cast<game::EquipSlot>(slot), next);
    game::normalizeRpg(pet.character(), p); // taking armor off may lower max HP
    g->svc.playSfx(Sfx::Tap);
    refreshInventory();
    saveNow();
}

// ---- detailed stats window (tap on the stat bars) ---------------------------

void refreshStatsWindow()
{
    game::VaiwuPet& pet = *g->svc.pet;
    const game::PetStats& stats = pet.stats();
    game::RpgProgress& p = pet.rpg();
    const game::CharacterId id = pet.character();
    game::normalizeRpg(id, p);
    char buf[192];
    char frag1[32];
    char frag2[32];

    std::snprintf(buf, sizeof(buf), "%s   Lv.%d", pet.name().c_str(), p.level);
    lv_label_set_text(g->statsInfoTitle, buf);

    // Left: the tamagotchi numbers behind the four bars + body/purse.
    std::snprintf(frag1, sizeof(frag1), tr_(Str::WeightFmt), static_cast<int>(stats.weightKg));
    std::snprintf(frag2, sizeof(frag2), tr_(Str::DayWordFmt),
        static_cast<unsigned>(stats.ageSeconds / 86400U + 1U));
    char coins[32];
    std::snprintf(coins, sizeof(coins), tr_(Str::CoinsFmt), static_cast<unsigned>(pet.coins()));
    std::snprintf(buf, sizeof(buf), "%s %d%%\n%s %d%%\n%s %d%%\n%s %d%%\n%s  %s\n%s",
        tr_(Str::BarFood), static_cast<int>(stats.hunger),
        tr_(Str::BarMood), static_cast<int>(stats.happiness),
        tr_(Str::BarEnergy), static_cast<int>(stats.energy),
        tr_(Str::BarDisc), static_cast<int>(stats.discipline),
        frag1, frag2, coins);
    lv_label_set_text(g->statsInfoLeft, buf);

    // Right: the RPG side.
    char potion[32];
    std::snprintf(potion, sizeof(potion), tr_(Str::RpgPotionFmt), p.potions);
    std::snprintf(buf, sizeof(buf), "HP %d/%d\nMP %d/%d\nXP %d/%d\n%s",
        p.hp, game::hpMaxFor(id, p), p.mp, game::mpMaxFor(id, p),
        p.xp, game::xpForNext(p.level), potion);
    lv_label_set_text(g->statsInfoRight, buf);

    // Bottom: effective characteristics + what is currently worn.
    const game::RpgStats total = game::totalStats(id, p);
    char gear[96];
    gear[0] = '\0';
    for (int slot = 0; slot < 3; ++slot) {
        const int itemIndex = p.equipped[slot];
        const char* name = itemIndex < 0 ? tr_(Str::EqNone)
            : tr_(game::equipInfo(static_cast<game::EquipItem>(itemIndex)).nameId);
        char part[48];
        std::snprintf(part, sizeof(part), "%s%s", slot > 0 ? " / " : "", name);
        std::strncat(gear, part, sizeof(gear) - std::strlen(gear) - 1);
    }
    std::snprintf(buf, sizeof(buf), "%s %d  %s %d  %s %d  %s %d  %s %d\n%s",
        tr_(Str::StatShortStr), total.str, tr_(Str::StatShortVit), total.vit,
        tr_(Str::StatShortAgi), total.agi, tr_(Str::StatShortMag), total.mag,
        tr_(Str::StatShortLuck), total.luck, gear);
    lv_label_set_text(g->statsInfoBottom, buf);
}

void onOpenStats(lv_event_t* e)
{
    static_cast<void>(e);
    if (!g->svc.pet->alive() || g->game != GameKind::None) {
        return;
    }
    g->svc.playSfx(Sfx::Tap);
    refreshStatsWindow();
    lv_obj_remove_flag(g->statsInfoPanel, LV_OBJ_FLAG_HIDDEN);
}

void onStatsClose(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_obj_add_flag(g->statsInfoPanel, LV_OBJ_FLAG_HIDDEN);
}

void onStatsGear(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_obj_add_flag(g->statsInfoPanel, LV_OBJ_FLAG_HIDDEN);
    g->invFromShop = false; // BACK returns to the main screen
    refreshInventory();
    lv_screen_load(g->invScreen);
}

// Long-press a slot to sell what is worn there (half the shop price).
void onInvSell(lv_event_t* e)
{
    const int slot = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    game::VaiwuPet& pet = *g->svc.pet;
    const int itemIndex = pet.rpg().equipped[slot];
    if (itemIndex < 0) {
        lv_label_set_text(g->invHint, tr_(Str::NothingToSell));
        g->svc.playSfx(Sfx::Lose);
        return;
    }
    const std::uint32_t gold =
        game::sellEquip(pet, static_cast<game::EquipItem>(itemIndex));
    if (gold == 0U) {
        lv_label_set_text(g->invHint, tr_(Str::NothingToSell));
        return;
    }
    char text[64];
    std::snprintf(text, sizeof(text), tr_(Str::SoldFmt), static_cast<int>(gold));
    lv_label_set_text(g->invHint, text);
    g->svc.playSfx(Sfx::Feed);
    refreshInventory();
    saveNow();
}

void onOpenInventory(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    g->invFromShop = true;
    refreshInventory();
    lv_screen_load(g->invScreen);
}

void onInvBack(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    if (g->invFromShop) {
        refreshShop();
        lv_screen_load(g->shopScreen);
    } else {
        refreshStats();
        lv_screen_load(g->mainScreen);
    }
}

void onShopCatPage(lv_event_t* e)
{
    const int delta = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    g->shopCategory = (g->shopCategory + delta + game::kShopCategoryCount) % game::kShopCategoryCount;
    g->svc.playSfx(Sfx::Tap);
    lv_label_set_text(g->shopStatus, "");
    refreshShop();
}

void onOpenShop(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_label_set_text(g->shopStatus, "");
    refreshShop();
    lv_screen_load(g->shopScreen);
}

void onShopBack(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    refreshStats();
    lv_screen_load(g->mainScreen);
}

void onGearBtn(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_screen_load(g->settingsScreen);
}

void onOpenHelp(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_screen_load(g->helpScreen);
}

void onHelpBack(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_screen_load(g->settingsScreen);
}

void onVolumeSlider(lv_event_t* e)
{
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const int value = static_cast<int>(lv_slider_get_value(slider));
    if (g->svc.setVolume) {
        g->svc.setVolume(value); // live, while dragging
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        char text[8];
        std::snprintf(text, sizeof(text), "%d", value);
        static_cast<void>(g->svc.storage->writeText("vol", text));
        g->svc.playSfx(Sfx::Tap); // audible sample of the new volume
    }
}

void onBrightnessSlider(lv_event_t* e)
{
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const int value = static_cast<int>(lv_slider_get_value(slider));
    g->brightnessPercent = value;
    g->dimmed = false; // dragging the slider counts as being awake
    g->idleSeconds = 0;
    if (g->svc.setBrightness) {
        g->svc.setBrightness(value); // live, while dragging
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        char text[8];
        std::snprintf(text, sizeof(text), "%d", value);
        static_cast<void>(g->svc.storage->writeText("bri", text));
    }
}

void onDimRow(lv_event_t* e)
{
    static_cast<void>(e);
    g->dimMode = (g->dimMode + 1) % 4;
    char text[8];
    std::snprintf(text, sizeof(text), "%d", g->dimMode);
    static_cast<void>(g->svc.storage->writeText("dim", text));
    g->idleSeconds = 0;
    applyTexts();
    g->svc.playSfx(Sfx::Tap);
}

void onVoiceRow(lv_event_t* e)
{
    static_cast<void>(e);
    const bool haveTts = static_cast<bool>(g->svc.speak);
    // Without a TTS URL the "speech" mode is skipped entirely.
    const int modes = haveTts ? 3 : 2;
    g->voiceMode = (g->voiceMode + 1) % modes;
    char text[8];
    std::snprintf(text, sizeof(text), "%d", g->voiceMode);
    static_cast<void>(g->svc.storage->writeText("voice", text));
    applyTexts();
    g->svc.playSfx(Sfx::Tap);
    if (g->voiceMode != 0) { // sample the new setting right away
        voicePhrase(game::offlinePhrase(g->svc.pet->character(),
            PhraseContext::Greeting, nextSalt(), g->lang));
    }
}

void onLangRow(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    g->lang = g->lang == Lang::En ? Lang::Ru : Lang::En;
    static_cast<void>(g->svc.storage->writeText("lang", g->lang == Lang::Ru ? "1" : "0"));
    applyTexts();
}

void populateWifiRows()
{
    const auto results = g->svc.wifiScanResults ? g->svc.wifiScanResults() : std::vector<WifiNetworkInfo>{};
    for (int i = 0; i < 5; ++i) {
        if (i < static_cast<int>(results.size())) {
            const auto& net = results[static_cast<std::size_t>(i)];
            std::snprintf(g->wifiSsids[i], sizeof(g->wifiSsids[i]), "%s", net.ssid.c_str());
            const int quality = net.rssiDbm >= -50 ? 100
                : net.rssiDbm <= -100 ? 0
                : (net.rssiDbm + 100) * 2;
            char text[64];
            std::snprintf(text, sizeof(text), "%s%s  %d%%",
                net.secure ? "" : "* ", net.ssid.c_str(), quality);
            lv_label_set_text(g->wifiRowTexts[i], text);
            lv_obj_remove_flag(g->wifiRows[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            g->wifiSsids[i][0] = '\0';
            lv_obj_add_flag(g->wifiRows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_label_set_text(g->wifiStatus, results.empty() ? tr_(Str::NoNetworks) : "");
}

void startWifiScan()
{
    for (int i = 0; i < 5; ++i) {
        lv_obj_add_flag(g->wifiRows[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(g->wifiStatus, tr_(Str::Scanning));
    if (g->svc.wifiRequestScan) {
        g->svc.wifiRequestScan();
    }
}

void onWifiRow(lv_event_t* e)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(lv_event_get_user_data(e)));
    if (g->wifiSsids[index][0] == '\0') {
        return;
    }
    g->svc.playSfx(Sfx::Tap);
    std::snprintf(g->targetSsid, sizeof(g->targetSsid), "%s", g->wifiSsids[index]);
    char title[80];
    std::snprintf(title, sizeof(title), tr_(Str::PasswordFor), g->targetSsid);
    lv_label_set_text(g->passTitle, title);
    lv_label_set_text(g->passStatus, "");
    lv_textarea_set_text(g->passArea, "");
    lv_keyboard_set_textarea(g->passKeyboard, g->passArea);
    lv_screen_load(g->wifiPassScreen);
}

void onWifiRescan(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    startWifiScan();
}

void onOpenWifiList(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_screen_load(g->wifiListScreen);
    startWifiScan();
}

void onWifiConnect(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    const char* password = lv_textarea_get_text(g->passArea);
    lv_label_set_text(g->passStatus, tr_(Str::Connecting));
    if (g->svc.wifiRequestConnect) {
        g->svc.wifiRequestConnect(g->targetSsid, password != nullptr ? password : "");
    }
}

void onNewGameRow(lv_event_t* e)
{
    static_cast<void>(e); // fires on LV_EVENT_LONG_PRESSED only
    g->svc.playSfx(Sfx::Lose);
    static_cast<void>(g->svc.storage->writeText("pet", ""));
    lv_textarea_set_text(g->nameArea, "");
    lv_obj_add_flag(g->deadPanel, LV_OBJ_FLAG_HIDDEN);
    g->wasDead = false;
    lv_screen_load(g->selectScreen);
}

void onSettingsBack(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_screen_load(g->mainScreen);
}

void onWifiListBack(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_screen_load(g->settingsScreen);
}

void onWifiPassBack(lv_event_t* e)
{
    static_cast<void>(e);
    g->svc.playSfx(Sfx::Tap);
    lv_screen_load(g->wifiListScreen);
    startWifiScan();
}

// ---- timers ------------------------------------------------------------------

void pollWifiState()
{
    const int state = g->svc.wifiState ? g->svc.wifiState() : 0;
    if (state == g->lastWifiState) {
        return;
    }
    g->lastWifiState = state;

    lv_obj_t* active = lv_screen_active();
    if (state == 2 && active == g->wifiListScreen) { // ScanDone
        populateWifiRows();
    } else if (active == g->wifiPassScreen) {
        if (state == 4) { // Connected
            lv_label_set_text(g->passStatus, tr_(Str::Connected));
            g->svc.playSfx(Sfx::Win);
        } else if (state == 5) { // Failed
            lv_label_set_text(g->passStatus, tr_(Str::ConnectFailed));
            g->svc.playSfx(Sfx::Lose);
        }
    }
}

void pollWeather()
{
    if (!g->awaitingWeather || !g->svc.weatherState) {
        return;
    }
    const int state = g->svc.weatherState();
    if (state == 2) { // Done
        g->awaitingWeather = false;
        const WeatherReading wx = g->svc.weatherTake();
        char msg[96];
        std::snprintf(msg, sizeof(msg), tr_(Str::WeatherFmt),
            wx.cityName.c_str(), static_cast<int>(wx.temperatureCelsius),
            localizedCondition(wx.condition.c_str()));
        appendChat("", msg, kTextHex);
        g->svc.playSfx(Sfx::Talk);
        say(msg);
    } else if (state == 3) { // Error
        g->awaitingWeather = false;
        appendChat("", tr_(Str::WeatherFailed), kDangerHex);
    }
}

void pollVoice()
{
    if (!g->svc.voiceState) {
        return;
    }
    const int state = g->svc.voiceState();
    if (state == g->lastVoiceState) {
        return;
    }
    g->lastVoiceState = state;
    // 0 Idle,1 Recording,2 Transcribing,3 Done,4 Error (VoiceService::State).
    if (state == 2) {
        lv_label_set_text(g->chatStatusLabel, tr_(Str::Transcribing));
    } else if (state == 4) {
        lv_label_set_text(g->chatStatusLabel, tr_(Str::VoiceFailed));
        g->svc.playSfx(Sfx::Lose);
    } else if (state == 3) {
        const std::string text = g->svc.voiceTake ? g->svc.voiceTake() : std::string{};
        lv_label_set_text(g->chatStatusLabel, tr_(Str::VoiceHint));
        if (!text.empty()) {
            g->svc.playSfx(Sfx::Talk);
            if (lv_screen_active() != g->chatScreen) {
                lv_screen_load(g->chatScreen);
            }
            submitText(text.c_str());
        }
    }
}

void pollAlarms()
{
    if (g->svc.dueAlarm && g->svc.dueAlarm()) {
        g->alarmRingTicks = 40; // ~4 s of ringing (fastTick re-triggers sound)
        lv_label_set_text(g->alarmTitleLabel, tr_(Str::AlarmTitle));
        lv_obj_remove_flag(g->alarmPanel, LV_OBJ_FLAG_HIDDEN);
        if (lv_screen_active() != g->mainScreen) {
            lv_screen_load(g->mainScreen);
        }
        g->svc.playSfx(Sfx::Alarm);
    }
}

void pollOta()
{
    if (g->otaPanel == nullptr || lv_obj_has_flag(g->otaPanel, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    if (!g->svc.otaState) {
        return;
    }
    const int state = g->svc.otaState();
    char msg[96];
    switch (state) {
    case 1: // Checking
        lv_label_set_text(g->otaStatus, tr_(Str::OtaChecking));
        break;
    case 2: // UpToDate
        std::snprintf(msg, sizeof(msg), tr_(Str::OtaUpToDateFmt), g->svc.otaTag ? g->svc.otaTag() : "");
        lv_label_set_text(g->otaStatus, msg);
        break;
    case 3: // Available
        std::snprintf(msg, sizeof(msg), tr_(Str::OtaAvailFmt), g->svc.otaTag ? g->svc.otaTag() : "");
        lv_label_set_text(g->otaStatus, msg);
        break;
    case 4: // Downloading
        std::snprintf(msg, sizeof(msg), tr_(Str::OtaDownloadingFmt),
            g->svc.otaProgress ? g->svc.otaProgress() : 0);
        lv_label_set_text(g->otaStatus, msg);
        lv_bar_set_value(g->otaBar, g->svc.otaProgress ? g->svc.otaProgress() : 0,
            LV_ANIM_OFF);
        break;
    case 5: // ReadyToReboot
        lv_label_set_text(g->otaStatus, tr_(Str::OtaReady));
        lv_bar_set_value(g->otaBar, 100, LV_ANIM_OFF);
        break;
    case 6: // Failed
        lv_label_set_text(g->otaStatus, tr_(Str::OtaFailed));
        break;
    default: // Idle / Disabled: keep the previous text
        break;
    }
    const bool canUpdate = (state == 3);
    const bool canReboot = (state == 5);
    if (canUpdate) {
        lv_obj_remove_flag(g->otaUpdateBtn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g->otaUpdateBtn, LV_OBJ_FLAG_HIDDEN);
    }
    if (canReboot) {
        lv_obj_remove_flag(g->otaRebootBtn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g->otaRebootBtn, LV_OBJ_FLAG_HIDDEN);
    }
}

void slowTick(lv_timer_t* timer)
{
    static_cast<void>(timer);
    pollWifiState();
    pollVoice();
    pollWeather();
    pollOta();
    pollAlarms();
    tickIdleDimming();

    game::VaiwuPet& pet = *g->svc.pet;
    if (lv_screen_active() != g->mainScreen) {
        return;
    }

    const bool wasSleeping = pet.life() == game::LifeState::Sleeping;
    pet.tick(g->svc.nowMs());

    if (pet.life() == game::LifeState::Dead && !g->wasDead) {
        g->wasDead = true;
        g->game = GameKind::None;
        g->resultTicksLeft = 0;
        g->awaitingChatReply = false;
        g->svc.playSfx(Sfx::Death);
        lv_obj_add_flag(g->dialogPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->menuPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->guessPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->heartsPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->melodyPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->rpsPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->mathPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->melodyDiffPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->foodPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->rpgPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->statPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->statsInfoPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->encounterPanel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->restPanel, LV_OBJ_FLAG_HIDDEN);
        g->dungeon = game::DungeonRun{};
        g->resumeDungeon = false;
        lv_obj_remove_flag(g->deadPanel, LV_OBJ_FLAG_HIDDEN);
        hideBubble();
        saveNow();
    }
    if (pet.isSick() && !g->wasSick) {
        g->svc.playSfx(Sfx::Sick);
        showPetPhrase(PhraseContext::Sick);
    }
    g->wasSick = pet.isSick();

    const bool sleeping = pet.life() == game::LifeState::Sleeping;
    if (sleeping && !wasSleeping) {
        showPetPhrase(PhraseContext::Sleeping);
    }
    g->wasSleeping = sleeping;

    // Chat mailbox is shared between the pet's speech bubble and the
    // assistant screen; awaitingAssistant decides where the reply goes.
    if (g->awaitingChatReply || g->awaitingAssistant) {
        if (const auto reply = g->svc.chat->takeReply()) {
            if (g->awaitingAssistant) {
                g->awaitingAssistant = false;
                appendChat("", reply->c_str(), kTextHex);
                g->svc.playSfx(Sfx::Talk);
                say(reply->c_str());
            } else {
                g->awaitingChatReply = false;
                showBubble(reply->c_str(), 12); // speaks via voicePhrase()
                lv_label_set_text(g->dialogLabel, reply->c_str());
                if (pet.alive() && g->game == GameKind::None) {
                    lv_obj_remove_flag(g->dialogPanel, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }

    if (g->bubbleSecondsLeft > 0 && --g->bubbleSecondsLeft == 0) {
        hideBubble();
    }

    if (g->saveCountdown > 0) {
        --g->saveCountdown;
    }
    if (g->saveCountdown == 0 && pet.dirty()) {
        saveNow();
    }

    refreshStats();
    refreshWifiBadge();
    refreshClock();
    maybeEncounter();
}

void fastTick(lv_timer_t* timer)
{
    static_cast<void>(timer);

    switch (g->game) {
    case GameKind::Guess:
        if (--g->guessTicksLeft <= 0) {
            finishGame(false, Str::TimeoutBanner);
        } else {
            lv_bar_set_value(g->guessBar, g->guessTicksLeft, LV_ANIM_OFF);
        }
        break;
    case GameKind::Hearts:
        if (--g->heartsTicksLeft <= 0) {
            finishGame(false, Str::TimeoutBanner);
        } else {
            lv_bar_set_value(g->heartsBar, g->heartsTicksLeft, LV_ANIM_OFF);
        }
        break;
    case GameKind::Melody:
        if (g->melodyPhase == 0) {
            // Playback: 4 ticks lit + 2 ticks gap per note.
            if (g->melodyTick % 6 == 0) {
                if (g->melodyPlayIndex >= g->melodyLen) {
                    g->melodyPhase = 1;
                    flashPad(-1, false);
                    lv_label_set_text(g->melodyTitle, tr_(Str::MelodyRepeat));
                } else {
                    const int pad = g->melodySeq[g->melodyPlayIndex++];
                    g->melodyFlashPad = pad;
                    flashPad(pad, true);
                    static const Sfx kNotes[4] = {Sfx::Note1, Sfx::Note2, Sfx::Note3, Sfx::Note4};
                    g->svc.playSfx(kNotes[pad]);
                }
            } else if (g->melodyTick % 6 == 4) {
                flashPad(-1, false);
            }
            ++g->melodyTick;
        } else if (--g->melodyInputTicksLeft <= 0) {
            finishGame(false, Str::TimeoutBanner);
        }
        break;
    case GameKind::Rps:
    case GameKind::Math:
    case GameKind::Rpg:
        break; // input-driven, no countdown
    case GameKind::None:
        if (g->resultTicksLeft > 0 && --g->resultTicksLeft == 0 && g->activeGamePanel != nullptr) {
            lv_obj_add_flag(g->activeGamePanel, LV_OBJ_FLAG_HIDDEN);
            g->activeGamePanel = nullptr;
            if (g->pendingAlloc) { // level-up: hand out the stat points first
                g->pendingAlloc = false;
                // ...then pick the run back up where it was left.
                g->resumeDungeon = g->dungeon.active
                    && g->battlePhrase == PhraseContext::Victory;
                g->battlePhrase = PhraseContext::Played;
                refreshStatAlloc();
                lv_obj_remove_flag(g->statPanel, LV_OBJ_FLAG_HIDDEN);
            } else if (g->dungeon.active
                && g->battlePhrase == PhraseContext::Victory) {
                g->battlePhrase = PhraseContext::Played;
                advanceDungeonAfterVictory();
            } else {
                showPetPhrase(g->battlePhrase);
                g->battlePhrase = PhraseContext::Played;
            }
        }
        break;
    }

    if (g->alarmRingTicks > 0) {
        --g->alarmRingTicks;
        if (g->alarmRingTicks % 10 == 0 && g->alarmRingTicks > 0) {
            g->svc.playSfx(Sfx::Alarm);
        }
    }

    if (lv_screen_active() == g->mainScreen && g->svc.pet->alive()) {
        if (++g->breathCounter >= 6) {
            g->breathCounter = 0;
        }
        lv_obj_set_y(g->sprite, 56 + (g->breathCounter < 3 ? 0 : 2));
    }
}

// ---- touch indev -----------------------------------------------------------

void indevRead(lv_indev_t* indev, lv_indev_data_t* data)
{
    static_cast<void>(indev);
    bool touched = false;
    std::int16_t x = 0;
    std::int16_t y = 0;
    if (g->svc.readTouch && g->svc.readTouch(touched, x, y)) {
        if (touched) {
            wakeScreen();
        }
        g->touchPressed = touched;
        if (touched) {
            g->touchX = x;
            g->touchY = y;
        }
    }
    data->state = g->touchPressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = g->touchX;
    data->point.y = g->touchY;
}

// ---- screen builders ----------------------------------------------------------

void buildSelectScreen()
{
    g->selectScreen = lv_obj_create(nullptr);
    mkScreenBg(g->selectScreen);

    g->selectTitle = mkLabel(g->selectScreen, F16, kTextHex, "");
    lv_obj_align(g->selectTitle, LV_ALIGN_TOP_MID, 0, 6);

    // One character shown at a time, centered, flanked by paging arrows.
    g->selectPortrait = lv_image_create(g->selectScreen);
    lv_image_set_src(g->selectPortrait, spriteFor(CharacterId::Aoi, Emotion::Idle));
    lv_obj_align(g->selectPortrait, LV_ALIGN_TOP_MID, 0, 26); // 96x128 sprite

    g->selectPrevBtn = mkButton(g->selectScreen, 6, 74, 44, 60, LV_SYMBOL_LEFT,
        &lv_font_montserrat_24, onSelectPage, -1);
    g->selectNextBtn = mkButton(g->selectScreen, 270, 74, 44, 60, LV_SYMBOL_RIGHT,
        &lv_font_montserrat_24, onSelectPage, +1);

    g->selectName = mkLabel(g->selectScreen, F16, kTextHex, "");
    lv_obj_align(g->selectName, LV_ALIGN_TOP_MID, 0, 152);
    g->selectLikes = mkLabel(g->selectScreen, F12, kDimHex, "");
    lv_obj_align(g->selectLikes, LV_ALIGN_TOP_MID, 0, 170);
    g->selectStats = mkLabel(g->selectScreen, F12, kDimHex, "");
    lv_obj_align(g->selectStats, LV_ALIGN_TOP_MID, 0, 184);

    // Name entry + Start.
    g->nameCaption = mkLabel(g->selectScreen, F14, kDimHex, "");
    lv_obj_set_pos(g->nameCaption, 8, 206);

    g->nameArea = lv_textarea_create(g->selectScreen);
    lv_textarea_set_one_line(g->nameArea, true);
    lv_textarea_set_max_length(g->nameArea, 16);
    lv_obj_set_pos(g->nameArea, 56, 200);
    lv_obj_set_size(g->nameArea, 158, 34);
    lv_obj_set_style_bg_color(g->nameArea, lv_color_hex(kPanelHex), 0);
    lv_obj_set_style_text_color(g->nameArea, lv_color_hex(kTextHex), 0);
    lv_obj_set_style_border_color(g->nameArea, lv_color_hex(kPanelBorderHex), 0);
    lv_obj_set_style_text_font(g->nameArea, F14, 0);
    lv_obj_add_event_cb(g->nameArea, onNameFocus, LV_EVENT_ALL, nullptr);

    mkButton(g->selectScreen, 220, 200, 92, 34, "", F16, onStartBtn, 0, &g->startLabel);

    // Hidden until the name field is focused; shown over the lower half.
    g->nameHint = mkLabel(g->selectScreen, F12, kDimHex, "");
    lv_obj_add_flag(g->nameHint, LV_OBJ_FLAG_HIDDEN);

    g->keyboard = lv_keyboard_create(g->selectScreen);
    lv_obj_set_size(g->keyboard, 320, 120);
    lv_obj_align(g->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);

    // RU/EN toggle for the name keyboard (top-right; shown with the keyboard).
    g->nameLangBtn = mkButton(g->selectScreen, 272, 44, 40, 26, "", F12, onLangToggle, 0, &g->nameLangLabel);
    lv_label_set_text(g->nameLangLabel, "\u0420\u0423\u0421"); // РУС
    lv_obj_set_user_data(g->nameLangBtn, g->keyboard);
    setupRuKeyboard(g->keyboard, g->nameLangLabel);
    lv_keyboard_set_textarea(g->keyboard, g->nameArea);
    lv_obj_add_flag(g->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->nameLangBtn, LV_OBJ_FLAG_HIDDEN);

    updateSelectView();
}

void buildMainScreen()
{
    g->mainScreen = lv_obj_create(nullptr);
    mkScreenBg(g->mainScreen);

    // Scene backdrop (moon, hills, rug) behind the pet; created first so
    // every later widget stays above it. Sprites are RGB565A8 (transparent
    // background) so the scene shows through around the character.
    g->scene = lv_image_create(g->mainScreen);
    lv_image_set_src(g->scene, vaiwu_homes[1]); // day until the clock says otherwise
    lv_obj_set_pos(g->scene, 0, 56);

    for (int i = 0; i < 4; ++i) {
        const int col = i / 2;
        const int row = i % 2;
        const int x = 8 + col * 160;
        const int y = 26 + row * 16; // below the top status row

        g->barCaptions[i] = mkLabel(g->mainScreen, F12, kDimHex, "");
        lv_obj_set_pos(g->barCaptions[i], x, y);

        lv_obj_t* bar = lv_bar_create(g->mainScreen);
        lv_obj_set_pos(bar, x + 52, y + 2);
        lv_obj_set_size(bar, 96, 10);
        lv_bar_set_range(bar, 0, 100);
        lv_obj_set_style_bg_color(bar, lv_color_hex(kPanelHex), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(kBarColors[i]), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
        g->bars[i] = bar;
    }

    // Tapping the bar area opens the detailed stats + gear window.
    lv_obj_t* statsTap = lv_obj_create(g->mainScreen);
    lv_obj_set_pos(statsTap, 4, 24);
    lv_obj_set_size(statsTap, 312, 34);
    lv_obj_set_style_bg_opa(statsTap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(statsTap, 0, 0);
    lv_obj_remove_flag(statsTap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(statsTap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(statsTap, onOpenStats, LV_EVENT_CLICKED, nullptr);

    // Top status row: clock and Wi-Fi on the left, shop chip and gear on
    // the right (the pet info lives in the left column, above VOICE).
    g->clockLabel = mkLabel(g->mainScreen, F14, kTextHex, "--:--");
    lv_obj_set_pos(g->clockLabel, 8, 7);

    g->wifiLabel = mkLabel(g->mainScreen, &lv_font_montserrat_14, kPanelBorderHex, LV_SYMBOL_WIFI);
    lv_obj_set_pos(g->wifiLabel, 56, 7);



    // SHOP button: market-stall icon + current gold balance in one tappable
    // chip. Tapping it opens the category-paged shop.
    lv_obj_t* coinChip = lv_button_create(g->mainScreen);
    lv_obj_set_pos(coinChip, 196, 3);
    lv_obj_set_size(coinChip, 82, 26);
    lv_obj_set_style_bg_color(coinChip, lv_color_hex(kBtnHex), 0);
    lv_obj_set_style_bg_color(coinChip, lv_color_hex(kBtnPressedHex), LV_STATE_PRESSED);
    lv_obj_set_style_radius(coinChip, 8, 0);
    lv_obj_set_style_shadow_width(coinChip, 0, 0);
    lv_obj_set_style_pad_all(coinChip, 0, 0);
    lv_obj_t* shopIcon = lv_image_create(coinChip);
    lv_image_set_src(shopIcon, &vaiwu_icon_shop);
    lv_image_set_scale(shopIcon, 160); // 32 -> 20 px
    lv_obj_align(shopIcon, LV_ALIGN_LEFT_MID, -2, 0);
    lv_obj_t* coinIcon = lv_image_create(coinChip);
    lv_image_set_src(coinIcon, &vaiwu_icon_coin);
    lv_image_set_scale(coinIcon, 128); // 32 -> 16 px
    lv_obj_align(coinIcon, LV_ALIGN_LEFT_MID, 24, 0);
    g->coinLabel = mkLabel(coinChip, F14, 0xF2CD46, "0");
    lv_obj_align(g->coinLabel, LV_ALIGN_LEFT_MID, 46, 0);
    lv_obj_add_event_cb(coinChip, onOpenShop, LV_EVENT_CLICKED, nullptr);

    // Gear button (settings).
    lv_obj_t* gear = lv_button_create(g->mainScreen);
    lv_obj_set_pos(gear, 284, 3);
    lv_obj_set_size(gear, 32, 26); // a little wider = easier to hit
    lv_obj_set_style_bg_color(gear, lv_color_hex(kBtnHex), 0);
    lv_obj_set_style_bg_color(gear, lv_color_hex(kBtnPressedHex), LV_STATE_PRESSED);
    lv_obj_set_style_radius(gear, 6, 0);
    lv_obj_set_style_shadow_width(gear, 0, 0);
    lv_obj_t* gearLabel = mkLabel(gear, &lv_font_montserrat_14, kTextHex, LV_SYMBOL_SETTINGS);
    lv_obj_center(gearLabel);
    lv_obj_add_event_cb(gear, onGearBtn, LV_EVENT_CLICKED, nullptr);

    // Left column: pet info block (weight first, then name - spec; the day
    // and the sick tag go to the second line) with the voice button below it.
    g->infoLabel = mkLabel(g->mainScreen, F12, kDimHex, "");
    lv_label_set_long_mode(g->infoLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->infoLabel, 92);
    lv_obj_set_pos(g->infoLabel, 8, 58);

    // Big voice-chat button under the info block: opens the assistant
    // chat where the hold-to-talk microphone lives.
    lv_obj_t* voiceBtn = lv_button_create(g->mainScreen);
    lv_obj_set_pos(voiceBtn, 8, 100);
    lv_obj_set_size(voiceBtn, 92, 92);
    lv_obj_set_style_bg_color(voiceBtn, lv_color_hex(kHeartHex), 0);
    lv_obj_set_style_bg_color(voiceBtn, lv_color_hex(0xFF8FB5), LV_STATE_PRESSED);
    lv_obj_set_style_radius(voiceBtn, 14, 0);
    lv_obj_set_style_shadow_width(voiceBtn, 0, 0);
    lv_obj_t* voiceIcon = mkLabel(voiceBtn, F24, kTextHex, LV_SYMBOL_AUDIO);
    lv_obj_align(voiceIcon, LV_ALIGN_TOP_MID, 0, 12);
    g->voiceBtnLabel = mkLabel(voiceBtn, F14, kTextHex, "");
    lv_obj_align(g->voiceBtnLabel, LV_ALIGN_BOTTOM_MID, 0, 8);
    lv_obj_add_event_cb(voiceBtn, onOpenChat, LV_EVENT_CLICKED, nullptr);

    g->sprite = lv_image_create(g->mainScreen);
    lv_image_set_src(g->sprite, spriteFor(CharacterId::Aoi, Emotion::Idle));
    lv_obj_set_pos(g->sprite, 112, 56);
    lv_obj_add_flag(g->sprite, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g->sprite, onSpriteTap, LV_EVENT_CLICKED, nullptr);
    // Scale around the sprite's center so weight changes bulge both sides.
    lv_image_set_pivot(g->sprite, 48, 64);

    g->bubblePanel = mkPanel(g->mainScreen, 8, 148, 304, 44);
    g->bubbleLabel = mkLabel(g->bubblePanel, F12, kTextHex, "");
    lv_label_set_long_mode(g->bubbleLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->bubbleLabel, 290);
    lv_obj_align(g->bubbleLabel, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(g->bubblePanel, LV_OBJ_FLAG_HIDDEN);

    // Action bar: five equal-size buttons with language-independent icons
    // (feed / play / wash / wc / talk).
    static const lv_image_dsc_t* const kActionIcons[5] = {
        &vaiwu_icon_feed, &vaiwu_icon_play, &vaiwu_icon_wash, &vaiwu_icon_wc, &vaiwu_icon_talk};
    for (int i = 0; i < 5; ++i) {
        lv_obj_t* btn = lv_button_create(g->mainScreen);
        lv_obj_set_pos(btn, 4 + i * 64, 198);
        lv_obj_set_size(btn, 60, 38);
        lv_obj_set_style_bg_color(btn, lv_color_hex(kBtnHex), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(kBtnPressedHex), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_t* icon = lv_image_create(btn);
        lv_image_set_src(icon, kActionIcons[i]);
        lv_obj_center(icon);
        lv_obj_add_event_cb(btn, onActionBtn, LV_EVENT_CLICKED,
            reinterpret_cast<void*>(static_cast<std::intptr_t>(i)));
    }

    // Dialog overlay.
    g->dialogPanel = mkPanel(g->mainScreen, 20, 30, 280, 152);
    g->dialogLabel = mkLabel(g->dialogPanel, F14, kTextHex, "");
    lv_label_set_long_mode(g->dialogLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->dialogLabel, 260);
    lv_obj_align(g->dialogLabel, LV_ALIGN_TOP_MID, 0, 4);
    for (int i = 0; i < 3; ++i) { // hug / pat / leave (treats live in the food menu)
        mkButton(g->dialogPanel, 4 + i * 90, 104, 84, 34, "", F12, onDialogBtn, i, &g->dialogBtnLabels[i]);
    }
    lv_obj_add_flag(g->dialogPanel, LV_OBJ_FLAG_HIDDEN);

    // Game-select overlay.
    g->menuPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->menuTitle = mkLabel(g->menuPanel, F16, kTextHex, "");
    lv_obj_align(g->menuTitle, LV_ALIGN_TOP_MID, 0, 6);
    for (int i = 0; i < 8; ++i) {  // 5 games + adventure + dungeon + cancel
        const int col = i % 2;
        const int row = i / 2;
        mkButton(g->menuPanel, 20 + col * 148, 32 + row * 50, 132, 44, "", F12,
            onMenuBtn, i, &g->menuBtnLabels[i]);
        wrapButtonLabel(g->menuBtnLabels[i], 124); // long RU names fit on 2 lines
    }
    lv_obj_add_flag(g->menuPanel, LV_OBJ_FLAG_HIDDEN);

    // Food selection overlay (free dish + paid treats).
    g->foodPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->foodTitle = mkLabel(g->foodPanel, F16, kTextHex, "");
    lv_obj_align(g->foodTitle, LV_ALIGN_TOP_MID, 0, 6);
    for (int i = 0; i < 5; ++i) { // rice + 3 treats + cancel, 2 columns
        const int col = i % 2;
        const int row = i / 2;
        mkButton(g->foodPanel, 20 + col * 148, 36 + row * 62, 132, 52, "", F12,
            onFeedFood, i, &g->foodBtnLabels[i]);
        wrapButtonLabel(g->foodBtnLabels[i], 124);
    }
    lv_obj_add_flag(g->foodPanel, LV_OBJ_FLAG_HIDDEN);

    // Rock-paper-scissors overlay.
    g->rpsPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->rpsTitle = mkLabel(g->rpsPanel, F16, kTextHex, "");
    lv_obj_align(g->rpsTitle, LV_ALIGN_TOP_MID, 0, 10);
    {
        static const Str kRps[3] = {Str::RpsRock, Str::RpsPaper, Str::RpsScissors};
        for (int i = 0; i < 3; ++i) {
            mkButton(g->rpsPanel, 24 + i * 94, 96, 84, 60, "", F12, onRpsBtn, i, &g->rpsBtnLabels[i]);
            wrapButtonLabel(g->rpsBtnLabels[i], 76);
            static_cast<void>(kRps);
        }
    }
    lv_obj_add_flag(g->rpsPanel, LV_OBJ_FLAG_HIDDEN);

    // Quick-math overlay.
    g->mathPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->mathTitle = mkLabel(g->mathPanel, F16, kTextHex, "");
    lv_obj_align(g->mathTitle, LV_ALIGN_TOP_MID, 0, 8);
    g->mathQuestion = mkLabel(g->mathPanel, F24, kTextHex, "");
    lv_obj_align(g->mathQuestion, LV_ALIGN_TOP_MID, 0, 44);
    for (int i = 0; i < 4; ++i) {
        const int col = i % 2;
        const int row = i / 2;
        mkButton(g->mathPanel, 40 + col * 128, 104 + row * 58, 108, 48, "", F20,
            onMathBtn, i, &g->mathBtnLabels[i]);
    }
    lv_obj_add_flag(g->mathPanel, LV_OBJ_FLAG_HIDDEN);

    // Guess-emotion overlay.
    g->guessPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->guessTitle = mkLabel(g->guessPanel, F14, kTextHex, "");
    lv_obj_align(g->guessTitle, LV_ALIGN_TOP_MID, 0, 2);
    g->guessSprite = lv_image_create(g->guessPanel);
    lv_image_set_src(g->guessSprite, spriteFor(CharacterId::Aoi, Emotion::Happy));
    lv_obj_set_pos(g->guessSprite, 20, 44);
    for (int i = 0; i < 4; ++i) {
        const int col = i % 2;
        const int row = i / 2;
        mkButton(g->guessPanel, 148 + col * 80, 60 + row * 50, 74, 42, "", F12,
            onGuessBtn, i, &g->guessBtnLabels[i]);
    }
    g->guessBar = lv_bar_create(g->guessPanel);
    lv_obj_set_pos(g->guessBar, 20, 200);
    lv_obj_set_size(g->guessBar, 280, 12);
    lv_bar_set_range(g->guessBar, 0, kMiniGameTicks);
    lv_obj_set_style_bg_color(g->guessBar, lv_color_hex(kBgHex), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g->guessBar, lv_color_hex(kDangerHex), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g->guessBar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_add_flag(g->guessPanel, LV_OBJ_FLAG_HIDDEN);

    // Catch-hearts overlay.
    g->heartsPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->heartsTitle = mkLabel(g->heartsPanel, F14, kTextHex, "");
    lv_obj_align(g->heartsTitle, LV_ALIGN_TOP_MID, 0, 2);
    g->heartsCount = mkLabel(g->heartsPanel, F16, kTextHex, "0/5");
    lv_obj_set_pos(g->heartsCount, 272, 2);
    g->heartBtn = lv_button_create(g->heartsPanel);
    lv_obj_set_size(g->heartBtn, 56, 56);
    lv_obj_set_style_radius(g->heartBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g->heartBtn, lv_color_hex(kHeartHex), 0);
    lv_obj_set_style_bg_color(g->heartBtn, lv_color_hex(0xFF8FB5), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(g->heartBtn, 0, 0);
    lv_obj_t* heartLabel = mkLabel(g->heartBtn, F16, kTextHex, "<3");
    lv_obj_center(heartLabel);
    lv_obj_add_event_cb(g->heartBtn, onHeartBtn, LV_EVENT_CLICKED, nullptr);
    g->heartsBar = lv_bar_create(g->heartsPanel);
    lv_obj_set_pos(g->heartsBar, 20, 200);
    lv_obj_set_size(g->heartsBar, 280, 12);
    lv_bar_set_range(g->heartsBar, 0, kHeartsTicks);
    lv_obj_set_style_bg_color(g->heartsBar, lv_color_hex(kBgHex), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g->heartsBar, lv_color_hex(kHeartHex), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g->heartsBar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_add_flag(g->heartsPanel, LV_OBJ_FLAG_HIDDEN);

    // Repeat-melody overlay.
    g->melodyPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->melodyTitle = mkLabel(g->melodyPanel, F14, kTextHex, "");
    lv_obj_align(g->melodyTitle, LV_ALIGN_TOP_MID, 0, 4);
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* pad = lv_button_create(g->melodyPanel);
        lv_obj_set_pos(pad, 16 + i * 74, 80);
        lv_obj_set_size(pad, 64, 80);
        lv_obj_set_style_radius(pad, 10, 0);
        lv_obj_set_style_shadow_width(pad, 0, 0);
        lv_obj_set_style_bg_color(pad, lv_color_hex(kPadColors[i]), 0);
        lv_obj_add_event_cb(pad, onPadBtn, LV_EVENT_CLICKED,
            reinterpret_cast<void*>(static_cast<std::intptr_t>(i)));
        g->pads[i] = pad;
    }
    lv_obj_add_flag(g->melodyPanel, LV_OBJ_FLAG_HIDDEN);

    // Melody difficulty picker overlay.
    g->melodyDiffPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->melodyDiffTitle = mkLabel(g->melodyDiffPanel, F16, kTextHex, "");
    lv_obj_align(g->melodyDiffTitle, LV_ALIGN_TOP_MID, 0, 12);
    for (int i = 0; i < 4; ++i) { // easy / medium / hard / cancel
        mkButton(g->melodyDiffPanel, 60, 44 + i * 46, 200, 40, "", F12,
            onMelodyDiff, i, &g->melodyDiffBtnLabels[i]);
    }
    lv_obj_add_flag(g->melodyDiffPanel, LV_OBJ_FLAG_HIDDEN);

    // RPG battle overlay: foe on the right, bars on the left, actions below.
    // The meadow backdrops (time-of-day) serve as the outdoors scenery.
    g->rpgPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->rpgScene = lv_image_create(g->rpgPanel);
    lv_image_set_src(g->rpgScene, vaiwu_bgs[1]);
    lv_obj_set_pos(g->rpgScene, 0, 36);
    g->rpgLog = mkLabel(g->rpgPanel, F12, kTextHex, "");
    lv_label_set_long_mode(g->rpgLog, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->rpgLog, 304);
    lv_obj_set_pos(g->rpgLog, 4, 2);

    g->rpgFoeName = mkLabel(g->rpgPanel, F14, kTextHex, "");
    lv_obj_set_pos(g->rpgFoeName, 8, 44);
    g->rpgFoeBar = lv_bar_create(g->rpgPanel);
    lv_obj_set_pos(g->rpgFoeBar, 8, 66);
    lv_obj_set_size(g->rpgFoeBar, 150, 10);
    lv_obj_set_style_bg_color(g->rpgFoeBar, lv_color_hex(kBgHex), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g->rpgFoeBar, lv_color_hex(kDangerHex), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g->rpgFoeBar, LV_OPA_COVER, LV_PART_INDICATOR);
    g->rpgFoeImg = lv_image_create(g->rpgPanel);
    lv_obj_set_pos(g->rpgFoeImg, 204, 38);

    g->rpgLvlLabel = mkLabel(g->rpgPanel, F12, kDimHex, "");
    lv_obj_set_pos(g->rpgLvlLabel, 8, 126);
    g->rpgHpLabel = mkLabel(g->rpgPanel, F12, kOkHex, "");
    lv_obj_set_pos(g->rpgHpLabel, 8, 146);
    g->rpgHpBar = lv_bar_create(g->rpgPanel);
    lv_obj_set_pos(g->rpgHpBar, 96, 150);
    lv_obj_set_size(g->rpgHpBar, 110, 10);
    lv_obj_set_style_bg_color(g->rpgHpBar, lv_color_hex(kBgHex), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g->rpgHpBar, lv_color_hex(kOkHex), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g->rpgHpBar, LV_OPA_COVER, LV_PART_INDICATOR);
    g->rpgMpLabel = mkLabel(g->rpgPanel, F12, 0x6FB7FF, "");
    lv_obj_set_pos(g->rpgMpLabel, 8, 168);
    g->rpgMpBar = lv_bar_create(g->rpgPanel);
    lv_obj_set_pos(g->rpgMpBar, 96, 172);
    lv_obj_set_size(g->rpgMpBar, 110, 10);
    lv_obj_set_style_bg_color(g->rpgMpBar, lv_color_hex(kBgHex), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g->rpgMpBar, lv_color_hex(0x3D9FE0), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g->rpgMpBar, LV_OPA_COVER, LV_PART_INDICATOR);
    // attack / skill / potion / run / mana - five compact buttons in a row
    for (int i = 0; i < 5; ++i) {
        lv_obj_t* label = nullptr;
        mkButton(g->rpgPanel, 3 + i * 63, 194, 60, 38, "", F12, onRpgAction, i, &label);
        g->rpgBtnLabels[i] = label;
        wrapButtonLabel(label, 56);
    }
    lv_obj_add_flag(g->rpgPanel, LV_OBJ_FLAG_HIDDEN);

    // Dungeon breather between rooms.
    g->restPanel = mkPanel(g->mainScreen, 24, 56, 272, 128);
    g->restLabel = mkLabel(g->restPanel, F12, kTextHex, "");
    lv_label_set_long_mode(g->restLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->restLabel, 252);
    lv_obj_align(g->restLabel, LV_ALIGN_TOP_MID, 0, 6);
    mkButton(g->restPanel, 10, 70, 118, 40, "", F12, onRestBtn, 0, &g->restNextLabel);
    wrapButtonLabel(g->restNextLabel, 110);
    mkButton(g->restPanel, 136, 70, 118, 40, "", F12, onRestBtn, 1, &g->restLeaveLabel);
    wrapButtonLabel(g->restLeaveLabel, 110);
    lv_obj_add_flag(g->restPanel, LV_OBJ_FLAG_HIDDEN);

    // Random-encounter prompt (small, so the home stays visible behind).
    g->encounterPanel = mkPanel(g->mainScreen, 30, 60, 260, 120);
    g->encounterLabel = mkLabel(g->encounterPanel, F12, kTextHex, "");
    lv_label_set_long_mode(g->encounterLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->encounterLabel, 240);
    lv_obj_align(g->encounterLabel, LV_ALIGN_TOP_MID, 0, 6);
    mkButton(g->encounterPanel, 8, 66, 112, 38, "", F12, onEncounterBtn, 0,
        &g->encounterFightLabel);
    wrapButtonLabel(g->encounterFightLabel, 104);
    mkButton(g->encounterPanel, 130, 66, 112, 38, "", F12, onEncounterBtn, 1,
        &g->encounterIgnoreLabel);
    wrapButtonLabel(g->encounterIgnoreLabel, 104);
    lv_obj_add_flag(g->encounterPanel, LV_OBJ_FLAG_HIDDEN);

    // Detailed stats window (opened by tapping the stat bars).
    g->statsInfoPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->statsInfoTitle = mkLabel(g->statsInfoPanel, F16, kTextHex, "");
    lv_obj_align(g->statsInfoTitle, LV_ALIGN_TOP_MID, 0, 2);
    g->statsInfoLeft = mkLabel(g->statsInfoPanel, F12, kTextHex, "");
    lv_label_set_long_mode(g->statsInfoLeft, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->statsInfoLeft, 148);
    lv_obj_set_pos(g->statsInfoLeft, 8, 28);
    g->statsInfoRight = mkLabel(g->statsInfoPanel, F12, kTextHex, "");
    lv_label_set_long_mode(g->statsInfoRight, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->statsInfoRight, 140);
    lv_obj_set_pos(g->statsInfoRight, 168, 28);
    g->statsInfoBottom = mkLabel(g->statsInfoPanel, F12, kDimHex, "");
    lv_label_set_long_mode(g->statsInfoBottom, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->statsInfoBottom, 300);
    lv_obj_set_pos(g->statsInfoBottom, 8, 138);
    mkButton(g->statsInfoPanel, 24, 192, 128, 34, "", F12, onStatsGear, 0, &g->statsInfoGearLabel);
    mkButton(g->statsInfoPanel, 168, 192, 128, 34, "", F12, onStatsClose, 0, &g->statsInfoCloseLabel);
    lv_obj_add_flag(g->statsInfoPanel, LV_OBJ_FLAG_HIDDEN);

    // Level-up stat allocation overlay.
    g->statPanel = mkPanel(g->mainScreen, 0, 0, 320, 240);
    g->statTitle = mkLabel(g->statPanel, F16, kTextHex, "");
    lv_obj_align(g->statTitle, LV_ALIGN_TOP_MID, 0, 4);
    g->statPoints = mkLabel(g->statPanel, F14, 0xF2CD46, "");
    lv_obj_align(g->statPoints, LV_ALIGN_TOP_MID, 0, 26);
    for (int i = 0; i < 5; ++i) {
        g->statRowLabels[i] = mkLabel(g->statPanel, F14, kTextHex, "");
        lv_obj_set_pos(g->statRowLabels[i], 24, 52 + i * 30);
        mkButton(g->statPanel, 240, 48 + i * 30, 56, 26, "+", F16, onStatPlus, i);
    }
    mkButton(g->statPanel, 90, 204, 140, 30, "", F14, onStatDone, 0, &g->statDoneLabel);
    lv_obj_add_flag(g->statPanel, LV_OBJ_FLAG_HIDDEN);

    // Death overlay.
    g->deadPanel = lv_obj_create(g->mainScreen);
    lv_obj_set_pos(g->deadPanel, 0, 0);
    lv_obj_set_size(g->deadPanel, 320, 240);
    lv_obj_set_style_bg_color(g->deadPanel, lv_color_hex(0x101014), 0);
    lv_obj_set_style_bg_opa(g->deadPanel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(g->deadPanel, 0, 0);
    lv_obj_remove_flag(g->deadPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* dots = mkLabel(g->deadPanel, F24, kDimHex, ". . .");
    lv_obj_align(dots, LV_ALIGN_TOP_MID, 0, 46);
    g->deadMsg = mkLabel(g->deadPanel, F14, kDimHex, "");
    lv_obj_align(g->deadMsg, LV_ALIGN_TOP_MID, 0, 90);
    mkButton(g->deadPanel, 105, 150, 110, 42, "", F14, onRestartBtn, 0, &g->restartLabel);
    lv_obj_add_flag(g->deadPanel, LV_OBJ_FLAG_HIDDEN);

    // Alarm/reminder overlay (assistant). Sits above every other overlay.
    g->alarmPanel = lv_obj_create(g->mainScreen);
    lv_obj_set_pos(g->alarmPanel, 0, 0);
    lv_obj_set_size(g->alarmPanel, 320, 240);
    lv_obj_set_style_bg_color(g->alarmPanel, lv_color_hex(0x2A1030), 0);
    lv_obj_set_style_bg_opa(g->alarmPanel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g->alarmPanel, 0, 0);
    lv_obj_remove_flag(g->alarmPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bell = mkLabel(g->alarmPanel, F24, kHeartHex, LV_SYMBOL_BELL);
    lv_obj_align(bell, LV_ALIGN_TOP_MID, 0, 40);
    g->alarmTitleLabel = mkLabel(g->alarmPanel, F24, kTextHex, "");
    lv_obj_align(g->alarmTitleLabel, LV_ALIGN_TOP_MID, 0, 84);
    mkButton(g->alarmPanel, 110, 150, 100, 44, "", F16, onAlarmDismiss, 0, &g->alarmDismissLabel);
    lv_obj_add_flag(g->alarmPanel, LV_OBJ_FLAG_HIDDEN);
}

void buildChatScreen()
{
    g->chatScreen = lv_obj_create(nullptr);
    mkScreenBg(g->chatScreen);

    g->chatTitle = mkLabel(g->chatScreen, F16, kTextHex, "");
    lv_obj_align(g->chatTitle, LV_ALIGN_TOP_MID, 0, 6);
    mkButton(g->chatScreen, 250, 4, 66, 26, "", F12, onChatBack, 0, &g->chatBackLabel);

    g->chatStatusLabel = mkLabel(g->chatScreen, F12, kDimHex, "");
    lv_obj_set_pos(g->chatStatusLabel, 6, 30);

    // Scrollable message log (its height is set per input mode).
    g->chatLog = lv_obj_create(g->chatScreen);
    lv_obj_set_pos(g->chatLog, 6, 46);
    lv_obj_set_size(g->chatLog, 308, 118);
    lv_obj_set_style_bg_color(g->chatLog, lv_color_hex(kPanelHex), 0);
    lv_obj_set_style_bg_opa(g->chatLog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g->chatLog, lv_color_hex(kPanelBorderHex), 0);
    lv_obj_set_style_border_width(g->chatLog, 1, 0);
    lv_obj_set_style_radius(g->chatLog, 6, 0);
    lv_obj_set_style_pad_all(g->chatLog, 6, 0);
    lv_obj_set_flex_flow(g->chatLog, LV_FLEX_FLOW_COLUMN);

    // Big hold-to-talk mic button (voice mode).
    g->chatMicBtn = lv_button_create(g->chatScreen);
    lv_obj_set_pos(g->chatMicBtn, 6, 168);
    lv_obj_set_size(g->chatMicBtn, 250, 64);
    lv_obj_set_style_bg_color(g->chatMicBtn, lv_color_hex(kHeartHex), 0);
    lv_obj_set_style_bg_color(g->chatMicBtn, lv_color_hex(0xFF8FB5), LV_STATE_PRESSED);
    lv_obj_set_style_radius(g->chatMicBtn, 12, 0);
    lv_obj_set_style_shadow_width(g->chatMicBtn, 0, 0);
    g->chatMicLabel = mkLabel(g->chatMicBtn, &lv_font_montserrat_24, kTextHex, LV_SYMBOL_AUDIO);
    lv_obj_center(g->chatMicLabel);
    lv_obj_add_event_cb(g->chatMicBtn, onMicPressed, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(g->chatMicBtn, onMicReleased, LV_EVENT_RELEASED, nullptr);

    // Text-input widgets (text mode): textarea + send.
    g->chatArea = lv_textarea_create(g->chatScreen);
    lv_textarea_set_one_line(g->chatArea, true);
    lv_textarea_set_max_length(g->chatArea, 120);
    lv_obj_set_pos(g->chatArea, 6, 108);
    lv_obj_set_size(g->chatArea, 210, 34);
    lv_obj_set_style_bg_color(g->chatArea, lv_color_hex(kPanelHex), 0);
    lv_obj_set_style_text_color(g->chatArea, lv_color_hex(kTextHex), 0);
    lv_obj_set_style_border_color(g->chatArea, lv_color_hex(kPanelBorderHex), 0);
    lv_obj_set_style_text_font(g->chatArea, F14, 0);

    g->chatSendBtn = mkButton(g->chatScreen, 222, 108, 44, 34, "", F12, onChatSend, 0, &g->chatSendLabel);

    // Input-mode toggle (keyboard <-> mic). Repositioned by setChatMode().
    g->chatModeBtn = lv_button_create(g->chatScreen);
    lv_obj_set_style_bg_color(g->chatModeBtn, lv_color_hex(kBtnHex), 0);
    lv_obj_set_style_bg_color(g->chatModeBtn, lv_color_hex(kBtnPressedHex), LV_STATE_PRESSED);
    lv_obj_set_style_radius(g->chatModeBtn, 10, 0);
    lv_obj_set_style_shadow_width(g->chatModeBtn, 0, 0);
    g->chatModeLabel = mkLabel(g->chatModeBtn, &lv_font_montserrat_24, kTextHex, LV_SYMBOL_KEYBOARD);
    lv_obj_center(g->chatModeLabel);
    lv_obj_add_event_cb(g->chatModeBtn, onChatModeToggle, LV_EVENT_CLICKED, nullptr);

    // On-screen keyboard, anchored to the bottom (text mode only). Uses the
    // Cyrillic-capable vaiwu font and adds a Russian YCUKEN layout in the
    // USER_1/USER_2 slots, reachable via the language button below.
    g->chatKeyboard = lv_keyboard_create(g->chatScreen);
    lv_obj_set_size(g->chatKeyboard, 320, 94);
    lv_obj_align(g->chatKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Language toggle (RU <-> EN) for the keyboard, near the title.
    g->chatLangBtn = mkButton(g->chatScreen, 206, 4, 40, 26, "", F12, onLangToggle, 0, &g->chatLangLabel);
    lv_label_set_text(g->chatLangLabel, "\u0420\u0423\u0421"); // РУС
    lv_obj_set_user_data(g->chatLangBtn, g->chatKeyboard);
    setupRuKeyboard(g->chatKeyboard, g->chatLangLabel);
    lv_keyboard_set_textarea(g->chatKeyboard, g->chatArea);

    setChatMode(false); // default to voice
}

void onOtaRow(lv_event_t* e)
{
    static_cast<void>(e);
    lv_obj_remove_flag(g->otaPanel, LV_OBJ_FLAG_HIDDEN);
    if (g->svc.otaCheck) {
        g->svc.otaCheck();
    }
    pollOta();
}

void onOtaUpdate(lv_event_t* e)
{
    static_cast<void>(e);
    if (g->svc.otaUpdate) {
        g->svc.otaUpdate();
    }
}

void onOtaReboot(lv_event_t* e)
{
    static_cast<void>(e);
    if (g->svc.otaReboot) {
        g->svc.otaReboot();
    }
}

void onOtaClose(lv_event_t* e)
{
    static_cast<void>(e);
    lv_obj_add_flag(g->otaPanel, LV_OBJ_FLAG_HIDDEN);
}

void buildOtaPanel()
{
    g->otaPanel = mkPanel(g->settingsScreen, 16, 30, 288, 180);
    g->otaTitle = mkLabel(g->otaPanel, F14, kTextHex, "");
    lv_obj_align(g->otaTitle, LV_ALIGN_TOP_MID, 0, 4);
    g->otaStatus = mkLabel(g->otaPanel, F12, kDimHex, "");
    lv_label_set_long_mode(g->otaStatus, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->otaStatus, 262);
    lv_obj_align(g->otaStatus, LV_ALIGN_TOP_MID, 0, 30);
    g->otaBar = lv_bar_create(g->otaPanel);
    lv_obj_set_size(g->otaBar, 248, 10);
    lv_bar_set_range(g->otaBar, 0, 100);
    lv_obj_align(g->otaBar, LV_ALIGN_TOP_MID, 0, 64);
    g->otaUpdateBtn = mkButton(g->otaPanel, 10, 92, 128, 34, "", F12, onOtaUpdate, 0,
        &g->otaUpdateLabel);
    g->otaRebootBtn = mkButton(g->otaPanel, 150, 92, 128, 34, "", F12, onOtaReboot, 0,
        &g->otaRebootLabel);
    mkButton(g->otaPanel, 10, 136, 268, 34, "", F12, onOtaClose, 0, &g->otaCloseLabel);
    lv_obj_add_flag(g->otaUpdateBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->otaRebootBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g->otaPanel, LV_OBJ_FLAG_HIDDEN);
}

void buildSettingsScreen()
{
    g->settingsScreen = lv_obj_create(nullptr);
    mkScreenBg(g->settingsScreen);

    g->settingsTitle = mkLabel(g->settingsScreen, F16, kTextHex, "");
    lv_obj_align(g->settingsTitle, LV_ALIGN_TOP_MID, 0, 6);

    // Scrollable list of rows (six rows no longer fit a fixed 240 px layout);
    // BACK stays fixed at the bottom so it is always reachable.
    lv_obj_t* list = lv_obj_create(g->settingsScreen);
    lv_obj_set_pos(list, 6, 26);
    lv_obj_set_size(list, 308, 176);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 2, 0);
    lv_obj_set_style_pad_row(list, 5, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    mkButton(list, 0, 0, 288, 32, "", F12, onLangRow, 0, &g->langLabel);
    mkButton(list, 0, 0, 288, 32, "", F12, onOpenWifiList, 0, &g->wifiRowLabel);

    // Volume / brightness rows: caption on the left, live slider on the right.
    lv_obj_t* volRow = mkPanel(list, 0, 0, 288, 38);
    g->volCaption = mkLabel(volRow, F12, kDimHex, "");
    lv_obj_align(g->volCaption, LV_ALIGN_LEFT_MID, 4, 0);
    g->volSlider = mkSlider(volRow, 0, 100, readPercentSetting("vol", 70), onVolumeSlider);
    lv_obj_set_size(g->volSlider, 140, 8);
    lv_obj_align(g->volSlider, LV_ALIGN_RIGHT_MID, -12, 0);

    lv_obj_t* briRow = mkPanel(list, 0, 0, 288, 38);
    g->briCaption = mkLabel(briRow, F12, kDimHex, "");
    lv_obj_align(g->briCaption, LV_ALIGN_LEFT_MID, 4, 0);
    // Brightness never goes below 10%: a fully dark screen would lock the
    // user out of the very slider needed to fix it.
    g->briSlider = mkSlider(briRow, 10, 100, readPercentSetting("bri", 100), onBrightnessSlider);
    lv_obj_set_size(g->briSlider, 140, 8);
    lv_obj_align(g->briSlider, LV_ALIGN_RIGHT_MID, -12, 0);

    mkButton(list, 0, 0, 288, 32, "", F12, onVoiceRow, 0, &g->voiceRowLabel);
    mkButton(list, 0, 0, 288, 32, "", F12, onDimRow, 0, &g->dimRowLabel);
    mkButton(list, 0, 0, 288, 32, "", F12, onOpenHelp, 0, &g->helpRowLabel);
    mkButton(list, 0, 0, 288, 32, "", F12, onOtaRow, 0, &g->otaRowLabel);
    buildOtaPanel();

    // New game: long-press only, to avoid accidental resets.
    lv_obj_t* resetBtn = lv_button_create(list);
    lv_obj_set_size(resetBtn, 288, 32);
    lv_obj_set_style_bg_color(resetBtn, lv_color_hex(kBtnHex), 0);
    lv_obj_set_style_bg_color(resetBtn, lv_color_hex(kDangerHex), LV_STATE_PRESSED);
    lv_obj_set_style_radius(resetBtn, 6, 0);
    lv_obj_set_style_shadow_width(resetBtn, 0, 0);
    g->newGameLabel = mkLabel(resetBtn, F12, kTextHex, "");
    lv_obj_center(g->newGameLabel);
    lv_obj_add_event_cb(resetBtn, onNewGameRow, LV_EVENT_LONG_PRESSED, nullptr);

    mkButton(g->settingsScreen, 10, 206, 300, 28, "", F14, onSettingsBack, 0, &g->settingsBackLabel);
}

void buildHelpScreen()
{
    g->helpScreen = lv_obj_create(nullptr);
    mkScreenBg(g->helpScreen);

    g->helpTitle = mkLabel(g->helpScreen, F16, kTextHex, "");
    lv_obj_align(g->helpTitle, LV_ALIGN_TOP_MID, 0, 6);

    // Scrollable how-to / about text.
    lv_obj_t* box = lv_obj_create(g->helpScreen);
    lv_obj_set_pos(box, 6, 30);
    lv_obj_set_size(box, 308, 168);
    lv_obj_set_style_bg_color(box, lv_color_hex(kPanelHex), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(kPanelBorderHex), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_pad_all(box, 8, 0);
    g->helpBody = mkLabel(box, F12, kTextHex, "");
    lv_label_set_long_mode(g->helpBody, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->helpBody, 282);

    mkButton(g->helpScreen, 10, 206, 300, 28, "", F14, onHelpBack, 0, &g->helpBackLabel);
}

void buildWifiListScreen()
{
    g->wifiListScreen = lv_obj_create(nullptr);
    mkScreenBg(g->wifiListScreen);

    g->wifiListTitle = mkLabel(g->wifiListScreen, F16, kTextHex, "");
    lv_obj_align(g->wifiListTitle, LV_ALIGN_TOP_MID, 0, 6);
    g->wifiStatus = mkLabel(g->wifiListScreen, F12, kDimHex, "");
    lv_obj_align(g->wifiStatus, LV_ALIGN_TOP_MID, 0, 26);

    for (int i = 0; i < 5; ++i) {
        g->wifiRows[i] = mkButton(g->wifiListScreen, 12, 42 + i * 30, 296, 26, "", F12,
            onWifiRow, i, &g->wifiRowTexts[i]);
        lv_obj_add_flag(g->wifiRows[i], LV_OBJ_FLAG_HIDDEN);
    }

    mkButton(g->wifiListScreen, 12, 200, 140, 34, "", F14, onWifiRescan, 0, &g->wifiRescanLabel);
    mkButton(g->wifiListScreen, 168, 200, 140, 34, "", F14, onWifiListBack, 0, &g->wifiBackLabel);
}

void buildShopScreen()
{
    g->shopScreen = lv_obj_create(nullptr);
    mkScreenBg(g->shopScreen);

    g->shopTitle = mkLabel(g->shopScreen, F20, kTextHex, "");
    lv_obj_align(g->shopTitle, LV_ALIGN_TOP_MID, 0, 6);
    g->shopCoins = mkLabel(g->shopScreen, F14, 0xF2CD46, "");
    lv_obj_set_pos(g->shopCoins, 8, 10);
    g->shopStatus = mkLabel(g->shopScreen, F12, kDimHex, "");
    lv_obj_align(g->shopStatus, LV_ALIGN_TOP_RIGHT, -8, 12);

    // Category pager, styled like the character select: < FOOD > etc.
    mkButton(g->shopScreen, 8, 36, 44, 32, LV_SYMBOL_LEFT, &lv_font_montserrat_14,
        onShopCatPage, -1);
    mkButton(g->shopScreen, 268, 36, 44, 32, LV_SYMBOL_RIGHT, &lv_font_montserrat_14,
        onShopCatPage, +1);
    g->shopCatLabel = mkLabel(g->shopScreen, F16, kTextHex, "");
    lv_obj_align(g->shopCatLabel, LV_ALIGN_TOP_MID, 0, 44);

    // One category page: up to kShopPageRows item rows (extras stay hidden).
    for (int i = 0; i < kShopPageRows; ++i) {
        lv_obj_t* row = lv_obj_create(g->shopScreen);
        lv_obj_set_pos(row, 8, 70 + i * 27);
        lv_obj_set_size(row, 304, 25);
        lv_obj_set_style_bg_color(row, lv_color_hex(kPanelHex), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        g->shopRows[i] = row;

        g->shopRowName[i] = mkLabel(row, F12, kTextHex, "");
        lv_obj_align(g->shopRowName[i], LV_ALIGN_LEFT_MID, 8, 0);
        mkButton(row, 240, 2, 60, 21, "", F12, onShopBuy, i, &g->shopBuyLabels[i]);
    }

    mkButton(g->shopScreen, 10, 208, 146, 28, "", F14, onOpenInventory, 0, &g->shopInvLabel);
    mkButton(g->shopScreen, 164, 208, 146, 28, "", F14, onShopBack, 0, &g->shopBackLabel);
}

void buildInventoryScreen()
{
    g->invScreen = lv_obj_create(nullptr);
    mkScreenBg(g->invScreen);

    g->invTitle = mkLabel(g->invScreen, F16, kTextHex, "");
    lv_obj_align(g->invTitle, LV_ALIGN_TOP_MID, 0, 6);

    // One button per slot; tapping cycles through the owned items ("none"
    // closes the cycle so anything can be taken off).
    for (int slot = 0; slot < 3; ++slot) {
        g->invSlotBtns[slot] = mkButton(g->invScreen, 12, 30 + slot * 42, 296, 38, "", F12,
            onInvSlot, slot, &g->invSlotLabels[slot]);
        wrapButtonLabel(g->invSlotLabels[slot], 284);
        // Tap cycles what is worn, a long press sells it.
        lv_obj_add_event_cb(g->invSlotBtns[slot], onInvSell, LV_EVENT_LONG_PRESSED,
            reinterpret_cast<void*>(static_cast<std::intptr_t>(slot)));
    }
    g->invHint = mkLabel(g->invScreen, F12, kDimHex, "");
    lv_obj_set_pos(g->invHint, 12, 156);

    // Effective stats with the current gear.
    g->invStats = mkLabel(g->invScreen, F12, kDimHex, "");
    lv_label_set_long_mode(g->invStats, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g->invStats, 296);
    lv_obj_set_pos(g->invStats, 12, 172);

    mkButton(g->invScreen, 10, 206, 300, 28, "", F14, onInvBack, 0, &g->invBackLabel);
}

void buildWifiPassScreen()
{
    g->wifiPassScreen = lv_obj_create(nullptr);
    mkScreenBg(g->wifiPassScreen);

    g->passTitle = mkLabel(g->wifiPassScreen, F14, kTextHex, "");
    lv_obj_set_pos(g->passTitle, 8, 6);
    g->passStatus = mkLabel(g->wifiPassScreen, F12, kDimHex, "");
    lv_obj_set_pos(g->passStatus, 8, 26);

    g->passArea = lv_textarea_create(g->wifiPassScreen);
    lv_textarea_set_one_line(g->passArea, true);
    lv_textarea_set_max_length(g->passArea, 64);
    lv_obj_set_pos(g->passArea, 8, 44);
    lv_obj_set_size(g->passArea, 160, 36);
    lv_obj_set_style_bg_color(g->passArea, lv_color_hex(kPanelHex), 0);
    lv_obj_set_style_text_color(g->passArea, lv_color_hex(kTextHex), 0);
    lv_obj_set_style_border_color(g->passArea, lv_color_hex(kPanelBorderHex), 0);

    mkButton(g->wifiPassScreen, 176, 44, 88, 36, "", F12, onWifiConnect, 0, &g->passConnectLabel);
    mkButton(g->wifiPassScreen, 268, 44, 46, 36, "", F12, onWifiPassBack, 0, &g->passBackLabel);

    g->passKeyboard = lv_keyboard_create(g->wifiPassScreen);
    lv_obj_set_size(g->passKeyboard, 320, 150);
    lv_obj_align(g->passKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(g->passKeyboard, g->passArea);
}

} // namespace

VaiwuUi::VaiwuUi(Services services)
    : services_(services)
{
}

bool VaiwuUi::init(bool haveSave)
{
    if (g != nullptr) {
        services_.logger->error("VaiwuUi: init() called twice");
        return false;
    }
    g = new (std::nothrow) UiState();
    if (g == nullptr) {
        return false;
    }
    g->svc = services_;
    g->lang = services_.initialLang;
    // Persisted voice mode; default babble, and never "speech" without TTS.
    g->brightnessPercent = readPercentSetting("bri", 100);
    g->dimMode = readPercentSetting("dim", 2); // default: dim after a minute
    if (g->dimMode < 0 || g->dimMode > 3) {
        g->dimMode = 2;
    }
    g->voiceMode = readPercentSetting("voice", 1);
    if (g->voiceMode > 2 || g->voiceMode < 0) {
        g->voiceMode = 1;
    }
    if (g->voiceMode == 2 && !services_.speak) {
        g->voiceMode = 1;
    }
    std::srand(static_cast<unsigned>(g->svc.nowMs()));

    if (!lvgl_port_lock(0)) {
        services_.logger->error("VaiwuUi: failed to lock LVGL");
        return false;
    }

    buildSelectScreen();
    buildMainScreen();
    buildChatScreen();
    buildSettingsScreen();
    buildHelpScreen();
    buildWifiListScreen();
    buildWifiPassScreen();
    buildShopScreen();
    buildInventoryScreen();
    applyTexts();

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indevRead);

    g->slowTimer = lv_timer_create(slowTick, 1000, nullptr);
    g->fastTimer = lv_timer_create(fastTick, 100, nullptr);

    if (haveSave) {
        showMainScreen();
    } else {
        lv_screen_load(g->selectScreen);
    }

    lvgl_port_unlock();
    return true;
}

} // namespace vaiwu::ui

#else // !ESP_PLATFORM

namespace vaiwu::ui {

VaiwuUi::VaiwuUi(Services services)
    : services_(services)
{
}

bool VaiwuUi::init(bool haveSave)
{
    static_cast<void>(haveSave);
    if (services_.logger != nullptr) {
        services_.logger->error("VaiwuUi requires ESP-IDF/LVGL; not available on host builds");
    }
    return false;
}

} // namespace vaiwu::ui

#endif // ESP_PLATFORM

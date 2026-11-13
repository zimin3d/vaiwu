#pragma once

#include <cstdint>

namespace vaiwu::game {

enum class Lang : std::uint8_t {
    En = 0,
    Ru = 1,
};

// Every fixed UI string. Keep the enum and both tables in I18n.cpp in sync.
enum class Str : std::uint8_t {
    ChooseTitle = 0,
    NameLabel,
    NamePlaceholder,
    NameHint,
    Start,
    Likes,
    Feed, Play, Wash, Wc, Talk,
    Hug, Pat, Leave,
    GamesTitle, GameGuess, GameHearts, GameMelody, Cancel,
    GuessTitle,
    EmoHappy, EmoSad, EmoAngry, EmoSleepy,
    HeartsTitle,
    MelodyWatch, MelodyRepeat,
    WinBanner, LoseBanner, TimeoutBanner,
    TooTired, FullAlready,
    GoneMsg, Restart,
    InfoFmt,     // printf: weight kg, name, day, sick-tag (2 lines)
    SickTag,
    Settings, Language, LanguageValue, WifiNetworks, NewGame, NewGameHint, Back,
    WifiTitle, Rescan, Scanning, NoNetworks, PasswordFor /* printf: ssid */,
    Connect, Connecting, ConnectFailed, Connected,
    Dots,
    BarFood, BarMood, BarEnergy, BarDisc,
    DialogAsk, ChatTitle, ChatPlaceholder, Send,
    AlarmSetFmt /* printf: hour, minute */, ReminderSetFmt /* printf: minutes */,
    AlarmNeedTime, AlarmsCancelled, TooManyAlarms,
    TimeFmt /* printf: hour, minute */, TimeUnknown,
    AlarmTitle, Ok, OfflineAnswer,
    Listening, Transcribing, VoiceFailed, VoiceHint, VoiceBtn,
    WeatherFetching, WeatherFmt /* printf: city, temp, condition */, WeatherFailed,
    PomodoroStarted,
    CoinsFmt /* printf: count */, RewardFmt /* printf: coins */,
    ShopTitle, Buy, NotEnoughCoins, Bought,
    ShopTastyFood, ShopCake, ShopToy, ShopEnergyDrink, ShopMedicine, ShopRibbon, ShopSushi,
    ShopVitamins, ShopSuperEnergy, ShopHpPotion, ShopMpPotion,
    FoodTitle, FoodRice, FoodFree,
    GameRps, GameMath, RpsRock, RpsPaper, RpsScissors, RpsWin, RpsLose, RpsDraw,
    MathTitle,
    DifficultyTitle, DiffEasy, DiffMedium, DiffHard,
    CatFood, CatMedicine, CatGifts,
    HelpRow, HelpTitle, HelpBody,
    Volume, Brightness,
    GameAdventure,
    RpgEncounterFmt /* printf: monster, level */, MonSlime, MonBat, MonShroom,
    MonFox, MonGhost, MonGolem,
    RpgAttack, RpgSkill, RpgPotionFmt /* printf: count */, RpgRun,
    RpgYouHitFmt /* printf: dmg */, RpgCrit, RpgFoeHitFmt /* printf: dmg */,
    RpgFoeMiss, RpgNoMp, RpgNoPotion, RpgHealedFmt /* printf: hp */,
    RpgRunOk, RpgRunFail,
    RpgWinFmt /* printf: xp, gold */, RpgDefeatFmt /* printf: gold */,
    RpgLevelUpTitleFmt /* printf: level */, RpgPointsFmt /* printf: points */,
    StatStr, StatVit, StatAgi, StatMag, StatLuck,
    RpgDone,
    CatWeapons, CatGear, CatTrinkets,
    EqWoodenSword, EqSteelSword, EqMagicStaff, EqWindDagger,
    EqLeatherVest, EqIronArmor, EqLuckyCharm, EqStarAmulet,
    EqMoonBlade, EqScaleMail, EqSilkCloak, EqSageRing,
    SlotWeapon, SlotArmor, SlotAccessory,
    EqNone, AlreadyOwned, Inventory, InventoryTitle,
    StatShortStr, StatShortVit, StatShortAgi, StatShortMag, StatShortLuck,
    WeightFmt /* printf: kg */, DayWordFmt /* printf: day */,
    SkillBlast, SkillHeal, SkillDrain, SkillFocus, SkillFlurry, SkillGuard, SkillLucky,
    RpgSecondHitFmt /* printf: dmg */, RpgDrainFmt /* printf: hp */,
    RpgGuardUp, RpgSnagFmt /* printf: gold */,
    EncounterFmt /* printf: monster */, EncounterFight, EncounterIgnore,
    VoiceRow /* printf: mode */, VoiceOff, VoiceBabble, VoiceSpeech,
    DimRow /* printf: timeout */, DimOff, Dim30s, Dim1m, Dim5m,
    GameDungeon, DungeonRoomFmt /* printf: room, rooms */, DungeonBossRoom,
    DungeonRestFmt /* printf: hp, mp */, DungeonNext, DungeonLeave,
    DungeonChestFmt /* printf: gold */, DungeonClearedFmt /* printf: rooms */,
    RpgMpPotionFmt /* printf: count */, RpgMpRestoredFmt /* printf: mp */,
    SellRow, SoldFmt /* printf: gold */, NothingToSell,
    Count_,
};

/**
 * Purpose: two-language string catalog (spec change request: Russian UI).
 * Responsibilities: return the UTF-8 string for a given language + id;
 *                    Russian strings require the generated vaiwu fonts
 *                    (DejaVu with Cyrillic), see tools/gen_font.py.
 * Public API: tr().
 */
[[nodiscard]] const char* tr(Lang lang, Str id);

} // namespace vaiwu::game

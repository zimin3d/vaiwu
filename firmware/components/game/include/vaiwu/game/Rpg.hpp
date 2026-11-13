#pragma once

#include "vaiwu/game/Characters.hpp"
#include "vaiwu/game/I18n.hpp"

#include <cstdint>

namespace vaiwu::game {

/**
 * Purpose: the fantasy-RPG layer on top of the tamagotchi sim (spec: leave
 *          the gates, fight a monster, gain XP, level up, distribute stat
 *          points; every character has its own base stats).
 * Responsibilities: per-character base stats, derived combat values
 *                    (HP/MP/attack/dodge/crit), XP curve, foe generation
 *                    scaled to the pet's level, and one battle turn
 *                    (player action + foe response) with an injected RNG so
 *                    unit tests are deterministic. Pure C++, host-testable;
 *                    no LVGL/ESP.
 * Public API: baseStatsFor(), totalStats(), hpMaxFor(), mpMaxFor(),
 *             xpForNext(), normalizeRpg(), spawnFoe(), battleTurn().
 */

// The five allocatable characteristics (spec: strength, health, ...).
struct RpgStats {
    int str{0}; // strength: physical damage
    int vit{0}; // vitality: max HP
    int agi{0}; // agility: dodge chance, escape chance
    int mag{0}; // magic: max MP and skill damage
    int luck{0}; // luck: crit chance
};

// Equipment: three slots, each item gives flat stat bonuses. Bought in the
// shop (Weapons / Gear pages), owned forever, swapped on the gear screen.
enum class EquipSlot : std::uint8_t { Weapon = 0, Armor, Accessory, Count_ };
constexpr int kEquipSlotCount = static_cast<int>(EquipSlot::Count_);

enum class EquipItem : std::uint8_t {
    WoodenSword = 0, // +2 str
    SteelSword,      // +4 str
    MagicStaff,      // +3 mag
    WindDagger,      // +3 agi
    LeatherVest,     // +2 vit
    IronArmor,       // +4 vit
    LuckyCharm,      // +3 luck
    StarAmulet,      // +2 mag +2 luck
    MoonBlade,       // +3 str +2 agi
    ScaleMail,       // +3 vit +1 str
    SilkCloak,       // +2 agi +1 vit
    SageRing,        // +3 mag +1 luck
    Count_,
};
constexpr int kEquipItemCount = static_cast<int>(EquipItem::Count_);

struct EquipInfo {
    EquipSlot slot;
    Str nameId;     // localized display name
    RpgStats bonus; // added to totalStats while equipped
};

[[nodiscard]] const EquipInfo& equipInfo(EquipItem item);

// Persisted RPG progress (lives inside VaiwuPet, save format V4).
struct RpgProgress {
    int level{1};
    int xp{0};          // toward the next level
    int statPoints{0};  // unspent, +kPointsPerLevel per level-up
    RpgStats alloc{};   // player-allocated points on top of the base stats
    int hp{-1};         // current; -1 = "not initialized yet" -> max
    int mp{-1};
    int potions{1};     // HP potions carried (bought in the shop)
    int mpPotions{0};   // mana potions (restore MP mid-fight)
    int guardTurns{0};  // Guard skill: foe damage is halved while > 0
    std::uint32_t equipOwned{0};                 // bitmask by EquipItem
    int equipped[kEquipSlotCount]{-1, -1, -1};   // EquipItem index or -1
};

[[nodiscard]] bool ownsEquip(const RpgProgress& p, EquipItem item);
// Own the item and auto-equip it into its slot (shop purchase).
void grantEquip(RpgProgress& p, EquipItem item);
// Equip an owned item (or -1 to clear); wrong slot/ownership is ignored.
void setEquipped(RpgProgress& p, EquipSlot slot, int itemIndex);
// Sum of the equipped items' bonuses (already part of totalStats()).
[[nodiscard]] RpgStats equipBonus(const RpgProgress& p);

constexpr int kPointsPerLevel = 3;
constexpr int kSkillMpCost = 4;

// Every character fights in her own way (spec: personal skills). The kind
// is fixed per character - see skillFor().
enum class SkillKind : std::uint8_t {
    Blast = 0, // plain magic damage (baseline)
    Heal,      // no damage, restores HP instead
    Drain,     // damage + heals half of it back
    Focus,     // guaranteed critical (double damage)
    Flurry,    // two quick hits, each smaller
    Guard,     // small damage + halves the foe's next hit
    Lucky,     // damage scales with LUCK instead of MAGIC, may snag gold
};

[[nodiscard]] SkillKind skillFor(CharacterId id);
// Localized skill name for the battle button.
[[nodiscard]] Str skillNameFor(CharacterId id);

[[nodiscard]] const RpgStats& baseStatsFor(CharacterId id);
[[nodiscard]] RpgStats totalStats(CharacterId id, const RpgProgress& p);
[[nodiscard]] int hpMaxFor(CharacterId id, const RpgProgress& p);
[[nodiscard]] int mpMaxFor(CharacterId id, const RpgProgress& p);
[[nodiscard]] int xpForNext(int level);

// Clamp/initialize hp/mp against the maxima (call after load / level-up).
void normalizeRpg(CharacterId id, RpgProgress& p);

// Out-of-battle recovery: +2 HP and +1 MP per simulated minute.
void rpgRegenMinutes(CharacterId id, RpgProgress& p, float minutes);

// The first three roam near the gates from level 1; the fox shows up at
// pet level 3, the ghost at 5, the golem at 7 (see spawnFoe).
enum class MonsterKind : std::uint8_t { Slime = 0, Bat, Shroom, Fox, Ghost, Golem, Count_ };
constexpr int kMonsterKindCount = static_cast<int>(MonsterKind::Count_);

struct Foe {
    MonsterKind kind{MonsterKind::Slime};
    int level{1};
    int hp{1};
    int hpMax{1};
    int atk{1};
    int xpReward{0};
    int goldReward{0};
};

// A "weak monster outside the gates": level petLevel or petLevel-1.
[[nodiscard]] Foe spawnFoe(int petLevel, std::uint32_t rng);

enum class RpgAction : std::uint8_t { Attack = 0, Skill, Potion, Run, MpPotion };

// Selling gear back: half the shop price, rounded down (min 1). The item
// is removed from the bag and unequipped if it was worn.
[[nodiscard]] int equipSellPrice(int shopPrice);

// ---- dungeon runs ------------------------------------------------------------
// A run is a short chain of fights with a rest between rooms and a boss in
// the last one; the chest at the end pays a bonus. Leaving early keeps
// whatever was already earned, so a run is never a trap.
struct DungeonRun {
    bool active{false};
    int room{0};        // 0-based index of the room being fought
    int rooms{3};       // total rooms, boss included
    int goldBanked{0};  // loot collected so far (already given to the pet)
    int xpBanked{0};
};

constexpr int kDungeonMinRooms = 3;
constexpr int kDungeonMaxRooms = 5;
constexpr int kRestHpPercent = 25; // healed between rooms
constexpr int kRestMpPercent = 20;

// Rooms scale with the pet; the last one is a boss (tougher, pays double).
[[nodiscard]] DungeonRun startDungeon(int petLevel, std::uint32_t rng);
[[nodiscard]] bool isBossRoom(const DungeonRun& run);
[[nodiscard]] Foe spawnDungeonFoe(const DungeonRun& run, int petLevel, std::uint32_t rng);
// Between rooms: a breather that restores a slice of HP/MP.
void restBetweenRooms(CharacterId id, RpgProgress& p);
// Chest at the end of a cleared run (gold only; XP already came per room).
[[nodiscard]] int dungeonChestGold(const DungeonRun& run, std::uint32_t rng);

// Everything the UI needs to narrate one turn.
struct TurnResult {
    SkillKind skill{SkillKind::Blast}; // which personal skill was used
    int playerDmg{0};
    int extraDmg{0};      // Flurry: the second hit
    int guarded{0};       // Guard: turns of protection gained
    int drained{0};       // Drain/Heal: HP restored by the skill
    int mpRestored{0};    // mana potion
    int snagged{0};       // Lucky: gold snatched mid-fight
    bool playerCrit{false};
    bool playerMissed{false};
    bool skillUsed{false};
    bool noMp{false};       // skill refused: not enough MP (turn not spent)
    bool noPotion{false};   // potion refused: none left (turn not spent)
    int healed{0};
    bool escaped{false};
    bool runFailed{false};
    int foeDmg{0};
    bool foeMissed{false};
    bool victory{false};
    bool defeat{false};     // hp reached 0: pet faints, no pet death
    int xpGained{0};
    int goldGained{0};      // caller applies to pet.addCoins()
    int goldLostHint{0};    // on defeat: caller caps by actual coins
    int levelsGained{0};
};

// One full turn: the player's action, then (if the fight goes on) the foe's
// counterattack. Mutates progress (hp/mp/potions/xp/level/statPoints) and
// the foe. rng seeds all rolls for this turn.
TurnResult battleTurn(CharacterId id, RpgProgress& p, Foe& foe, RpgAction action, std::uint32_t rng);

} // namespace vaiwu::game

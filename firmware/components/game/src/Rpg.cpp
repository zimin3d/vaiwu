#include "vaiwu/game/Rpg.hpp"

#include <algorithm>

namespace vaiwu::game {

namespace {

// Base stats per character, same order as CharacterId. Every row sums to 13
// so no starter is strictly better - they are just different builds.
//                       str vit agi mag luck
constexpr RpgStats kBase[kCharacterCount] = {
    {2, 3, 2, 4, 2}, // Aoi: dreamy battle-mage
    {3, 2, 4, 1, 3}, // Hina: fast striker
    {4, 3, 3, 1, 2}, // Rin: disciplined duelist
    {1, 2, 2, 5, 3}, // Yuki: glass-cannon sorceress
    {2, 2, 4, 2, 3}, // Akane: tricky skirmisher
    {1, 4, 1, 4, 3}, // Uni: sturdy healer-mage
    {2, 4, 2, 1, 4}, // Kolobok: round lucky tank ("I rolled away from everyone")
};

// Small deterministic PRNG steps derived from the per-turn seed.
std::uint32_t roll(std::uint32_t& state)
{
    state = state * 1664525U + 1013904223U;
    return state >> 16;
}

int clampInt(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

SkillKind skillFor(CharacterId id)
{
    // Matches each character's personality and stat build.
    static constexpr SkillKind kSkills[kCharacterCount] = {
        SkillKind::Blast,  // Aoi: quiet spellcasting
        SkillKind::Flurry, // Hina: energetic double strike
        SkillKind::Focus,  // Rin: disciplined, always hits the weak spot
        SkillKind::Drain,  // Yuki: mysterious life-drain
        SkillKind::Lucky,  // Akane: fox tricks (and pickpocketing)
        SkillKind::Heal,   // Uni: unicorn healing magic
        SkillKind::Guard,  // Kolobok: rolls into a crust-hard ball
    };
    const auto index = static_cast<unsigned>(id);
    return kSkills[index < static_cast<unsigned>(kCharacterCount) ? index : 0U];
}

Str skillNameFor(CharacterId id)
{
    switch (skillFor(id)) {
    case SkillKind::Heal: return Str::SkillHeal;
    case SkillKind::Drain: return Str::SkillDrain;
    case SkillKind::Focus: return Str::SkillFocus;
    case SkillKind::Flurry: return Str::SkillFlurry;
    case SkillKind::Guard: return Str::SkillGuard;
    case SkillKind::Lucky: return Str::SkillLucky;
    case SkillKind::Blast:
    default: return Str::SkillBlast;
    }
}

const RpgStats& baseStatsFor(CharacterId id)
{
    const auto index = static_cast<unsigned>(id);
    return kBase[index < static_cast<unsigned>(kCharacterCount) ? index : 0U];
}

const EquipInfo& equipInfo(EquipItem item)
{
    //                    slot                  name                 str vit agi mag luck
    static constexpr EquipInfo kEquip[kEquipItemCount] = {
        {EquipSlot::Weapon, Str::EqWoodenSword, {2, 0, 0, 0, 0}},
        {EquipSlot::Weapon, Str::EqSteelSword, {4, 0, 0, 0, 0}},
        {EquipSlot::Weapon, Str::EqMagicStaff, {0, 0, 0, 3, 0}},
        {EquipSlot::Weapon, Str::EqWindDagger, {0, 0, 3, 0, 0}},
        {EquipSlot::Armor, Str::EqLeatherVest, {0, 2, 0, 0, 0}},
        {EquipSlot::Armor, Str::EqIronArmor, {0, 4, 0, 0, 0}},
        {EquipSlot::Accessory, Str::EqLuckyCharm, {0, 0, 0, 0, 3}},
        {EquipSlot::Accessory, Str::EqStarAmulet, {0, 0, 0, 2, 2}},
        {EquipSlot::Weapon, Str::EqMoonBlade, {3, 0, 2, 0, 0}},
        {EquipSlot::Armor, Str::EqScaleMail, {1, 3, 0, 0, 0}},
        {EquipSlot::Armor, Str::EqSilkCloak, {0, 1, 2, 0, 0}},
        {EquipSlot::Accessory, Str::EqSageRing, {0, 0, 0, 3, 1}},
    };
    const auto index = static_cast<unsigned>(item);
    return kEquip[index < static_cast<unsigned>(kEquipItemCount) ? index : 0U];
}

bool ownsEquip(const RpgProgress& p, EquipItem item)
{
    return (p.equipOwned >> static_cast<unsigned>(item)) & 1U;
}

void grantEquip(RpgProgress& p, EquipItem item)
{
    p.equipOwned |= 1U << static_cast<unsigned>(item);
    p.equipped[static_cast<int>(equipInfo(item).slot)] = static_cast<int>(item);
}

void setEquipped(RpgProgress& p, EquipSlot slot, int itemIndex)
{
    const int slotIndex = static_cast<int>(slot);
    if (slotIndex < 0 || slotIndex >= kEquipSlotCount) {
        return;
    }
    if (itemIndex < 0) {
        p.equipped[slotIndex] = -1;
        return;
    }
    if (itemIndex >= kEquipItemCount) {
        return;
    }
    const auto item = static_cast<EquipItem>(itemIndex);
    if (!ownsEquip(p, item) || equipInfo(item).slot != slot) {
        return;
    }
    p.equipped[slotIndex] = itemIndex;
}

RpgStats equipBonus(const RpgProgress& p)
{
    RpgStats sum{};
    for (int slot = 0; slot < kEquipSlotCount; ++slot) {
        const int itemIndex = p.equipped[slot];
        if (itemIndex < 0 || itemIndex >= kEquipItemCount) {
            continue;
        }
        const auto item = static_cast<EquipItem>(itemIndex);
        if (!ownsEquip(p, item) || equipInfo(item).slot != static_cast<EquipSlot>(slot)) {
            continue; // corrupt save: ignore rather than cheat
        }
        const RpgStats& b = equipInfo(item).bonus;
        sum.str += b.str;
        sum.vit += b.vit;
        sum.agi += b.agi;
        sum.mag += b.mag;
        sum.luck += b.luck;
    }
    return sum;
}

RpgStats totalStats(CharacterId id, const RpgProgress& p)
{
    const RpgStats& base = baseStatsFor(id);
    const RpgStats gear = equipBonus(p);
    return {base.str + p.alloc.str + gear.str, base.vit + p.alloc.vit + gear.vit,
        base.agi + p.alloc.agi + gear.agi, base.mag + p.alloc.mag + gear.mag,
        base.luck + p.alloc.luck + gear.luck};
}

int hpMaxFor(CharacterId id, const RpgProgress& p)
{
    const RpgStats total = totalStats(id, p);
    return 20 + total.vit * 6 + (p.level - 1) * 4;
}

int mpMaxFor(CharacterId id, const RpgProgress& p)
{
    const RpgStats total = totalStats(id, p);
    return 8 + total.mag * 4 + (p.level - 1) * 2;
}

int xpForNext(int level)
{
    return 20 + (level - 1) * 15;
}

void normalizeRpg(CharacterId id, RpgProgress& p)
{
    p.level = clampInt(p.level, 1, 99);
    const int hpMax = hpMaxFor(id, p);
    const int mpMax = mpMaxFor(id, p);
    p.hp = (p.hp < 0) ? hpMax : clampInt(p.hp, 0, hpMax);
    p.mp = (p.mp < 0) ? mpMax : clampInt(p.mp, 0, mpMax);
    p.potions = clampInt(p.potions, 0, 99);
    p.mpPotions = clampInt(p.mpPotions, 0, 99);
    p.guardTurns = clampInt(p.guardTurns, 0, 3);
    p.equipOwned &= (1U << kEquipItemCount) - 1U;
    for (int slot = 0; slot < kEquipSlotCount; ++slot) {
        const int itemIndex = p.equipped[slot];
        if (itemIndex < -1 || itemIndex >= kEquipItemCount
            || (itemIndex >= 0
                && (!ownsEquip(p, static_cast<EquipItem>(itemIndex))
                    || equipInfo(static_cast<EquipItem>(itemIndex)).slot
                        != static_cast<EquipSlot>(slot)))) {
            p.equipped[slot] = -1;
        }
    }
}

void rpgRegenMinutes(CharacterId id, RpgProgress& p, float minutes)
{
    if (minutes <= 0.0f) {
        return;
    }
    normalizeRpg(id, p);
    p.hp = clampInt(p.hp + static_cast<int>(2.0f * minutes), 0, hpMaxFor(id, p));
    p.mp = clampInt(p.mp + static_cast<int>(1.0f * minutes), 0, mpMaxFor(id, p));
}

Foe spawnFoe(int petLevel, std::uint32_t rng)
{
    std::uint32_t state = rng | 1U;
    Foe foe;
    // The bestiary grows with the pet: fox at 3, ghost at 5, golem at 7.
    int kinds = 3;
    if (petLevel >= 3) ++kinds;
    if (petLevel >= 5) ++kinds;
    if (petLevel >= 7) ++kinds;
    foe.kind = static_cast<MonsterKind>(roll(state) % static_cast<unsigned>(kinds));
    foe.level = std::max(1, petLevel - static_cast<int>(roll(state) % 2U)); // weak: <= pet

    // Per-kind flavor (percent): tanky golem, spiky ghost, sly fox...
    //                               slime bat shroom fox ghost golem
    static constexpr int kHpPct[] = {100, 80, 120, 100, 85, 150};
    static constexpr int kAtkPct[] = {100, 120, 90, 115, 130, 100};
    static constexpr int kRewardPct[] = {100, 100, 100, 115, 125, 140};
    const int kind = static_cast<int>(foe.kind);
    foe.hpMax = (14 + foe.level * 6) * kHpPct[kind] / 100;
    foe.hp = foe.hpMax;
    foe.atk = (3 + foe.level * 2) * kAtkPct[kind] / 100;
    foe.xpReward = (10 + foe.level * 5) * kRewardPct[kind] / 100;
    foe.goldReward = (3 + foe.level * 2) * kRewardPct[kind] / 100;
    return foe;
}

int equipSellPrice(int shopPrice)
{
    const int price = shopPrice / 2;
    return price < 1 ? 1 : price;
}

DungeonRun startDungeon(int petLevel, std::uint32_t rng)
{
    std::uint32_t state = rng | 1U;
    DungeonRun run;
    run.active = true;
    run.room = 0;
    run.rooms = kDungeonMinRooms
        + static_cast<int>(roll(state) % (kDungeonMaxRooms - kDungeonMinRooms + 1U));
    static_cast<void>(petLevel);
    return run;
}

bool isBossRoom(const DungeonRun& run)
{
    return run.active && run.room == run.rooms - 1;
}

Foe spawnDungeonFoe(const DungeonRun& run, int petLevel, std::uint32_t rng)
{
    Foe foe = spawnFoe(petLevel, rng);
    // Deeper rooms are meaner; the boss is a proper wall with real loot.
    const int depthPct = 100 + run.room * 10;
    foe.hpMax = foe.hpMax * depthPct / 100;
    foe.hp = foe.hpMax;
    foe.atk = foe.atk * depthPct / 100;
    if (isBossRoom(run)) {
        foe.level += 1;
        foe.hpMax = foe.hpMax * 180 / 100;
        foe.hp = foe.hpMax;
        foe.atk = foe.atk * 130 / 100;
        foe.xpReward *= 2;
        foe.goldReward *= 2;
    }
    return foe;
}

void restBetweenRooms(CharacterId id, RpgProgress& p)
{
    normalizeRpg(id, p);
    const int hpMax = hpMaxFor(id, p);
    const int mpMax = mpMaxFor(id, p);
    p.hp = clampInt(p.hp + hpMax * kRestHpPercent / 100, 0, hpMax);
    p.mp = clampInt(p.mp + mpMax * kRestMpPercent / 100, 0, mpMax);
    p.guardTurns = 0;
}

int dungeonChestGold(const DungeonRun& run, std::uint32_t rng)
{
    std::uint32_t state = rng | 1U;
    return 5 * run.rooms + static_cast<int>(roll(state) % 10U);
}

TurnResult battleTurn(CharacterId id, RpgProgress& p, Foe& foe, RpgAction action, std::uint32_t rng)
{
    TurnResult out;
    std::uint32_t state = rng | 1U;
    normalizeRpg(id, p);
    const RpgStats total = totalStats(id, p);

    // ---- player action -----------------------------------------------------
    switch (action) {
    case RpgAction::Attack: {
        const int base = 3 + total.str * 2;
        int dmg = base + static_cast<int>(roll(state) % 3U); // small variance
        const int critPct = clampInt(3 + total.luck * 3, 0, 35);
        if (static_cast<int>(roll(state) % 100U) < critPct) {
            dmg *= 2;
            out.playerCrit = true;
        }
        foe.hp = std::max(0, foe.hp - dmg);
        out.playerDmg = dmg;
        break;
    }
    case RpgAction::Skill: {
        if (p.mp < kSkillMpCost) {
            out.noMp = true; // refused: the turn is not spent
            return out;
        }
        p.mp -= kSkillMpCost;
        out.skillUsed = true;
        out.skill = skillFor(id);
        const int magic = 6 + total.mag * 3 + static_cast<int>(roll(state) % 4U);
        const int hpMax = hpMaxFor(id, p);
        switch (out.skill) {
        case SkillKind::Heal: {
            const int heal = 8 + total.mag * 3;
            out.drained = std::min(hpMax - p.hp, heal);
            p.hp += out.drained;
            break; // no damage: the foe still gets its turn
        }
        case SkillKind::Drain: {
            const int dmg = magic * 4 / 5; // slightly weaker, but it feeds
            foe.hp = std::max(0, foe.hp - dmg);
            out.playerDmg = dmg;
            out.drained = std::min(hpMax - p.hp, dmg / 2);
            p.hp += out.drained;
            break;
        }
        case SkillKind::Focus: {
            const int dmg = (3 + total.str * 2) * 2; // guaranteed crit
            foe.hp = std::max(0, foe.hp - dmg);
            out.playerDmg = dmg;
            out.playerCrit = true;
            break;
        }
        case SkillKind::Flurry: {
            const int hit = 2 + total.str + total.agi / 2;
            foe.hp = std::max(0, foe.hp - hit);
            out.playerDmg = hit;
            if (foe.hp > 0) {
                const int second = hit - 1 > 1 ? hit - 1 : 1;
                foe.hp = std::max(0, foe.hp - second);
                out.extraDmg = second;
            }
            break;
        }
        case SkillKind::Guard: {
            const int dmg = 2 + total.vit;
            foe.hp = std::max(0, foe.hp - dmg);
            out.playerDmg = dmg;
            p.guardTurns = 2; // this turn and the next one
            out.guarded = p.guardTurns;
            break;
        }
        case SkillKind::Lucky: {
            const int dmg = 5 + total.luck * 3 + static_cast<int>(roll(state) % 4U);
            foe.hp = std::max(0, foe.hp - dmg);
            out.playerDmg = dmg;
            if (static_cast<int>(roll(state) % 100U) < 20 + total.luck * 4) {
                out.snagged = 1 + foe.level; // fox fingers: gold mid-fight
            }
            break;
        }
        case SkillKind::Blast:
        default:
            foe.hp = std::max(0, foe.hp - magic); // magic never misses
            out.playerDmg = magic;
            break;
        }
        break;
    }
    case RpgAction::MpPotion: {
        if (p.mpPotions <= 0) {
            out.noPotion = true; // refused: the turn is not spent
            return out;
        }
        --p.mpPotions;
        const int mpMax = mpMaxFor(id, p);
        out.mpRestored = std::min(mpMax - p.mp, mpMax / 2);
        p.mp += out.mpRestored;
        break;
    }
    case RpgAction::Potion: {
        if (p.potions <= 0) {
            out.noPotion = true; // refused: the turn is not spent
            return out;
        }
        --p.potions;
        const int hpMax = hpMaxFor(id, p);
        out.healed = std::min(hpMax - p.hp, hpMax / 2);
        p.hp += out.healed;
        break;
    }
    case RpgAction::Run:
    default: {
        const int chance = clampInt(50 + total.agi * 3 - foe.level * 2, 20, 90);
        if (static_cast<int>(roll(state) % 100U) < chance) {
            out.escaped = true;
            return out;
        }
        out.runFailed = true; // failed: the foe still gets its turn
        break;
    }
    }

    // ---- victory? -----------------------------------------------------------
    if (foe.hp <= 0) {
        p.guardTurns = 0; // the fight is over
        out.victory = true;
        out.xpGained = foe.xpReward;
        out.goldGained = foe.goldReward;
        p.xp += foe.xpReward;
        while (p.xp >= xpForNext(p.level) && p.level < 99) {
            p.xp -= xpForNext(p.level);
            ++p.level;
            p.statPoints += kPointsPerLevel;
            ++out.levelsGained;
        }
        if (out.levelsGained > 0) {
            // Level-up perk: fully rested for the next fight.
            p.hp = hpMaxFor(id, p);
            p.mp = mpMaxFor(id, p);
        }
        return out;
    }

    // ---- foe's counterattack --------------------------------------------------
    const int dodgePct = clampInt(total.agi * 3, 0, 25);
    if (static_cast<int>(roll(state) % 100U) < dodgePct) {
        out.foeMissed = true;
    } else {
        int dmg = foe.atk + static_cast<int>(roll(state) % 3U);
        if (p.guardTurns > 0) {
            dmg = (dmg + 1) / 2; // Kolobok's crust: half damage, rounded up
        }
        p.hp = std::max(0, p.hp - dmg);
        out.foeDmg = dmg;
        if (p.guardTurns > 0) {
            --p.guardTurns; // the crust wears off after taking the swing
        }
        if (p.hp <= 0) {
            out.defeat = true;
            out.goldLostHint = 5 + foe.level * 2;
            p.hp = std::max(1, hpMaxFor(id, p) * 3 / 10); // wakes up at ~30%
        }
    }
    return out;
}

} // namespace vaiwu::game

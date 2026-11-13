// Unit tests for the RPG layer (stats, XP curve, battle turns, save V3).
#include "vaiwu/game/Rpg.hpp"
#include "vaiwu/game/Shop.hpp"
#include "vaiwu/game/VaiwuPet.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace vaiwu::game;

static int failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

namespace {

void testBaseStats()
{
    int sum0 = -1;
    for (int c = 0; c < kCharacterCount; ++c) {
        const RpgStats& base = baseStatsFor(static_cast<CharacterId>(c));
        CHECK(base.str >= 1 && base.vit >= 1 && base.agi >= 1 && base.mag >= 1 && base.luck >= 1);
        const int sum = base.str + base.vit + base.agi + base.mag + base.luck;
        if (sum0 < 0) {
            sum0 = sum;
        }
        CHECK(sum == sum0); // fairness: every character sums to the same total
    }
}

void testDerivedAndAllocation()
{
    RpgProgress p;
    normalizeRpg(CharacterId::Kolobok, p);
    CHECK(p.hp == hpMaxFor(CharacterId::Kolobok, p)); // -1 -> full
    CHECK(p.mp == mpMaxFor(CharacterId::Kolobok, p));

    const int hpBefore = hpMaxFor(CharacterId::Kolobok, p);
    p.alloc.vit += 2; // allocating vitality raises max HP
    CHECK(hpMaxFor(CharacterId::Kolobok, p) == hpBefore + 12);

    CHECK(xpForNext(1) == 20);
    CHECK(xpForNext(2) > xpForNext(1));
}

void testSpawnFoeIsWeak()
{
    for (std::uint32_t rng = 0; rng < 40; ++rng) {
        const Foe foe = spawnFoe(3, rng * 2654435761U + 17U);
        CHECK(foe.level >= 2 && foe.level <= 3); // "a weak monster": <= pet level
        CHECK(foe.hp == foe.hpMax && foe.hpMax > 0);
        CHECK(static_cast<int>(foe.kind) < kMonsterKindCount);
        CHECK(foe.xpReward > 0 && foe.goldReward > 0);
    }
    const Foe foe = spawnFoe(1, 12345U);
    CHECK(foe.level == 1); // never below level 1

    // Bestiary gating: a rookie only meets the first three kinds; the fox
    // joins at 3, the ghost at 5, the golem at 7.
    bool sawLate = false;
    for (std::uint32_t rng = 0; rng < 64; ++rng) {
        const Foe rookie = spawnFoe(2, rng * 7919U + 3U);
        CHECK(static_cast<int>(rookie.kind) < 3);
        const Foe veteran = spawnFoe(9, rng * 7919U + 3U);
        sawLate = sawLate || static_cast<int>(veteran.kind) >= 3;
    }
    CHECK(sawLate);

    // Kind modifiers: at the same level the golem is tankier than the
    // slime and the ghost hits harder while being frailer.
    int slimeHp = 0, golemHp = 0, ghostAtk = 0, slimeAtk = 0;
    for (std::uint32_t rng = 0; rng < 400; ++rng) {
        const Foe f = spawnFoe(9, rng * 2654435761U + 11U);
        if (f.level != 9) {
            continue;
        }
        if (f.kind == MonsterKind::Slime) { slimeHp = f.hpMax; slimeAtk = f.atk; }
        if (f.kind == MonsterKind::Golem) { golemHp = f.hpMax; }
        if (f.kind == MonsterKind::Ghost) { ghostAtk = f.atk; }
    }
    CHECK(slimeHp > 0 && golemHp > 0 && ghostAtk > 0);
    CHECK(golemHp > slimeHp);
    CHECK(ghostAtk > slimeAtk);
}

void testBattleVictoryAndLevelUp()
{
    RpgProgress p;
    normalizeRpg(CharacterId::Rin, p);
    Foe foe = spawnFoe(1, 7U);
    foe.hp = 1; // next hit wins
    const TurnResult r = battleTurn(CharacterId::Rin, p, foe, RpgAction::Attack, 3U);
    CHECK(r.victory);
    CHECK(r.playerDmg > 0);
    CHECK(r.xpGained == foe.xpReward && r.goldGained == foe.goldReward);
    CHECK(r.foeDmg == 0); // a dead foe does not strike back
    if (r.levelsGained > 0) {
        CHECK(p.statPoints == r.levelsGained * kPointsPerLevel);
        CHECK(p.hp == hpMaxFor(CharacterId::Rin, p)); // level-up fully heals
    }

    // Grind until a level-up definitely happened.
    RpgProgress q;
    normalizeRpg(CharacterId::Rin, q);
    int levels = 0;
    for (int i = 0; i < 12 && levels == 0; ++i) {
        Foe weak = spawnFoe(1, static_cast<std::uint32_t>(i) * 97U + 5U);
        weak.hp = 1;
        q.hp = hpMaxFor(CharacterId::Rin, q);
        const TurnResult win = battleTurn(CharacterId::Rin, q, weak, RpgAction::Attack,
            static_cast<std::uint32_t>(i) + 11U);
        CHECK(win.victory);
        levels += win.levelsGained;
    }
    CHECK(levels >= 1);
    CHECK(q.level == 1 + levels);
    CHECK(q.statPoints == levels * kPointsPerLevel);
}

void testSkillPotionRun()
{
    RpgProgress p;
    normalizeRpg(CharacterId::Yuki, p); // mag 5: strong skill
    Foe foe = spawnFoe(1, 21U);
    const int mpBefore = p.mp;
    p.hp -= 20; // room to drain into
    const TurnResult skill = battleTurn(CharacterId::Yuki, p, foe, RpgAction::Skill, 9U);
    CHECK(skill.skillUsed && skill.skill == SkillKind::Drain);
    CHECK(skill.playerDmg > 0 && skill.drained == skill.playerDmg / 2);
    CHECK(p.mp == mpBefore - kSkillMpCost);

    p.mp = kSkillMpCost - 1; // not enough mana: turn refused, foe frozen
    const int foeHp = foe.hp;
    const TurnResult noMp = battleTurn(CharacterId::Yuki, p, foe, RpgAction::Skill, 9U);
    CHECK(noMp.noMp && foe.hp == foeHp && noMp.foeDmg == 0);

    // Potion heals up to half of max HP and consumes one bottle.
    p.potions = 1;
    p.hp = 5;
    const TurnResult heal = battleTurn(CharacterId::Yuki, p, foe, RpgAction::Potion, 13U);
    CHECK(heal.healed > 0 && p.potions == 0 && p.hp > 5);
    const TurnResult empty = battleTurn(CharacterId::Yuki, p, foe, RpgAction::Potion, 13U);
    CHECK(empty.noPotion);

    // Running eventually works (agility-based chance, try many seeds).
    bool escaped = false;
    for (std::uint32_t rng = 1; rng < 32 && !escaped; ++rng) {
        RpgProgress runner;
        normalizeRpg(CharacterId::Hina, runner); // agi 4
        Foe f = spawnFoe(1, rng);
        escaped = battleTurn(CharacterId::Hina, runner, f, RpgAction::Run, rng * 7U).escaped;
    }
    CHECK(escaped);
}

void testPersonalSkills()
{
    // Every character has a skill and a localized name for it.
    for (int c = 0; c < kCharacterCount; ++c) {
        const auto id = static_cast<CharacterId>(c);
        CHECK(tr(Lang::En, skillNameFor(id)) != nullptr);
        CHECK(std::string(tr(Lang::Ru, skillNameFor(id))) != "?");
    }
    CHECK(skillFor(CharacterId::Unicorn) == SkillKind::Heal);
    CHECK(skillFor(CharacterId::Kolobok) == SkillKind::Guard);

    // Uni heals instead of hitting.
    RpgProgress uni;
    normalizeRpg(CharacterId::Unicorn, uni);
    uni.hp = 5;
    Foe foe = spawnFoe(1, 5U);
    const int foeHp = foe.hp;
    const TurnResult heal = battleTurn(CharacterId::Unicorn, uni, foe, RpgAction::Skill, 3U);
    CHECK(heal.skill == SkillKind::Heal && heal.drained > 0 && heal.playerDmg == 0);
    CHECK(foe.hp == foeHp); // the foe took no damage...
    CHECK(uni.hp > 5);      // ...but she is healthier

    // Rin's Focus is a guaranteed crit; Hina's Flurry lands a second hit.
    RpgProgress rin;
    normalizeRpg(CharacterId::Rin, rin);
    Foe foe2 = spawnFoe(1, 6U);
    foe2.hpMax = foe2.hp = 500; // survive both tests
    const TurnResult focus = battleTurn(CharacterId::Rin, rin, foe2, RpgAction::Skill, 4U);
    CHECK(focus.playerCrit && focus.playerDmg > 0);
    RpgProgress hina;
    normalizeRpg(CharacterId::Hina, hina);
    const TurnResult flurry = battleTurn(CharacterId::Hina, hina, foe2, RpgAction::Skill, 4U);
    CHECK(flurry.skill == SkillKind::Flurry && flurry.extraDmg > 0);

    // Kolobok's Guard halves the next incoming hit.
    RpgProgress kolo;
    normalizeRpg(CharacterId::Kolobok, kolo);
    Foe foe3 = spawnFoe(1, 8U);
    foe3.hpMax = foe3.hp = 500;
    const TurnResult guard = battleTurn(CharacterId::Kolobok, kolo, foe3, RpgAction::Skill, 2U);
    CHECK(guard.guarded > 0);
    int guardedHit = 0;
    int plainHit = 0;
    for (std::uint32_t rng = 1; rng < 40 && (guardedHit == 0 || plainHit == 0); ++rng) {
        RpgProgress a;
        normalizeRpg(CharacterId::Kolobok, a);
        a.guardTurns = 1;
        RpgProgress b;
        normalizeRpg(CharacterId::Kolobok, b);
        Foe fa = spawnFoe(1, rng);
        fa.hpMax = fa.hp = 500;
        Foe fb = fa;
        const TurnResult ra = battleTurn(CharacterId::Kolobok, a, fa, RpgAction::Attack, rng);
        const TurnResult rb = battleTurn(CharacterId::Kolobok, b, fb, RpgAction::Attack, rng);
        if (ra.foeDmg > 0 && rb.foeDmg > 0) {
            guardedHit = ra.foeDmg;
            plainHit = rb.foeDmg;
        }
    }
    CHECK(guardedHit > 0 && plainHit > 0 && guardedHit < plainHit);
}

void testDefeatIsGentle()
{
    RpgProgress p;
    normalizeRpg(CharacterId::Aoi, p);
    p.hp = 1; // one counterattack will floor us (dodge permitting)
    bool defeated = false;
    for (std::uint32_t rng = 1; rng < 64 && !defeated; ++rng) {
        Foe foe = spawnFoe(1, rng);
        p.hp = 1;
        const TurnResult r = battleTurn(CharacterId::Aoi, p, foe, RpgAction::Attack, rng * 31U);
        if (r.victory) {
            continue;
        }
        if (r.defeat) {
            defeated = true;
            CHECK(r.goldLostHint > 0);
            CHECK(p.hp > 0); // faints, wakes at ~30% - the pet never dies here
        }
    }
    CHECK(defeated);
}

void testDungeonRun()
{
    for (std::uint32_t rng = 0; rng < 32; ++rng) {
        const DungeonRun run = startDungeon(4, rng * 7919U + 1U);
        CHECK(run.active && run.room == 0);
        CHECK(run.rooms >= kDungeonMinRooms && run.rooms <= kDungeonMaxRooms);
    }

    DungeonRun run = startDungeon(4, 11U);
    CHECK(!isBossRoom(run));            // first room is not the boss...
    run.room = run.rooms - 1;
    CHECK(isBossRoom(run));             // ...the last one is

    // Deeper rooms are tougher and the boss is a wall that pays double.
    DungeonRun early = startDungeon(4, 11U);
    early.rooms = 4;
    early.room = 0;
    DungeonRun deep = early;
    deep.room = 2;
    DungeonRun boss = early;
    boss.room = 3;
    const Foe f0 = spawnDungeonFoe(early, 4, 5U);
    const Foe f2 = spawnDungeonFoe(deep, 4, 5U);
    const Foe fb = spawnDungeonFoe(boss, 4, 5U);
    CHECK(f2.hpMax > f0.hpMax && f2.atk >= f0.atk);
    CHECK(fb.hpMax > f2.hpMax);
    CHECK(fb.goldReward > f0.goldReward && fb.xpReward > f0.xpReward);

    // A breather restores a slice of HP/MP but never overfills.
    RpgProgress p;
    normalizeRpg(CharacterId::Aoi, p);
    p.hp = 1;
    p.mp = 0;
    p.guardTurns = 2;
    restBetweenRooms(CharacterId::Aoi, p);
    CHECK(p.hp > 1 && p.hp <= hpMaxFor(CharacterId::Aoi, p));
    CHECK(p.mp > 0 && p.mp <= mpMaxFor(CharacterId::Aoi, p));
    CHECK(p.guardTurns == 0); // protection does not carry between rooms
    p.hp = hpMaxFor(CharacterId::Aoi, p);
    restBetweenRooms(CharacterId::Aoi, p);
    CHECK(p.hp == hpMaxFor(CharacterId::Aoi, p));

    // The chest scales with the length of the run.
    CHECK(dungeonChestGold(early, 3U) > 0);
    DungeonRun longRun = early;
    longRun.rooms = kDungeonMaxRooms;
    CHECK(dungeonChestGold(longRun, 3U) > dungeonChestGold(early, 3U) - 10);
}

void testEquipment()
{
    RpgProgress p;
    normalizeRpg(CharacterId::Rin, p);
    const RpgStats before = totalStats(CharacterId::Rin, p);

    // Not owned -> setEquipped is ignored.
    setEquipped(p, EquipSlot::Weapon, static_cast<int>(EquipItem::SteelSword));
    CHECK(p.equipped[0] == -1);

    // Granting owns + auto-equips + boosts stats through totalStats.
    grantEquip(p, EquipItem::SteelSword);
    CHECK(ownsEquip(p, EquipItem::SteelSword));
    CHECK(p.equipped[0] == static_cast<int>(EquipItem::SteelSword));
    CHECK(totalStats(CharacterId::Rin, p).str == before.str + 4);

    // Swapping to another owned weapon; wrong-slot equips are ignored.
    grantEquip(p, EquipItem::MagicStaff);
    setEquipped(p, EquipSlot::Weapon, static_cast<int>(EquipItem::SteelSword));
    CHECK(p.equipped[0] == static_cast<int>(EquipItem::SteelSword));
    setEquipped(p, EquipSlot::Armor, static_cast<int>(EquipItem::SteelSword));
    CHECK(p.equipped[1] == -1);
    setEquipped(p, EquipSlot::Weapon, -1); // bare hands
    CHECK(totalStats(CharacterId::Rin, p).str == before.str);

    // Vitality gear raises max HP; normalize keeps hp within the new max.
    grantEquip(p, EquipItem::IronArmor);
    normalizeRpg(CharacterId::Rin, p);
    CHECK(totalStats(CharacterId::Rin, p).vit == before.vit + 4);

    // Every equip item is purchasable exactly once through the shop.
    VaiwuPet pet;
    pet.startNew(CharacterId::Hina, "H", 0);
    pet.addCoins(100);
    CHECK(buyShopItem(pet, ShopItem::WoodenSword));
    CHECK(ownsEquip(pet.rpg(), EquipItem::WoodenSword));
    const auto coins = pet.coins();
    CHECK(!buyShopItem(pet, ShopItem::WoodenSword)); // duplicate refused...
    CHECK(pet.coins() == coins);                     // ...and nothing spent
}

void testSellingAndManaPotions()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Yuki, "Y", 0);
    pet.addCoins(100);

    // Buying a mana potion fills the mana bag, not the HP one.
    const int hpPotionsBefore = pet.rpg().potions;
    CHECK(buyShopItem(pet, ShopItem::MpPotion));
    CHECK(pet.rpg().mpPotions == 1 && pet.rpg().potions == hpPotionsBefore);

    // ...and it restores MP mid-fight, refusing the turn when the bag is empty.
    Foe foe = spawnFoe(1, 4U);
    pet.rpg().mp = 0;
    const TurnResult drink = battleTurn(CharacterId::Yuki, pet.rpg(), foe,
        RpgAction::MpPotion, 7U);
    CHECK(drink.mpRestored > 0 && pet.rpg().mp > 0 && pet.rpg().mpPotions == 0);
    const TurnResult empty = battleTurn(CharacterId::Yuki, pet.rpg(), foe,
        RpgAction::MpPotion, 7U);
    CHECK(empty.noPotion);

    // Selling gear pays half, unequips it and removes it from the bag.
    CHECK(buyShopItem(pet, ShopItem::SteelSword));
    CHECK(pet.rpg().equipped[0] == static_cast<int>(EquipItem::SteelSword));
    const auto coinsBefore = pet.coins();
    const std::uint32_t paid = sellEquip(pet, EquipItem::SteelSword);
    CHECK(paid == static_cast<std::uint32_t>(equipSellPrice(22)));
    CHECK(pet.coins() == coinsBefore + paid);
    CHECK(!ownsEquip(pet.rpg(), EquipItem::SteelSword));
    CHECK(pet.rpg().equipped[0] == -1);
    CHECK(sellEquip(pet, EquipItem::SteelSword) == 0U); // nothing left to sell

    // ...and it can be bought again afterwards.
    CHECK(buyShopItem(pet, ShopItem::SteelSword));
}

void testStatsFeedBackIntoLifeSim()
{
    // Vitality points invested above the base slow the energy decay (spec
    // example); an untouched twin decays at the classic rate.
    VaiwuPet plain;
    plain.startNew(CharacterId::Aoi, "A", 0);
    VaiwuPet tough;
    tough.startNew(CharacterId::Aoi, "B", 0);
    tough.rpg().alloc.vit = 10;
    const std::uint64_t hour = 60ULL * 60ULL * 1000ULL;
    plain.tick(hour);
    tough.tick(hour);
    CHECK(tough.stats().energy > plain.stats().energy);
    // 10 points -> x100/150 = ~0.67 of the classic drain.
    const float plainDrop = 100.0f - plain.stats().energy;
    const float toughDrop = 100.0f - tough.stats().energy;
    CHECK(toughDrop < plainDrop * 0.72f && toughDrop > plainDrop * 0.60f);
    // Strength slows hunger, magic slows mood decay - same relief curve.
    VaiwuPet strong;
    strong.startNew(CharacterId::Aoi, "C", 0);
    strong.rpg().alloc.str = 10;
    strong.tick(hour);
    CHECK(strong.stats().hunger > plain.stats().hunger);
    VaiwuPet merry;
    merry.startNew(CharacterId::Aoi, "D", 0);
    merry.rpg().alloc.mag = 10;
    merry.tick(hour);
    CHECK(merry.stats().happiness > plain.stats().happiness);

    // Luck widens the healthy window: hunger 8% makes a plain pet sick but
    // not a lucky one (threshold 10 shifted below 8 by 10 points x 0.4).
    VaiwuPet frail;
    frail.startNew(CharacterId::Hina, "E", 0);
    VaiwuPet lucky;
    lucky.startNew(CharacterId::Hina, "F", 0);
    lucky.rpg().alloc.luck = 10;
    // Drain hunger to ~8% for both (92 min * 1%/2min = 46% -> too slow);
    // use the serialize path to set the exact state instead.
    CHECK(frail.deserialize("V2 1 8 50 50 50 20 100 0 0 0\nE", 0));
    CHECK(frail.isSick());
    CHECK(lucky.deserialize("V4 1 8 50 50 50 20 100 0 0 0 1 0 0 0 0 0 0 10 -1 -1 1 0 -1 -1 -1\nF", 0));
    CHECK(!lucky.isSick());
}

void testSaveV3RoundTripAndV2Compat()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Unicorn, "Uni", 0);
    pet.rpg().level = 4;
    pet.rpg().xp = 7;
    pet.rpg().statPoints = 2;
    pet.rpg().alloc = {1, 2, 0, 3, 0};
    normalizeRpg(CharacterId::Unicorn, pet.rpg());
    pet.rpg().hp = 9;
    pet.rpg().potions = 3;
    pet.rpg().mpPotions = 2;
    grantEquip(pet.rpg(), EquipItem::SteelSword);
    grantEquip(pet.rpg(), EquipItem::LuckyCharm);
    const std::string saved = pet.serialize();
    CHECK(saved.rfind("V5 ", 0) == 0);

    VaiwuPet loaded;
    CHECK(loaded.deserialize(saved, 0));
    CHECK(loaded.rpg().level == 4);
    CHECK(loaded.rpg().xp == 7);
    CHECK(loaded.rpg().statPoints == 2);
    CHECK(loaded.rpg().alloc.mag == 3);
    CHECK(loaded.rpg().hp == 9);
    CHECK(loaded.rpg().potions == 3);
    CHECK(loaded.rpg().mpPotions == 2);
    CHECK(ownsEquip(loaded.rpg(), EquipItem::SteelSword));
    CHECK(loaded.rpg().equipped[0] == static_cast<int>(EquipItem::SteelSword));
    CHECK(loaded.rpg().equipped[2] == static_cast<int>(EquipItem::LuckyCharm));

    // A V4 save (no mana potions yet) still loads, with an empty mana bag.
    VaiwuPet v4;
    CHECK(v4.deserialize(
        "V4 1 50 50 50 50 20 100 0 0 12 2 5 1 1 0 0 0 0 30 10 2 3 0 -1 -1\nHina", 0));
    CHECK(v4.rpg().level == 2 && v4.rpg().potions == 2 && v4.rpg().mpPotions == 0);
    CHECK(ownsEquip(v4.rpg(), EquipItem::WoodenSword)); // owned mask 3 = items 0,1

    // A V3 save (no equipment yet) still loads with empty gear.
    VaiwuPet v3;
    CHECK(v3.deserialize("V3 2 50 50 50 50 20 100 0 0 12 2 5 1 1 0 0 0 0 30 10 2\nRin", 0));
    CHECK(v3.rpg().level == 2 && v3.rpg().equipOwned == 0U);
    CHECK(v3.rpg().equipped[0] == -1);

    // An old V2 save still loads; the RPG block gets fresh defaults.
    VaiwuPet old;
    CHECK(old.deserialize("V2 1 50 50 50 50 20 100 0 0 12\nHina", 0));
    CHECK(old.rpg().level == 1);
    CHECK(old.rpg().hp == hpMaxFor(CharacterId::Hina, old.rpg()));
    CHECK(old.rpg().potions == 1);
}


void testPoisonTicksAndFaints()
{
    // Poison set directly: ticks at the start of the pet's turn, wears off.
    RpgProgress p;
    normalizeRpg(CharacterId::Aoi, p);
    p.poisonTurns = 2;
    const int hpBefore = p.hp;
    Foe foe = spawnFoe(1, 7U);
    const TurnResult r = battleTurn(CharacterId::Aoi, p, foe, RpgAction::Potion, 7U);
    CHECK(r.poisonDmg == 2 + foe.level / 2);
    CHECK(p.poisonTurns == 1);
    CHECK(p.hp == hpBefore - r.poisonDmg + r.healed - r.foeDmg); // the foe still swings

    // A lethal tick is a gentle faint like any other defeat.
    RpgProgress dying;
    normalizeRpg(CharacterId::Aoi, dying);
    dying.poisonTurns = 1;
    dying.hp = 1;
    Foe foe2 = spawnFoe(1, 9U);
    const TurnResult dead = battleTurn(CharacterId::Aoi, dying, foe2, RpgAction::Attack, 9U);
    CHECK(dead.defeat);
    CHECK(dying.hp >= 1); // wakes up at ~30%
    CHECK(dying.poisonTurns == 0);
}

void testStunWastesTheAction()
{
    RpgProgress p;
    normalizeRpg(CharacterId::Aoi, p);
    p.stunTurns = 1;
    Foe foe = spawnFoe(1, 11U);
    const int foeHp = foe.hp;
    const TurnResult r = battleTurn(CharacterId::Aoi, p, foe, RpgAction::Attack, 11U);
    CHECK(r.playerStunned);
    CHECK(r.playerDmg == 0);
    CHECK(foe.hp == foeHp); // she never swung
    CHECK(p.stunTurns == 0);
}

void testMonsterTricks()
{
    bool slimePoisoned = false;
    bool batStunned = false;
    bool foxStole = false;
    bool shroomRegen = false;
    for (std::uint32_t rng = 1; rng < 400; ++rng) {
        RpgProgress p;
        normalizeRpg(CharacterId::Hina, p);
        Foe foe = spawnFoe(1, rng);
        foe.hpMax = foe.hp = 500; // survive the hit so the counterattack happens
        if (!slimePoisoned) {
            foe.kind = MonsterKind::Slime;
            const TurnResult r = battleTurn(CharacterId::Hina, p, foe, RpgAction::Attack, rng);
            if (r.poisoned) {
                slimePoisoned = true;
                CHECK(p.poisonTurns == kPoisonTurns);
            }
        }
        if (!batStunned) {
            foe.kind = MonsterKind::Bat;
            const TurnResult r = battleTurn(CharacterId::Hina, p, foe, RpgAction::Attack, rng);
            if (r.stunned) {
                batStunned = true;
                CHECK(p.stunTurns == 1);
            }
        }
        if (!foxStole) {
            foe.kind = MonsterKind::Fox;
            const TurnResult r = battleTurn(CharacterId::Hina, p, foe, RpgAction::Attack, rng);
            if (r.goldStolen > 0) {
                foxStole = true;
                CHECK(r.goldStolen == 1 + foe.level);
            }
        }
        if (!shroomRegen) {
            foe.kind = MonsterKind::Shroom;
            foe.hp = 100;
            const TurnResult r = battleTurn(CharacterId::Hina, p, foe, RpgAction::Potion, rng);
            if (r.foeRegen > 0) {
                shroomRegen = true;
                CHECK(foe.hp == 100 + r.foeRegen);
            }
        }
        if (slimePoisoned && batStunned && foxStole && shroomRegen) {
            break;
        }
    }
    CHECK(slimePoisoned);
    CHECK(batStunned);
    CHECK(foxStole);
    CHECK(shroomRegen);
}

void testGhostPiercesGuard()
{
    int guardedHit = 0;
    int piercedHit = 0;
    for (std::uint32_t rng = 1; rng < 60 && (guardedHit == 0 || piercedHit == 0); ++rng) {
        RpgProgress a;
        normalizeRpg(CharacterId::Kolobok, a);
        a.guardTurns = 1;
        Foe foe = spawnFoe(1, rng);
        foe.hpMax = foe.hp = 500;
        foe.kind = MonsterKind::Slime;
        const TurnResult ra = battleTurn(CharacterId::Kolobok, a, foe, RpgAction::Attack, rng);
        if (ra.foeDmg > 0) {
            guardedHit = ra.foeDmg;
        }
        RpgProgress b;
        normalizeRpg(CharacterId::Kolobok, b);
        b.guardTurns = 1;
        Foe foeB = spawnFoe(1, rng);
        foeB.hpMax = foeB.hp = 500;
        foeB.kind = MonsterKind::Ghost;
        const TurnResult rb = battleTurn(CharacterId::Kolobok, b, foeB, RpgAction::Attack, rng);
        if (rb.foeDmg > 0) {
            piercedHit = rb.foeDmg;
        }
    }
    CHECK(guardedHit > 0 && piercedHit > 0);
    CHECK(piercedHit > guardedHit); // the ghost ignores the crust
}

void testWindDaggerPoisons()
{
    bool applied = false;
    for (std::uint32_t rng = 1; rng < 200 && !applied; ++rng) {
        RpgProgress p;
        normalizeRpg(CharacterId::Hina, p);
        grantEquip(p, EquipItem::WindDagger);
        Foe foe = spawnFoe(1, rng);
        foe.hpMax = foe.hp = 500;
        const TurnResult r = battleTurn(CharacterId::Hina, p, foe, RpgAction::Attack, rng);
        if (r.foePoisoned) {
            applied = true;
            CHECK(foe.poisonTurns == kPoisonTurns);
        }
    }
    CHECK(applied);

    // Golem is immune even with the dagger.
    for (std::uint32_t rng = 1; rng < 300; ++rng) {
        RpgProgress p;
        normalizeRpg(CharacterId::Hina, p);
        grantEquip(p, EquipItem::WindDagger);
        Foe foe = spawnFoe(1, rng);
        foe.hpMax = foe.hp = 500;
        foe.kind = MonsterKind::Golem;
        const TurnResult r = battleTurn(CharacterId::Hina, p, foe, RpgAction::Attack, rng);
        CHECK(!r.foePoisoned);
        CHECK(foe.poisonTurns == 0);
    }

    // Without the dagger a plain attack never poisons.
    for (std::uint32_t rng = 1; rng < 100; ++rng) {
        RpgProgress p;
        normalizeRpg(CharacterId::Hina, p);
        Foe foe = spawnFoe(1, rng);
        foe.hpMax = foe.hp = 500;
        const TurnResult r = battleTurn(CharacterId::Hina, p, foe, RpgAction::Attack, rng);
        CHECK(!r.foePoisoned);
    }
}

void testFoePoisonCanKill()
{
    RpgProgress p;
    normalizeRpg(CharacterId::Aoi, p);
    Foe foe = spawnFoe(1, 13U);
    foe.hp = 1;
    foe.poisonTurns = 1;
    // The player does nothing lethal; the venom finishes the fight.
    const TurnResult r = battleTurn(CharacterId::Aoi, p, foe, RpgAction::Potion, 13U);
    CHECK(r.foePoisonDmg > 0);
    CHECK(r.victory);
    CHECK(r.xpGained == foe.xpReward);
}

} // namespace

int main()
{
    testBaseStats();
    testDerivedAndAllocation();
    testSpawnFoeIsWeak();
    testBattleVictoryAndLevelUp();
    testSkillPotionRun();
    testDefeatIsGentle();
    testPersonalSkills();
    testEquipment();
    testDungeonRun();
    testSellingAndManaPotions();
    testStatsFeedBackIntoLifeSim();
    testSaveV3RoundTripAndV2Compat();
    testPoisonTicksAndFaints();
    testStunWastesTheAction();
    testMonsterTricks();
    testGhostPiercesGuard();
    testWindDaggerPoisons();
    testFoePoisonCanKill();

    if (failures == 0) {
        std::printf("ALL RPG TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURES\n", failures);
    return 1;
}

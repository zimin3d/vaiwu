#include "vaiwu/game/VaiwuPet.hpp"

#include <algorithm>
#include <cstdio>

namespace vaiwu::game {

namespace {
// Baseline decay rates, in stat points per minute (spec: hunger 1%/2min,
// happiness 1%/1.5min, energy 1%/1min, discipline 1%/5min).
constexpr float kHungerPerMin = 1.0f / 2.0f;
constexpr float kHappinessPerMin = 1.0f / 1.5f;
constexpr float kEnergyPerMin = 1.0f;
constexpr float kDisciplinePerMin = 1.0f / 5.0f;
// While asleep: energy regenerates 1% / 30s, hunger drains at half speed.
constexpr float kSleepRegenPerMin = 2.0f;

constexpr float kForcedSleepEnergy = 15.0f;
constexpr float kSadThreshold = 20.0f;
constexpr float kCapriceDiscipline = 20.0f;
constexpr float kHappyThreshold = 75.0f;

[[nodiscard]] float clamp01(float v)
{
    return std::clamp(v, 0.0f, 100.0f);
}
} // namespace

void VaiwuPet::startNew(CharacterId id, const std::string& name, std::uint64_t nowMs)
{
    character_ = id;
    name_ = name.empty() ? traitsFor(id).name : name;
    stats_ = PetStats{};
    life_ = LifeState::Awake;
    sick_ = false;
    started_ = true;
    dirty_ = true;
    lastTickMs_ = nowMs;
    rpg_ = RpgProgress{};
    normalizeRpg(character_, rpg_);
    rpgRegenPool_ = 0.0f;
}

void VaiwuPet::tick(std::uint64_t nowMs)
{
    if (!started_ || life_ == LifeState::Dead) {
        lastTickMs_ = nowMs;
        return;
    }
    if (nowMs <= lastTickMs_) {
        lastTickMs_ = nowMs;
        return;
    }

    const std::uint64_t deltaMs = nowMs - lastTickMs_;
    lastTickMs_ = nowMs;
    const float minutes = static_cast<float>(deltaMs) / 60000.0f;

    // Accumulate age with millisecond precision so 1-second ticks do not
    // systematically under- or over-count.
    ageMsRemainder_ += deltaMs;
    stats_.ageSeconds += static_cast<std::uint32_t>(ageMsRemainder_ / 1000ULL);
    ageMsRemainder_ %= 1000ULL;

    applyDecay(minutes);

    // Slow out-of-battle HP/MP recovery (2 HP + 1 MP per minute), pooled so
    // 1-second ticks do not truncate to zero.
    rpgRegenPool_ += minutes;
    if (rpgRegenPool_ >= 1.0f) {
        rpgRegenMinutes(character_, rpg_, rpgRegenPool_);
        rpgRegenPool_ = 0.0f;
    }

    refreshDerivedState();
    dirty_ = true;
}

namespace {
// Diminishing relief from invested RPG points: 0 pts = x1.0, 5 pts = x0.8,
// 10 pts = x0.67, floor x0.55 - helpful but never trivializing the sim.
float statRelief(int points)
{
    if (points <= 0) {
        return 1.0f;
    }
    const float relief = 100.0f / (100.0f + 5.0f * static_cast<float>(points));
    return relief < 0.55f ? 0.55f : relief;
}
} // namespace

RpgStats VaiwuPet::rpgBonusPoints() const
{
    const RpgStats total = totalStats(character_, rpg_);
    const RpgStats& base = baseStatsFor(character_);
    return {total.str - base.str, total.vit - base.vit, total.agi - base.agi,
        total.mag - base.mag, total.luck - base.luck};
}

void VaiwuPet::applyDecay(float minutes)
{
    const CharacterTraits& t = traits();
    const RpgStats bonus = rpgBonusPoints();

    if (life_ == LifeState::Sleeping) {
        // Agility = quick recovery: each invested point speeds sleep regen
        // by 3% (capped at +50%). Strength still slows hunger while asleep.
        float agiBoost = 1.0f + 0.03f * static_cast<float>(bonus.agi > 0 ? bonus.agi : 0);
        if (agiBoost > 1.5f) {
            agiBoost = 1.5f;
        }
        stats_.hunger = clamp01(stats_.hunger
            - kHungerPerMin * 0.5f * statRelief(bonus.str) * minutes);
        stats_.energy = clamp01(stats_.energy
            + kSleepRegenPerMin * t.sleepRegenMult * agiBoost * minutes);
        if (stats_.energy >= 100.0f) {
            life_ = LifeState::Awake; // fully rested, wakes up on her own
        }
        return;
    }

    // Strength = efficient body (hunger), vitality = stamina (energy),
    // magic = self-entertainment (mood) - see the spec in VaiwuPet.hpp.
    stats_.hunger = clamp01(stats_.hunger - kHungerPerMin * statRelief(bonus.str) * minutes);
    stats_.happiness = clamp01(stats_.happiness
        - kHappinessPerMin * t.happinessDecayMult * statRelief(bonus.mag) * minutes);
    stats_.energy = clamp01(stats_.energy
        - kEnergyPerMin * t.energyDecayMult * statRelief(bonus.vit) * minutes);

    float disciplineRate = kDisciplinePerMin;
    if (t.angryWhenHungry && stats_.hunger < 30.0f) {
        disciplineRate *= 3.0f; // Rin: being hungry erodes her patience fast
    }
    stats_.discipline = clamp01(stats_.discipline - disciplineRate * minutes);

    if (stats_.energy <= kForcedSleepEnergy) {
        life_ = LifeState::Sleeping;
    }
}

void VaiwuPet::refreshDerivedState()
{
    if (stats_.hunger <= 0.0f) {
        life_ = LifeState::Dead;
        sick_ = false;
        return;
    }

    sick_ = stats_.hunger < sickHungerThreshold() || stats_.weightKg > sickWeightThreshold();
}

// Luck widens the healthy window: every invested point lowers the hunger
// threshold by 0.4 (min 3) and raises the weight one by 0.4 (max 92).
float VaiwuPet::sickHungerThreshold() const
{
    const float base = traits().sicknessResistant ? 5.0f : 10.0f;
    const int luck = rpgBonusPoints().luck;
    const float shifted = base - 0.4f * static_cast<float>(luck > 0 ? luck : 0);
    return shifted < 3.0f ? 3.0f : shifted;
}

float VaiwuPet::sickWeightThreshold() const
{
    const float base = traits().sicknessResistant ? 90.0f : 80.0f;
    const int luck = rpgBonusPoints().luck;
    const float shifted = base + 0.4f * static_cast<float>(luck > 0 ? luck : 0);
    return shifted > 92.0f ? 92.0f : shifted;
}

void VaiwuPet::addHappiness(float amount)
{
    if (amount > 0.0f) {
        amount *= traits().happinessGainMult;
    }
    stats_.happiness = clamp01(stats_.happiness + amount);
}

ActionResult VaiwuPet::feed()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    if (stats_.discipline < kCapriceDiscipline) return ActionResult::RefusedCaprice;
    if (stats_.hunger >= 99.0f) return ActionResult::NotHungry;

    const bool overfeed = stats_.hunger > 90.0f;
    stats_.hunger = clamp01(stats_.hunger + 30.0f);
    stats_.weightKg = std::min(99.0f, stats_.weightKg + 1.0f);
    if (overfeed) {
        stats_.discipline = clamp01(stats_.discipline - 5.0f);
    }
    addHappiness(3.0f);
    refreshDerivedState();
    dirty_ = true;
    return ActionResult::Ok;
}

ActionResult VaiwuPet::startPlay()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    if (stats_.energy < 20.0f) return ActionResult::TooTired;

    // Games are meant to be the fun part - keep them cheap (was 15.0f,
    // which drained the pet after just a few rounds).
    stats_.energy = clamp01(stats_.energy - 8.0f);
    stats_.weightKg = std::max(1.0f, stats_.weightKg - 2.0f);
    if (stats_.energy <= kForcedSleepEnergy) {
        life_ = LifeState::Sleeping;
    }
    refreshDerivedState();
    dirty_ = true;
    return ActionResult::Ok;
}

void VaiwuPet::applyPlayResult(bool success)
{
    if (life_ == LifeState::Dead) return;
    const CharacterTraits& t = traits();
    if (success) {
        addHappiness(20.0f * t.playHappinessMult);
        stats_.discipline = clamp01(stats_.discipline + 2.0f);
    } else {
        addHappiness(5.0f * t.playHappinessMult);
        stats_.discipline = clamp01(stats_.discipline - 5.0f);
    }
    refreshDerivedState();
    dirty_ = true;
}

ActionResult VaiwuPet::wash()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    addHappiness(10.0f);
    dirty_ = true;
    return ActionResult::Ok;
}

ActionResult VaiwuPet::toilet()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    addHappiness(5.0f);
    stats_.discipline = clamp01(stats_.discipline + 1.0f);
    dirty_ = true;
    return ActionResult::Ok;
}

ActionResult VaiwuPet::talk()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    addHappiness(10.0f);
    dirty_ = true;
    return ActionResult::Ok;
}

void VaiwuPet::giveAffection(float happinessAmount)
{
    if (life_ != LifeState::Awake) {
        return;
    }
    addHappiness(happinessAmount);
    dirty_ = true;
}

ActionResult VaiwuPet::treat()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    if (stats_.hunger >= 99.0f) return ActionResult::NotHungry;
    stats_.hunger = clamp01(stats_.hunger + 10.0f);
    stats_.weightKg = std::min(99.0f, stats_.weightKg + 0.5f);
    addHappiness(5.0f);
    refreshDerivedState();
    dirty_ = true;
    return ActionResult::Ok;
}

bool VaiwuPet::wake()
{
    if (life_ != LifeState::Sleeping) {
        return false;
    }
    life_ = LifeState::Awake;
    // Being woken up early is mildly annoying (unless fully rested anyway).
    if (stats_.energy < 80.0f) {
        stats_.happiness = clamp01(stats_.happiness - 3.0f);
    }
    dirty_ = true;
    return true;
}

Emotion VaiwuPet::emotion() const
{
    if (life_ == LifeState::Dead) return Emotion::Dead;
    if (life_ == LifeState::Sleeping) return Emotion::Sleepy;
    if (sick_) return Emotion::Sick;
    const CharacterTraits& t = traits();
    if ((t.angryWhenHungry && stats_.hunger < 30.0f) || stats_.discipline < kCapriceDiscipline) {
        return Emotion::Angry;
    }
    if (stats_.happiness < kSadThreshold) return Emotion::Sad;
    if (stats_.happiness > kHappyThreshold) return Emotion::Happy;
    return Emotion::Idle;
}

void VaiwuPet::cure()
{
    if (life_ == LifeState::Dead || !sick_) {
        return;
    }
    const float sickHunger = sickHungerThreshold();
    const float sickWeight = sickWeightThreshold();
    if (stats_.hunger < sickHunger) {
        stats_.hunger = clamp01(sickHunger + 20.0f); // clear hunger-caused illness
    }
    if (stats_.weightKg > sickWeight) {
        stats_.weightKg = std::max(1.0f, sickWeight - 10.0f); // clear weight-caused illness
    }
    refreshDerivedState();
    dirty_ = true;
}

void VaiwuPet::addCoins(std::uint32_t amount)
{
    stats_.coins += amount;
    if (stats_.coins > 99999U) {
        stats_.coins = 99999U;
    }
    dirty_ = true;
}

bool VaiwuPet::spendCoins(std::uint32_t amount)
{
    if (stats_.coins < amount) {
        return false;
    }
    stats_.coins -= amount;
    dirty_ = true;
    return true;
}

void VaiwuPet::applyGift(float hunger, float happiness, float energy, float weightDelta)
{
    if (life_ == LifeState::Dead) {
        return;
    }
    stats_.hunger = clamp01(stats_.hunger + hunger);
    addHappiness(happiness);
    stats_.energy = clamp01(stats_.energy + energy);
    stats_.weightKg = std::clamp(stats_.weightKg + weightDelta, 1.0f, 99.0f);
    refreshDerivedState();
    dirty_ = true;
}

BodyShape VaiwuPet::bodyShape() const
{
    if (stats_.weightKg < 10.0f) return BodyShape::Thin;
    if (stats_.weightKg < 40.0f) return BodyShape::Normal;
    if (stats_.weightKg < 65.0f) return BodyShape::Chubby;
    return BodyShape::Fat;
}

std::string VaiwuPet::serialize() const
{
    if (!started_) {
        return {};
    }
    char buf[352];
    // V5 = V4 + the mana-potion count.
    std::snprintf(buf, sizeof(buf), "V5 %d %.2f %.2f %.2f %.2f %.2f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d %u %d %d %d %d\n",
        static_cast<int>(character_),
        static_cast<double>(stats_.hunger), static_cast<double>(stats_.happiness),
        static_cast<double>(stats_.energy), static_cast<double>(stats_.discipline),
        static_cast<double>(stats_.weightKg),
        static_cast<unsigned>(stats_.ageSeconds),
        static_cast<int>(life_), sick_ ? 1 : 0,
        static_cast<unsigned>(stats_.coins),
        rpg_.level, rpg_.xp, rpg_.statPoints,
        rpg_.alloc.str, rpg_.alloc.vit, rpg_.alloc.agi, rpg_.alloc.mag, rpg_.alloc.luck,
        rpg_.hp, rpg_.mp, rpg_.potions,
        static_cast<unsigned>(rpg_.equipOwned),
        rpg_.equipped[0], rpg_.equipped[1], rpg_.equipped[2], rpg_.mpPotions);
    return std::string(buf) + name_;
}

bool VaiwuPet::deserialize(const std::string& data, std::uint64_t nowMs)
{
    int charId = 0;
    float hunger = 0, happiness = 0, energy = 0, discipline = 0, weight = 0;
    unsigned age = 0;
    int life = 0, sick = 0;
    unsigned coins = 0;
    RpgProgress rpg{};
    unsigned owned = 0;
    // V5 adds mana potions; V4 (equipment), V3 (RPG), V2 (coins) and V1
    // saves are still accepted.
    int parsed = std::sscanf(data.c_str(),
        "V5 %d %f %f %f %f %f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d %u %d %d %d %d",
        &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick, &coins,
        &rpg.level, &rpg.xp, &rpg.statPoints,
        &rpg.alloc.str, &rpg.alloc.vit, &rpg.alloc.agi, &rpg.alloc.mag, &rpg.alloc.luck,
        &rpg.hp, &rpg.mp, &rpg.potions,
        &owned, &rpg.equipped[0], &rpg.equipped[1], &rpg.equipped[2], &rpg.mpPotions);
    if (parsed != 26) {
        rpg = RpgProgress{};
        owned = 0;
        parsed = std::sscanf(data.c_str(),
            "V4 %d %f %f %f %f %f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d %u %d %d %d",
            &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick, &coins,
            &rpg.level, &rpg.xp, &rpg.statPoints,
            &rpg.alloc.str, &rpg.alloc.vit, &rpg.alloc.agi, &rpg.alloc.mag, &rpg.alloc.luck,
            &rpg.hp, &rpg.mp, &rpg.potions,
            &owned, &rpg.equipped[0], &rpg.equipped[1], &rpg.equipped[2]);
    }
    if (parsed == 25 || parsed == 26) {
        rpg.equipOwned = owned;
    } else {
        rpg = RpgProgress{};
        parsed = std::sscanf(data.c_str(),
            "V3 %d %f %f %f %f %f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d",
            &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick, &coins,
            &rpg.level, &rpg.xp, &rpg.statPoints,
            &rpg.alloc.str, &rpg.alloc.vit, &rpg.alloc.agi, &rpg.alloc.mag, &rpg.alloc.luck,
            &rpg.hp, &rpg.mp, &rpg.potions);
    }
    if (parsed != 21 && parsed != 25 && parsed != 26) {
        rpg = RpgProgress{};
        parsed = std::sscanf(data.c_str(), "V2 %d %f %f %f %f %f %u %d %d %u",
            &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick, &coins);
        if (parsed != 10) {
            coins = 0;
            parsed = std::sscanf(data.c_str(), "V1 %d %f %f %f %f %f %u %d %d",
                &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick);
            if (parsed != 9) {
                return false;
            }
        }
    }
    const std::size_t nl = data.find('\n');
    if (nl == std::string::npos || nl + 1 >= data.size()) {
        return false;
    }
    if (charId < 0 || charId >= kCharacterCount || life < 0 || life > 2) {
        return false;
    }

    character_ = static_cast<CharacterId>(charId);
    name_ = data.substr(nl + 1);
    stats_.hunger = clamp01(hunger);
    stats_.happiness = clamp01(happiness);
    stats_.energy = clamp01(energy);
    stats_.discipline = clamp01(discipline);
    stats_.weightKg = std::clamp(weight, 1.0f, 99.0f);
    stats_.ageSeconds = age;
    stats_.coins = coins;
    life_ = static_cast<LifeState>(life);
    sick_ = sick != 0;
    rpg_ = rpg;
    normalizeRpg(character_, rpg_);
    rpgRegenPool_ = 0.0f;
    started_ = true;
    dirty_ = false;
    lastTickMs_ = nowMs;
    refreshDerivedState();
    return true;
}

} // namespace vaiwu::game

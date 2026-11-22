#include "vaiwu/game/VaiwuPet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace vaiwu::game {

namespace {
// Time conversion & offline simulation step size.
constexpr float kMsPerMinute = 60000.0f;
constexpr std::uint64_t kMsPerSecond = 1000ULL;
constexpr float kOfflineSimStepMinutes = 1.0f;

// Stat clamp boundaries and inventory limits.
constexpr float kStatMin = 0.0f;
constexpr float kStatMax = 100.0f;
constexpr float kMinWeightKg = 1.0f;
constexpr float kMaxWeightKg = 99.0f;
constexpr std::uint32_t kMaxCoins = 99999U;
constexpr int kMaxConsumableStack = 99;
constexpr std::size_t kMaxPetNameBytes = 128U;

// Baseline decay rates, in stat points per minute (spec: hunger 1%/2min,
// happiness 1%/1.5min, energy softened to 0.5%/min, discipline 1%/10min).
constexpr float kHungerPerMin = 1.0f / 2.0f;
constexpr float kHappinessPerMin = 1.0f / 1.5f;
constexpr float kEnergyPerMin = 0.5f;
// Discipline: the spec says 1%/5min, but discipline is the only stat without a
// burst recovery, so the raw spec rate drained the bar to caprice (<20) in
// 6.7h of wake time. Softened to 1%/10min and sleep now restores +1%/min
// (see kSleepDisciplineRegenPerMin).
constexpr float kDisciplinePerMin = 1.0f / 10.0f;

// Sleep recovery & modifiers: energy regenerates 2.5%/min (~34 min for a full
// rest), hunger drains at half speed, and discipline recovers (+1%/min).
constexpr float kSleepRegenPerMin = 2.5f;
constexpr float kSleepDisciplineRegenPerMin = 1.0f;
constexpr float kSleepHungerDrainMult = 0.5f;
constexpr float kAgiSleepBoostPerPoint = 0.03f; // +3% sleep regen per bonus AGI
constexpr float kAgiSleepBoostMax = 1.5f;       // cap at +50% faster sleep

// Diminishing-returns decay relief from bonus RPG stat points:
// 0 pts = x1.0, 5 pts = x0.8, 10 pts = x0.67, floor x0.55.
constexpr float kStatReliefSlope = 5.0f;
constexpr float kStatReliefFloor = 0.55f;

// Sleep / play / emotion thresholds.
constexpr float kForcedSleepEnergy = 15.0f;
constexpr float kManualSleepMaxEnergy = 90.0f;
constexpr float kPlayMinEnergy = 20.0f;
constexpr float kPlayEnergyCost = 8.0f;
// After a manual wake-up the pet stays awake this long even while her energy
// is still below kForcedSleepEnergy. Without it, wake() would be undone by the
// next tick and a sick, exhausted pet could not be fed or treated at all.
constexpr float kWakeGraceMinutes = 2.0f;
constexpr float kEarlyWakeEnergyThreshold = 80.0f;
constexpr float kEarlyWakeHappinessPenalty = 3.0f;
constexpr float kSadThreshold = 20.0f;
constexpr float kCapriceDiscipline = 20.0f;
constexpr float kHappyThreshold = 75.0f;
constexpr float kHungryAngerThreshold = 30.0f;
constexpr float kHungryAngerDisciplineMult = 3.0f; // Rin loses patience 3x faster when hungry

// Sickness thresholds and Luck scaling (+0.4 buffer per bonus Luck point).
constexpr float kSickHungerBase = 10.0f;
constexpr float kSickHungerResistant = 5.0f;
constexpr float kSickHungerMin = 3.0f;
constexpr float kSickWeightBase = 80.0f;
constexpr float kSickWeightResistant = 90.0f;
constexpr float kSickWeightMax = 92.0f;
constexpr float kSickLuckShiftPerPoint = 0.4f;
constexpr float kCureHungerMargin = 20.0f;
constexpr float kCureWeightMargin = 10.0f;

// Daily care action stat deltas.
constexpr float kNotHungryThreshold = 99.0f;
constexpr float kOverfeedHungerThreshold = 90.0f;
constexpr float kFeedHungerGain = 30.0f;
constexpr float kFeedWeightGain = 1.0f;
constexpr float kOverfeedDisciplinePenalty = 5.0f;
constexpr float kFeedHappinessGain = 3.0f;
constexpr float kPlayWeightLoss = 2.0f;
constexpr float kPlayWinHappiness = 20.0f;
constexpr float kPlayLossHappiness = 5.0f;
constexpr float kPlayDisciplineDelta = 3.0f;
constexpr float kWashHappinessGain = 10.0f;
constexpr float kToiletHappinessGain = 5.0f;
constexpr float kToiletDisciplineGain = 1.0f;
constexpr float kTalkHappinessGain = 10.0f;
constexpr float kTreatHungerGain = 10.0f;
constexpr float kTreatWeightGain = 0.5f;
constexpr float kTreatHappinessGain = 5.0f;

// Weight cutoffs for visual body shape tiers (see BodyShape in VaiwuPet.hpp).
constexpr float kThinMaxWeightKg = 10.0f;
constexpr float kNormalMaxWeightKg = 40.0f;
constexpr float kChubbyMaxWeightKg = 65.0f;

// Expected `sscanf` field counts for each backward-compatible save version.
constexpr int kV6FieldCount = 38;
constexpr int kV5FieldCount = 26;
constexpr int kV4FieldCount = 25;
constexpr int kV3FieldCount = 21;
constexpr int kV2FieldCount = 10;
constexpr int kV1FieldCount = 9;
constexpr int kMaxLifeStateId = 2;

// Clamps a percentage stat (hunger, happiness, energy, discipline) to [0, 100].
[[nodiscard]] float clamp01(float v)
{
    return std::clamp(v, kStatMin, kStatMax);
}

// Diminishing relief from invested RPG points so stats help without trivializing decay.
[[nodiscard]] float statRelief(int points)
{
    if (points <= 0) {
        return 1.0f;
    }
    const float relief = kStatMax / (kStatMax + kStatReliefSlope * static_cast<float>(points));
    return relief < kStatReliefFloor ? kStatReliefFloor : relief;
}
} // namespace

// Initializes a brand-new pet with default starting needs and level-1 RPG stats.
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
    wakeGraceMinutes_ = 0.0f;
}

// Advances simulation needs, sleep/wake transitions, age, and out-of-battle RPG regen.
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
    const float minutes = static_cast<float>(deltaMs) / kMsPerMinute;

    // Accumulate age with millisecond precision so 1-second ticks do not
    // systematically under- or over-count.
    ageMsRemainder_ += deltaMs;
    stats_.ageSeconds += static_cast<std::uint32_t>(ageMsRemainder_ / kMsPerSecond);
    ageMsRemainder_ %= kMsPerSecond;

    // Simulate long offline gaps in 1-minute steps so Awake <-> Sleeping
    // transitions happen naturally while the app is closed instead of draining
    // energy to 0% in one jump or sleeping for hours past 100%.
    float remaining = minutes;
    while (remaining > 0.0f && life_ != LifeState::Dead) {
        const float step = remaining > kOfflineSimStepMinutes ? kOfflineSimStepMinutes : remaining;
        applyDecay(step);
        refreshDerivedState();
        remaining -= step;
    }

    // Slow out-of-battle HP/MP recovery (2 HP + 1 MP per minute), pooled so
    // 1-second ticks do not truncate to zero.
    rpgRegenPool_ += minutes;
    if (rpgRegenPool_ >= kOfflineSimStepMinutes) {
        rpgRegenMinutes(character_, rpg_, rpgRegenPool_);
        rpgRegenPool_ = 0.0f;
    }

    dirty_ = true;
}

// Returns only the RPG stat points earned above the character's level-1 baseline.
RpgStats VaiwuPet::rpgBonusPoints() const
{
    const RpgStats total = totalStats(character_, rpg_);
    const RpgStats& base = baseStatsFor(character_);
    return {total.str - base.str, total.vit - base.vit, total.agi - base.agi,
        total.mag - base.mag, total.luck - base.luck};
}

// Applies one time step of passive need decay (while awake) or recovery (while sleeping).
void VaiwuPet::applyDecay(float minutes)
{
    const CharacterTraits& t = traits();
    const RpgStats bonus = rpgBonusPoints();

    if (life_ == LifeState::Sleeping) {
        // Agility = quick recovery: each invested point speeds sleep regen
        // by 3% (capped at +50%). Strength still slows hunger while asleep.
        float agiBoost = 1.0f + kAgiSleepBoostPerPoint * static_cast<float>(bonus.agi > 0 ? bonus.agi : 0);
        if (agiBoost > kAgiSleepBoostMax) {
            agiBoost = kAgiSleepBoostMax;
        }
        stats_.hunger = clamp01(stats_.hunger
            - kHungerPerMin * kSleepHungerDrainMult * statRelief(bonus.str) * minutes);
        stats_.energy = clamp01(stats_.energy
            + kSleepRegenPerMin * t.sleepRegenMult * agiBoost * minutes);
        // Sleep is the discipline recovery window: a full nap restores discipline
        // instead of merely freezing it.
        stats_.discipline = clamp01(stats_.discipline
            + kSleepDisciplineRegenPerMin * minutes);
        if (stats_.energy >= kStatMax) {
            life_ = LifeState::Awake; // fully rested, wakes up on her own
        }
        return;
    }

    // A manual wake-up buys a short awake window even at low energy, so the
    // player can actually reach the care actions before she dozes off again.
    if (wakeGraceMinutes_ > 0.0f) {
        wakeGraceMinutes_ = std::max(0.0f, wakeGraceMinutes_ - minutes);
    }

    // Strength = efficient body (hunger), vitality = stamina (energy),
    // magic = self-entertainment (mood) - see the spec in VaiwuPet.hpp.
    stats_.hunger = clamp01(stats_.hunger - kHungerPerMin * statRelief(bonus.str) * minutes);
    stats_.happiness = clamp01(stats_.happiness
        - kHappinessPerMin * t.happinessDecayMult * statRelief(bonus.mag) * minutes);
    stats_.energy = clamp01(stats_.energy
        - kEnergyPerMin * t.energyDecayMult * statRelief(bonus.vit) * minutes);

    float disciplineRate = kDisciplinePerMin;
    if (t.angryWhenHungry && stats_.hunger < kHungryAngerThreshold) {
        disciplineRate *= kHungryAngerDisciplineMult; // Rin: being hungry erodes her patience fast
    }
    stats_.discipline = clamp01(stats_.discipline - disciplineRate * minutes);

    if (wakeGraceMinutes_ <= 0.0f && stats_.energy <= kForcedSleepEnergy) {
        life_ = LifeState::Sleeping;
    }
}

// Re-evaluates death and sickness flags after any stat change.
void VaiwuPet::refreshDerivedState()
{
    if (stats_.hunger <= kStatMin) {
        life_ = LifeState::Dead;
        sick_ = false;
        return;
    }

    sick_ = stats_.hunger < sickHungerThreshold() || stats_.weightKg > sickWeightThreshold();
}

// Luck widens the healthy window: every invested point lowers the hunger
// sickness threshold by 0.4 (min 3).
float VaiwuPet::sickHungerThreshold() const
{
    const float base = traits().sicknessResistant ? kSickHungerResistant : kSickHungerBase;
    const int luck = rpgBonusPoints().luck;
    const float shifted = base - kSickLuckShiftPerPoint * static_cast<float>(luck > 0 ? luck : 0);
    return shifted < kSickHungerMin ? kSickHungerMin : shifted;
}

// Luck raises the overweight sickness threshold by 0.4 per bonus point (max 92 kg).
float VaiwuPet::sickWeightThreshold() const
{
    const float base = traits().sicknessResistant ? kSickWeightResistant : kSickWeightBase;
    const int luck = rpgBonusPoints().luck;
    const float shifted = base + kSickLuckShiftPerPoint * static_cast<float>(luck > 0 ? luck : 0);
    return shifted > kSickWeightMax ? kSickWeightMax : shifted;
}

// Applies a happiness delta scaled by the character's positive-mood trait multiplier.
void VaiwuPet::addHappiness(float amount)
{
    if (amount > 0.0f) {
        amount *= traits().happinessGainMult;
    }
    stats_.happiness = clamp01(stats_.happiness + amount);
}

// Feeds the pet a standard free meal (+30 hunger, +1 kg); overfeeding costs discipline.
ActionResult VaiwuPet::feed()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    if (stats_.discipline < kCapriceDiscipline) return ActionResult::RefusedCaprice;
    if (stats_.hunger >= kNotHungryThreshold) return ActionResult::NotHungry;

    const bool overfeed = stats_.hunger > kOverfeedHungerThreshold;
    stats_.hunger = clamp01(stats_.hunger + kFeedHungerGain);
    stats_.weightKg = std::min(kMaxWeightKg, stats_.weightKg + kFeedWeightGain);
    if (overfeed) {
        stats_.discipline = clamp01(stats_.discipline - kOverfeedDisciplinePenalty);
    }
    addHappiness(kFeedHappinessGain);
    refreshDerivedState();
    dirty_ = true;
    return ActionResult::Ok;
}

// Pays the upfront energy and weight cost of starting a mini-game round.
ActionResult VaiwuPet::startPlay()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    if (stats_.energy < kPlayMinEnergy) return ActionResult::TooTired;
    // The round pays its energy cost up front, so refuse it when the payment
    // alone would put her back to sleep: the caller would cancel immediately
    // and the energy would be gone without a game being played.
    if (stats_.energy - kPlayEnergyCost <= kForcedSleepEnergy) {
        return ActionResult::TooTired;
    }

    stats_.energy = clamp01(stats_.energy - kPlayEnergyCost);
    stats_.weightKg = std::max(kMinWeightKg, stats_.weightKg - kPlayWeightLoss);
    refreshDerivedState();
    dirty_ = true;
    return ActionResult::Ok;
}

// Applies happiness and symmetric discipline stakes (+3 win / -3 loss) after a mini-game finishes.
void VaiwuPet::applyPlayResult(bool success)
{
    if (life_ == LifeState::Dead) return;
    const CharacterTraits& t = traits();
    // Symmetric stakes (+3/-3): the old +2/-5 made every round a net drain
    // (EV -1.5 at 50% wins), so active play pushed discipline to caprice.
    if (success) {
        addHappiness(kPlayWinHappiness * t.playHappinessMult);
        stats_.discipline = clamp01(stats_.discipline + kPlayDisciplineDelta);
    } else {
        addHappiness(kPlayLossHappiness * t.playHappinessMult);
        stats_.discipline = clamp01(stats_.discipline - kPlayDisciplineDelta);
    }
    refreshDerivedState();
    dirty_ = true;
}

// Washes the pet for a moderate happiness boost.
ActionResult VaiwuPet::wash()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    addHappiness(kWashHappinessGain);
    dirty_ = true;
    return ActionResult::Ok;
}

// Cleans up after the pet, granting small happiness and discipline gains.
ActionResult VaiwuPet::toilet()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    addHappiness(kToiletHappinessGain);
    stats_.discipline = clamp01(stats_.discipline + kToiletDisciplineGain);
    dirty_ = true;
    return ActionResult::Ok;
}

// Chats with the pet for a moderate happiness boost.
ActionResult VaiwuPet::talk()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    addHappiness(kTalkHappinessGain);
    dirty_ = true;
    return ActionResult::Ok;
}

// Applies a small mood lift from dialog interactions (hug/pat) while awake.
void VaiwuPet::giveAffection(float happinessAmount)
{
    if (life_ != LifeState::Awake) {
        return;
    }
    addHappiness(happinessAmount);
    dirty_ = true;
}

// Gives a light snack that bypasses caprice refusal at the cost of a small weight gain.
ActionResult VaiwuPet::treat()
{
    if (life_ == LifeState::Dead) return ActionResult::RefusedDead;
    if (life_ == LifeState::Sleeping) return ActionResult::RefusedSleeping;
    if (stats_.hunger >= kNotHungryThreshold) return ActionResult::NotHungry;
    stats_.hunger = clamp01(stats_.hunger + kTreatHungerGain);
    stats_.weightKg = std::min(kMaxWeightKg, stats_.weightKg + kTreatWeightGain);
    addHappiness(kTreatHappinessGain);
    refreshDerivedState();
    dirty_ = true;
    return ActionResult::Ok;
}

// Wakes a sleeping pet early with a short grace window so she does not immediately fall back asleep.
bool VaiwuPet::wake()
{
    if (life_ != LifeState::Sleeping) {
        return false;
    }
    life_ = LifeState::Awake;
    wakeGraceMinutes_ = kWakeGraceMinutes;
    // Being woken up early is mildly annoying (unless nearly rested anyway).
    if (stats_.energy < kEarlyWakeEnergyThreshold) {
        stats_.happiness = clamp01(stats_.happiness - kEarlyWakeHappinessPenalty);
    }
    dirty_ = true;
    return true;
}

// Manually puts an awake pet to sleep whenever energy is below kManualSleepMaxEnergy (90%).
bool VaiwuPet::sleep()
{
    if (life_ != LifeState::Awake || stats_.energy >= kManualSleepMaxEnergy) {
        return false;
    }
    life_ = LifeState::Sleeping;
    wakeGraceMinutes_ = 0.0f;
    dirty_ = true;
    return true;
}

// Derives the current visual emotion from life state, sickness, traits, and stat thresholds.
Emotion VaiwuPet::emotion() const
{
    if (life_ == LifeState::Dead) return Emotion::Dead;
    if (life_ == LifeState::Sleeping) return Emotion::Sleepy;
    if (sick_) return Emotion::Sick;
    const CharacterTraits& t = traits();
    if ((t.angryWhenHungry && stats_.hunger < kHungryAngerThreshold) || stats_.discipline < kCapriceDiscipline) {
        return Emotion::Angry;
    }
    if (stats_.happiness < kSadThreshold) return Emotion::Sad;
    if (stats_.happiness > kHappyThreshold) return Emotion::Happy;
    return Emotion::Idle;
}

// Clears active illness by lifting hunger and/or trimming weight safely past the sick thresholds.
void VaiwuPet::cure()
{
    if (life_ == LifeState::Dead || !sick_) {
        return;
    }
    const float sickHunger = sickHungerThreshold();
    const float sickWeight = sickWeightThreshold();
    if (stats_.hunger < sickHunger) {
        stats_.hunger = clamp01(sickHunger + kCureHungerMargin); // clear hunger-caused illness
    }
    if (stats_.weightKg > sickWeight) {
        stats_.weightKg = std::max(kMinWeightKg, sickWeight - kCureWeightMargin); // clear weight-caused illness
    }
    refreshDerivedState();
    dirty_ = true;
}

// Credits gold coins up to the save-format cap (kMaxCoins).
void VaiwuPet::addCoins(std::uint32_t amount)
{
    stats_.coins += amount;
    if (stats_.coins > kMaxCoins) {
        stats_.coins = kMaxCoins;
    }
    dirty_ = true;
}

// Deducts gold coins if the pet has enough; returns false without mutating state otherwise.
bool VaiwuPet::spendCoins(std::uint32_t amount)
{
    if (stats_.coins < amount) {
        return false;
    }
    stats_.coins -= amount;
    dirty_ = true;
    return true;
}

// Applies a shop consumable's stat deltas and wakes a sleeping pet if energy reaches 100%.
void VaiwuPet::applyGift(float hunger, float happiness, float energy, float weightDelta)
{
    if (life_ == LifeState::Dead) {
        return;
    }
    stats_.hunger = clamp01(stats_.hunger + hunger);
    addHappiness(happiness);
    stats_.energy = clamp01(stats_.energy + energy);
    stats_.weightKg = std::clamp(stats_.weightKg + weightDelta, kMinWeightKg, kMaxWeightKg);
    if (life_ == LifeState::Sleeping && stats_.energy >= kStatMax) {
        life_ = LifeState::Awake;
        wakeGraceMinutes_ = 0.0f;
    }
    refreshDerivedState();
    dirty_ = true;
}

// Returns how many units of consumable [itemId] are carried in the bag.
int VaiwuPet::consumableCount(int itemId) const
{
    if (itemId < 0 || itemId >= kConsumableSlots) {
        return 0;
    }
    return consumables_[itemId];
}

// Adds [amount] units of [itemId] to the bag up to kMaxConsumableStack (99).
bool VaiwuPet::addConsumable(int itemId, int amount)
{
    if (itemId < 0 || itemId >= kConsumableSlots || amount <= 0) {
        return false;
    }
    const int current = consumables_[itemId];
    const int next = std::min(kMaxConsumableStack, current + amount);
    if (next == current) {
        return false; // bag slot is full
    }
    consumables_[itemId] = static_cast<std::uint8_t>(next);
    dirty_ = true;
    return true;
}

// Consumes one unit of [itemId] from the bag; returns false if empty or out of bounds.
bool VaiwuPet::takeConsumable(int itemId)
{
    if (itemId < 0 || itemId >= kConsumableSlots || consumables_[itemId] == 0) {
        return false;
    }
    --consumables_[itemId];
    dirty_ = true;
    return true;
}

// Maps current weight in kg to the visual BodyShape tier.
BodyShape VaiwuPet::bodyShape() const
{
    if (stats_.weightKg < kThinMaxWeightKg) return BodyShape::Thin;
    if (stats_.weightKg < kNormalMaxWeightKg) return BodyShape::Normal;
    if (stats_.weightKg < kChubbyMaxWeightKg) return BodyShape::Chubby;
    return BodyShape::Fat;
}

// Serializes the complete pet, RPG, and bag state into the V6 text save format.
std::string VaiwuPet::serialize() const
{
    if (!started_) {
        return {};
    }
    char buf[512];
    // V6 = V5 + the carried-consumables bag (kConsumableSlots counts indexed
    // by ShopItem id, appended after the mana-potion count).
    std::snprintf(buf, sizeof(buf), "V6 %d %.2f %.2f %.2f %.2f %.2f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d %u %d %d %d %d",
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
    std::string out(buf);
    for (int i = 0; i < kConsumableSlots; ++i) {
        out += ' ';
        out += std::to_string(static_cast<int>(consumables_[i]));
    }
    out += '\n';
    return out + name_;
}

// Parses and validates a V1..V6 save payload, clamping all stats and RPG fields against corruption.
bool VaiwuPet::deserialize(const std::string& data, std::uint64_t nowMs)
{
    int charId = 0;
    float hunger = 0, happiness = 0, energy = 0, discipline = 0, weight = 0;
    unsigned age = 0;
    int life = 0, sick = 0;
    unsigned coins = 0;
    RpgProgress rpg{};
    unsigned owned = 0;
    int carried[kConsumableSlots] = {};
    // V6 adds the carried-consumables bag; V5 (mana potions), V4 (equipment),
    // V3 (RPG), V2 (coins) and V1 saves are still accepted.
    int parsed = std::sscanf(data.c_str(),
        "V6 %d %f %f %f %f %f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d %u %d %d %d %d"
        " %d %d %d %d %d %d %d %d %d %d %d %d",
        &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick, &coins,
        &rpg.level, &rpg.xp, &rpg.statPoints,
        &rpg.alloc.str, &rpg.alloc.vit, &rpg.alloc.agi, &rpg.alloc.mag, &rpg.alloc.luck,
        &rpg.hp, &rpg.mp, &rpg.potions,
        &owned, &rpg.equipped[0], &rpg.equipped[1], &rpg.equipped[2], &rpg.mpPotions,
        &carried[0], &carried[1], &carried[2], &carried[3], &carried[4], &carried[5],
        &carried[6], &carried[7], &carried[8], &carried[9], &carried[10], &carried[11]);
    if (parsed == kV6FieldCount) {
        rpg.equipOwned = owned;
    } else {
        rpg = RpgProgress{};
        owned = 0;
        parsed = std::sscanf(data.c_str(),
            "V5 %d %f %f %f %f %f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d %u %d %d %d %d",
            &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick, &coins,
            &rpg.level, &rpg.xp, &rpg.statPoints,
            &rpg.alloc.str, &rpg.alloc.vit, &rpg.alloc.agi, &rpg.alloc.mag, &rpg.alloc.luck,
            &rpg.hp, &rpg.mp, &rpg.potions,
            &owned, &rpg.equipped[0], &rpg.equipped[1], &rpg.equipped[2], &rpg.mpPotions);
        if (parsed == kV5FieldCount) {
            rpg.equipOwned = owned;
        } else {
            rpg = RpgProgress{};
            owned = 0;
            parsed = std::sscanf(data.c_str(),
                "V4 %d %f %f %f %f %f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d %u %d %d %d",
                &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick, &coins,
                &rpg.level, &rpg.xp, &rpg.statPoints,
                &rpg.alloc.str, &rpg.alloc.vit, &rpg.alloc.agi, &rpg.alloc.mag, &rpg.alloc.luck,
                &rpg.hp, &rpg.mp, &rpg.potions,
                &owned, &rpg.equipped[0], &rpg.equipped[1], &rpg.equipped[2]);
            if (parsed == kV4FieldCount) {
                rpg.equipOwned = owned;
            } else {
                rpg = RpgProgress{};
                parsed = std::sscanf(data.c_str(),
                    "V3 %d %f %f %f %f %f %u %d %d %u %d %d %d %d %d %d %d %d %d %d %d",
                    &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life, &sick, &coins,
                    &rpg.level, &rpg.xp, &rpg.statPoints,
                    &rpg.alloc.str, &rpg.alloc.vit, &rpg.alloc.agi, &rpg.alloc.mag, &rpg.alloc.luck,
                    &rpg.hp, &rpg.mp, &rpg.potions);
                if (parsed != kV3FieldCount) {
                    rpg = RpgProgress{};
                    parsed = std::sscanf(data.c_str(), "V2 %d %f %f %f %f %f %u %d %d %u",
                        &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life,
                        &sick, &coins);
                    if (parsed != kV2FieldCount) {
                        coins = 0;
                        parsed = std::sscanf(data.c_str(), "V1 %d %f %f %f %f %f %u %d %d",
                            &charId, &hunger, &happiness, &energy, &discipline, &weight, &age, &life,
                            &sick);
                        if (parsed != kV1FieldCount) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    const std::size_t nl = data.find('\n');
    if (nl == std::string::npos || nl + 1 >= data.size()) {
        return false;
    }
    if (charId < 0 || charId >= kCharacterCount || life < 0 || life > kMaxLifeStateId) {
        return false;
    }
    if (!std::isfinite(hunger) || !std::isfinite(happiness) || !std::isfinite(energy)
        || !std::isfinite(discipline) || !std::isfinite(weight)) {
        return false;
    }

    character_ = static_cast<CharacterId>(charId);
    std::string loadedName = data.substr(nl + 1);
    loadedName.erase(std::remove_if(loadedName.begin(), loadedName.end(),
        [](char c) { return c == '\r' || c == '\n'; }), loadedName.end());
    if (loadedName.size() > kMaxPetNameBytes) {
        loadedName.resize(kMaxPetNameBytes);
    }
    name_ = loadedName.empty() ? traitsFor(character_).name : std::move(loadedName);
    stats_.hunger = clamp01(hunger);
    stats_.happiness = clamp01(happiness);
    stats_.energy = clamp01(energy);
    stats_.discipline = clamp01(discipline);
    stats_.weightKg = std::clamp(weight, kMinWeightKg, kMaxWeightKg);
    stats_.ageSeconds = age;
    stats_.coins = std::min(coins, kMaxCoins);
    life_ = static_cast<LifeState>(life);
    sick_ = sick != 0;
    rpg_ = rpg;
    normalizeRpg(character_, rpg_);
    for (int i = 0; i < kConsumableSlots; ++i) {
        consumables_[i] = static_cast<std::uint8_t>(std::clamp(carried[i], 0, kMaxConsumableStack));
    }
    rpgRegenPool_ = 0.0f;
    wakeGraceMinutes_ = 0.0f;
    started_ = true;
    dirty_ = false;
    lastTickMs_ = nowMs;
    refreshDerivedState();
    return true;
}

} // namespace vaiwu::game

#pragma once

#include "vaiwu/game/Characters.hpp"
#include "vaiwu/game/Rpg.hpp"

#include <cstdint>
#include <string>

namespace vaiwu::game {

enum class LifeState : std::uint8_t {
    Awake = 0,
    Sleeping = 1,
    Dead = 2,
};

// What the character visually shows right now. Derived from stats each
// tick; never stored directly (except Dead, which is sticky via LifeState).
enum class Emotion : std::uint8_t {
    Idle = 0,
    Happy,
    Sad,
    Angry,
    Sleepy,
    Sick,
    Dead,
};

// Visual body shape derived from weight: the sprite is drawn narrower or
// wider so overfeeding (or over-playing) is visible at a glance.
enum class BodyShape : std::uint8_t {
    Thin = 0,   // weight < 10 kg
    Normal,     // 10..39 kg
    Chubby,     // 40..64 kg
    Fat,        // >= 65 kg (sickness threshold is 80 kg)
};

enum class ActionResult : std::uint8_t {
    Ok = 0,
    RefusedDead,     // no reaction, the pet is gone
    RefusedSleeping, // Zzz... come back later
    RefusedCaprice,  // discipline < 20: refuses food (spec: "капризы")
    NotHungry,       // hunger already full
    TooTired,        // not enough energy to play
};

struct PetStats {
    float hunger{100.0f};      // 0..100, 0 = death
    float happiness{80.0f};    // 0..100
    float energy{100.0f};      // 0..100
    float discipline{60.0f};   // 0..100
    float weightKg{15.0f};     // 1..99
    std::uint32_t ageSeconds{0};
    std::uint32_t coins{0};   // gold earned from mini-games, spent in the shop
};

/**
 * Purpose: the whole tamagotchi simulation for one pet.
 * Responsibilities: advance stats over time (tick), apply user actions with
 *                    per-character trait modifiers, derive the current
 *                    emotion/state, and (de)serialize itself for storage.
 *                    Pure C++ - no ESP-IDF or LVGL dependencies, so it is
 *                    host-testable (see tests/test_pet.cpp).
 * Public API: startNew(), tick(), feed(), startPlay(), applyPlayResult(),
 *             wash(), toilet(), talk(), emotion(), life(), isSick(),
 *             stats(), serialize(), deserialize().
 */
class VaiwuPet {
public:
    void startNew(CharacterId id, const std::string& name, std::uint64_t nowMs);

    // Advances the simulation by the wall-clock delta since the last tick.
    // Safe to call at any rate; typically once per second from a UI timer.
    void tick(std::uint64_t nowMs);

    [[nodiscard]] ActionResult feed();
    // startPlay() only checks preconditions and pays the energy/weight cost;
    // the happiness outcome depends on the mini-game result reported via
    // applyPlayResult().
    [[nodiscard]] ActionResult startPlay();
    void applyPlayResult(bool success);
    [[nodiscard]] ActionResult wash();
    [[nodiscard]] ActionResult toilet();
    [[nodiscard]] ActionResult talk();

    // Dialog-screen extras (spec screen 3): small mood boosts for the
    // "Hug"/"Pat" replies and a light snack for "Treat" that bypasses the
    // caprice rule (she never refuses a treat) but still gains weight.
    void giveAffection(float happinessAmount);
    [[nodiscard]] ActionResult treat();

    // Economy: gold coins earned from winning mini-games and spent in the shop.
    [[nodiscard]] std::uint32_t coins() const { return stats_.coins; }
    void addCoins(std::uint32_t amount);
    [[nodiscard]] bool spendCoins(std::uint32_t amount);
    // Apply a purchased item's effect (a positive stat gift). weightDelta may
    // be negative (e.g. medicine that also trims weight).
    void applyGift(float hunger, float happiness, float energy, float weightDelta);

    // Cure illness: fixes whichever cause is active (raises hunger above the
    // sickness threshold and/or trims excess weight below it), so isSick()
    // becomes false. No effect if the pet is not sick.
    void cure();

    // Tap on the sprite while she sleeps: she wakes up (a little grumpy).
    // Returns true if she was actually sleeping.
    bool wake();

    [[nodiscard]] Emotion emotion() const;
    [[nodiscard]] BodyShape bodyShape() const;
    [[nodiscard]] LifeState life() const { return life_; }
    [[nodiscard]] bool isSick() const { return sick_; }
    [[nodiscard]] const PetStats& stats() const { return stats_; }
    [[nodiscard]] CharacterId character() const { return character_; }
    [[nodiscard]] const CharacterTraits& traits() const { return traitsFor(character_); }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] bool alive() const { return life_ != LifeState::Dead; }

    // True when state changed since the last clearDirty() - used by the UI
    // to autosave only when something actually happened.
    [[nodiscard]] bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    // Single-line-safe text form (name goes last, after a '\n', so it may
    // contain spaces). Returns empty string if the pet was never started.
    [[nodiscard]] std::string serialize() const;
    [[nodiscard]] bool deserialize(const std::string& data, std::uint64_t nowMs);

    // Fantasy-RPG progress (levels, allocated stats, HP/MP, potions). The
    // battle engine (Rpg.hpp battleTurn) mutates it through this reference;
    // it is persisted as part of the V3 save format.
    [[nodiscard]] RpgProgress& rpg() { return rpg_; }
    [[nodiscard]] const RpgProgress& rpg() const { return rpg_; }

private:
    void applyDecay(float minutes);
    void refreshDerivedState();
    void addHappiness(float amount);
    // RPG characteristics feed back into the life sim (spec): only the
    // points ABOVE the character's base (allocated + equipment) count, so
    // a fresh pet behaves exactly like before the RPG existed.
    [[nodiscard]] RpgStats rpgBonusPoints() const;
    [[nodiscard]] float sickHungerThreshold() const;
    [[nodiscard]] float sickWeightThreshold() const;

    CharacterId character_{CharacterId::Aoi};
    std::string name_{};
    PetStats stats_{};
    LifeState life_{LifeState::Awake};
    bool sick_{false};
    bool started_{false};
    bool dirty_{false};
    std::uint64_t lastTickMs_{0};
    std::uint64_t ageMsRemainder_{0};
    RpgProgress rpg_{};
    float rpgRegenPool_{0.0f}; // minutes accumulated toward HP/MP regen
};

} // namespace vaiwu::game

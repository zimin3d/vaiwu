#pragma once

#include <cstdint>

namespace vaiwu::game {

enum class CharacterId : std::uint8_t {
    Aoi = 0,
    Hina = 1,
    Rin = 2,
    Yuki = 3,
    Akane = 4,   // furry fox-girl (kitsune)
    Unicorn = 5, // magical unicorn girl (horn + pastel mane)
    Kolobok = 6, // round bread bun from the Russian folk tale
    Vesper = 7,  // adult demoness girl (30+), playful and light flirt
};

constexpr int kCharacterCount = 8;

// Static personality data for the four preset characters (see README).
// All multipliers are relative to the baseline decay/gain rates in
// VaiwuPet.cpp; 1.0f means "no special trait for this stat".
struct CharacterTraits {
    const char* name;          // default display name (ASCII, fits UI font)
    const char* personality;   // short English description used in the AI prompt
    const char* favoriteFood;  // shown on the select screen and used in prompts
    float happinessDecayMult;  // Aoi loses happiness slower
    float energyDecayMult;     // Hina burns energy faster
    float playHappinessMult;   // Hina gets a x1.3 happiness bonus from games
    float happinessGainMult;   // Yuki is harder to cheer up
    float sleepRegenMult;      // Aoi regenerates energy faster while asleep
    bool sicknessResistant;    // Yuki rarely gets sick
    bool angryWhenHungry;      // Rin (tsundere) gets angry when underfed
    std::uint32_t accentColor; // UI accent (ribbon/badge), 0xRRGGBB
};

[[nodiscard]] const CharacterTraits& traitsFor(CharacterId id);

} // namespace vaiwu::game

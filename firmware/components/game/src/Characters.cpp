#include "vaiwu/game/Characters.hpp"

namespace vaiwu::game {

namespace {
constexpr CharacterTraits kTraits[kCharacterCount] = {
    // name    personality                                   food       hapDec enDec playHap hapGain slpReg sickRes angry  accent
    {"Aoi", "calm, melancholic, gentle and a bit dreamy", "rice", 0.70f, 1.00f, 1.00f, 1.00f, 1.50f, false, false, 0x4FA3E0},
    {"Hina", "energetic, cheerful, loves games and jokes", "ramen", 1.00f, 1.30f, 1.30f, 1.00f, 1.00f, false, false, 0xF2A03D},
    {"Rin", "strict tsundere, hides warm feelings", "sweets", 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, false, true, 0xE05F6B},
    {"Yuki", "mysterious, quiet, speaks in short phrases", "anything", 1.00f, 1.00f, 1.00f, 0.60f, 1.00f, true, false, 0xB48FE0},
    // Akane: a playful, mischievous furry fox-girl (kitsune) who loves fried
    // tofu, pranks and games. Cheers up easily and gets a big kick out of
    // playing, but is a restless bundle of energy that tires quickly.
    {"Akane", "playful mischievous fox-girl, loves pranks and games", "tofu", 1.00f, 1.25f, 1.25f, 1.15f, 1.00f, false, false, 0xE8833A},
    // Uni: a dreamy unicorn girl who believes in rainbows and miracles.
    // Naturally serene (loses happiness slowly, cheers up easily) and
    // recharges especially well while asleep, dreaming of starlit meadows.
    {"Uni", "dreamy magical unicorn girl, gentle, believes in rainbows and miracles", "cotton candy", 0.80f, 1.00f, 1.00f, 1.20f, 1.30f, false, false, 0xC77DF2},
    // Kolobok: the boastful runaway bread bun from the Russian folk tale.
    // Rolls everywhere without tiring and sings his little song, but sulks
    // hard when underfed (a hungry bun is an angry bun).
    {"Kolobok", "jolly boastful round bread bun from a Russian folk tale, rolls everywhere, sings, brags about escaping everyone", "smetana", 1.00f, 0.80f, 1.10f, 1.00f, 1.00f, false, true, 0xF2C14E},
    // Vesper: an adult (30+) demoness who is playful and teasing, loves cozy
    // nights and attention. Flirty in a light way, never suggestive.
    {"Vesper", "playful, teasing demoness who loves cozy nights and attention, flirty in a light way", "wine", 0.95f, 1.15f, 1.25f, 1.20f, 0.90f, false, true, 0x9D4EDD},
};
} // namespace

const CharacterTraits& traitsFor(CharacterId id)
{
    const auto index = static_cast<unsigned>(id);
    return kTraits[index < kCharacterCount ? index : 0U];
}

} // namespace vaiwu::game

#pragma once

#include "vaiwu/game/Characters.hpp"

#include <cstdint>
#include <vector>

namespace vaiwu::game {

// Sound effect IDs for the whole game (spec asks for 5+; we ship 19).
enum class Sfx : std::uint8_t {
    Tap = 0,
    Feed,
    PlayStart,
    Win,
    Lose,
    Wash,
    Toilet,
    Talk,
    Sick,
    Death,
    Note1, // melody-game pads: C5 E5 G5 C6
    Note2,
    Note3,
    Note4,
    Alarm, // repeating alarm/reminder jingle
    Hit,   // battle: a plain weapon hit lands
    Crit,  // battle: critical / Focus strike
    Magic, // battle: a skill (spell, drain, heal...) goes off
    Hurt,  // battle: the foe strikes back
    Encounter, // battle: a monster shows up near the house
};

/**
 * Purpose: tiny chiptune synthesizer for UI/game sound effects.
 * Responsibilities: render a named effect into raw little-endian PCM16 mono
 *                    samples at the given sample rate (square wave with a
 *                    short decay envelope - no assets needed on flash or SD).
 *                    Pure C++, host-testable.
 * Public API: renderSfx().
 */
[[nodiscard]] std::vector<std::uint8_t> renderSfx(Sfx effect, std::uint32_t sampleRateHz);

// ---- babble voice ("Animal Crossing" style) ---------------------------------
// Every phrase can be "spoken" without any network or audio assets: the text
// is turned into a short chirping melody - one blip per vowel-ish letter,
// pitch wobbling around the character's own base tone. It is gibberish by
// design (no language model, no phonemes), but it reads as that character
// talking and works fully offline.
struct VoiceProfile {
    float basePitchHz{600.0f}; // center pitch of the babble
    float spread{0.25f};       // how far single blips wander from the base
    std::uint16_t blipMs{55};  // length of one blip
    std::uint16_t gapMs{16};   // silence between blips
    bool soft{false};          // triangle-ish timbre instead of a square wave
};

[[nodiscard]] VoiceProfile voiceProfileFor(CharacterId id);

// Renders at most kVoiceMaxBlips blips (long phrases are truncated so a
// single line never blocks the audio task for seconds).
constexpr int kVoiceMaxBlips = 14;

[[nodiscard]] std::vector<std::uint8_t> renderVoice(
    const char* utf8, const VoiceProfile& profile, std::uint32_t sampleRateHz);

} // namespace vaiwu::game

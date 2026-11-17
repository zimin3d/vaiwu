#include "vaiwu/game/SoundSynth.hpp"

#include <cmath>

namespace vaiwu::game {

namespace {
struct Note {
    float freqHz;    // 0 = rest
    std::uint16_t ms;
};

constexpr int kMaxNotes = 6;

struct Melody {
    Note notes[kMaxNotes];
};

// Frequencies are rough note values; each effect is a short recognizable jingle.
[[nodiscard]] Melody melodyFor(Sfx effect)
{
    switch (effect) {
    case Sfx::Tap:       return {{{1200.0f, 30}, {0, 0}}};
    case Sfx::Feed:      return {{{523.3f, 70}, {659.3f, 70}, {784.0f, 110}, {0, 0}}};
    case Sfx::PlayStart: return {{{659.3f, 60}, {880.0f, 60}, {1174.7f, 90}, {0, 0}}};
    case Sfx::Win:       return {{{784.0f, 80}, {987.8f, 80}, {1318.5f, 160}, {0, 0}}};
    case Sfx::Lose:      return {{{392.0f, 120}, {311.1f, 120}, {233.1f, 200}, {0, 0}}};
    case Sfx::Wash:      return {{{880.0f, 50}, {1108.7f, 50}, {880.0f, 50}, {1108.7f, 50}, {0, 0}}};
    case Sfx::Toilet:    return {{{440.0f, 60}, {349.2f, 100}, {0, 0}}};
    case Sfx::Talk:      return {{{659.3f, 50}, {0.0f, 30}, {659.3f, 50}, {830.6f, 90}, {0, 0}}};
    case Sfx::Sick:      return {{{330.0f, 90}, {0.0f, 40}, {311.1f, 90}, {0.0f, 40}, {293.7f, 140}, {0, 0}}};
    case Sfx::Death:     return {{{392.0f, 200}, {370.0f, 200}, {349.2f, 200}, {329.6f, 400}, {0, 0}}};
    case Sfx::Note1:     return {{{523.3f, 220}, {0, 0}}};
    case Sfx::Note2:     return {{{659.3f, 220}, {0, 0}}};
    case Sfx::Note3:     return {{{784.0f, 220}, {0, 0}}};
    case Sfx::Note4:     return {{{1046.5f, 220}, {0, 0}}};
    case Sfx::Alarm:     return {{{880.0f, 120}, {0.0f, 60}, {880.0f, 120}, {0.0f, 60}, {1174.7f, 240}, {0, 0}}};
    // Battle set: short and punchy so a long fight never gets tiring.
    case Sfx::Hit:       return {{{740.0f, 40}, {493.9f, 60}, {0, 0}}};
    case Sfx::Crit:      return {{{1046.5f, 40}, {1318.5f, 40}, {1568.0f, 90}, {0, 0}}};
    case Sfx::Magic:     return {{{659.3f, 45}, {880.0f, 45}, {1174.7f, 45}, {1568.0f, 110}, {0, 0}}};
    case Sfx::Hurt:      return {{{311.1f, 70}, {233.1f, 110}, {0, 0}}};
    case Sfx::Encounter: return {{{440.0f, 90}, {0.0f, 40}, {415.3f, 90}, {0.0f, 40}, {392.0f, 180}, {0, 0}}};
    }
    return {{{0, 0}}};
}
} // namespace

std::vector<std::uint8_t> renderSfx(Sfx effect, std::uint32_t sampleRateHz)
{
    const Melody melody = melodyFor(effect);
    std::vector<std::uint8_t> pcm;

    for (const Note& note : melody.notes) {
        if (note.ms == 0) {
            break;
        }
        const std::size_t sampleCount = static_cast<std::size_t>(sampleRateHz) * note.ms / 1000U;
        pcm.reserve(pcm.size() + sampleCount * 2U);

        if (note.freqHz <= 0.0f) { // rest
            for (std::size_t i = 0; i < sampleCount; ++i) {
                pcm.push_back(0);
                pcm.push_back(0);
            }
            continue;
        }

        const float period = static_cast<float>(sampleRateHz) / note.freqHz;
        for (std::size_t i = 0; i < sampleCount; ++i) {
            // Square wave with a linear decay envelope to avoid harsh clicks.
            const float phase = std::fmod(static_cast<float>(i), period) / period;
            const float envelope = 1.0f - static_cast<float>(i) / static_cast<float>(sampleCount);
            const float value = (phase < 0.5f ? 1.0f : -1.0f) * envelope * 0.35f;
            const auto sample = static_cast<std::int16_t>(value * 32767.0f);
            pcm.push_back(static_cast<std::uint8_t>(sample & 0xFF));
            pcm.push_back(static_cast<std::uint8_t>((sample >> 8) & 0xFF));
        }
    }
    return pcm;
}


// ---- babble voice ------------------------------------------------------------

namespace {

// Rough "is this a vowel" test for the two alphabets the game speaks. Only
// vowels get a blip, which makes the babble follow the rhythm of the text
// (short words chirp, long ones warble) instead of machine-gunning.
bool isVowelAscii(char c)
{
    switch (c | 0x20) { // lowercase
    case 'a': case 'e': case 'i': case 'o': case 'u': case 'y':
        return true;
    default:
        return false;
    }
}

// Cyrillic vowels live in the two-byte UTF-8 range 0x0410-0x044F (+ 0x0401
// / 0x0451 for Ё). Decoding just enough to spot them keeps this dependency
// free and host-testable.
bool isVowelCyrillic(std::uint32_t code)
{
    switch (code) {
    case 0x0410: case 0x0415: case 0x0401: case 0x0418: case 0x041E:
    case 0x0423: case 0x042B: case 0x042D: case 0x042E: case 0x042F:
    case 0x0430: case 0x0435: case 0x0451: case 0x0438: case 0x043E:
    case 0x0443: case 0x044B: case 0x044D: case 0x044E: case 0x044F:
        return true;
    default:
        return false;
    }
}

} // namespace

VoiceProfile voiceProfileFor(CharacterId id)
{
    // Pitch and pacing follow each character's personality.
    switch (id) {
    case CharacterId::Hina:    return {760.0f, 0.30f, 42, 10, false}; // fast, bright
    case CharacterId::Rin:     return {560.0f, 0.18f, 52, 14, false}; // clipped, firm
    case CharacterId::Yuki:    return {480.0f, 0.12f, 70, 26, true};  // slow, soft
    case CharacterId::Akane:   return {700.0f, 0.34f, 46, 12, false}; // playful
    case CharacterId::Unicorn: return {820.0f, 0.22f, 58, 18, true};  // airy, high
    case CharacterId::Kolobok: return {380.0f, 0.20f, 60, 14, false}; // round, low
    case CharacterId::Vesper:  return {600.0f, 0.24f, 54, 16, true};  // husky, teasing
    case CharacterId::Aoi:
    default:                   return {620.0f, 0.16f, 60, 20, true};  // calm
    }
}

std::vector<std::uint8_t> renderVoice(
    const char* utf8, const VoiceProfile& profile, std::uint32_t sampleRateHz)
{
    std::vector<std::uint8_t> pcm;
    if (utf8 == nullptr || sampleRateHz == 0U) {
        return pcm;
    }

    // Collect the blip pitches first: deterministic wobble derived from the
    // characters themselves, so the same phrase always sounds the same.
    float pitches[kVoiceMaxBlips];
    int blips = 0;
    std::uint32_t hash = 2166136261U;
    for (const char* p = utf8; *p != '\0' && blips < kVoiceMaxBlips; ++p) {
        const auto byte = static_cast<std::uint8_t>(*p);
        hash = (hash ^ byte) * 16777619U;
        bool vowel = false;
        if (byte < 0x80U) {
            vowel = isVowelAscii(*p);
        } else if ((byte & 0xE0U) == 0xC0U && p[1] != '\0') {
            const std::uint32_t code = ((byte & 0x1FU) << 6)
                | (static_cast<std::uint8_t>(p[1]) & 0x3FU);
            vowel = isVowelCyrillic(code);
            ++p; // the trailing byte belongs to this character
        }
        if (!vowel) {
            continue;
        }
        // Wobble between -spread and +spread around the base pitch.
        const float wobble = (static_cast<float>((hash >> 8) % 200U) / 100.0f) - 1.0f;
        pitches[blips++] = profile.basePitchHz * (1.0f + profile.spread * wobble);
    }
    if (blips == 0) { // punctuation-only lines ("...") still get one soft blip
        pitches[blips++] = profile.basePitchHz * 0.9f;
    }

    const std::size_t blipSamples =
        static_cast<std::size_t>(sampleRateHz) * profile.blipMs / 1000U;
    const std::size_t gapSamples =
        static_cast<std::size_t>(sampleRateHz) * profile.gapMs / 1000U;
    pcm.reserve((blipSamples + gapSamples) * static_cast<std::size_t>(blips) * 2U);

    for (int b = 0; b < blips; ++b) {
        const float period = static_cast<float>(sampleRateHz) / pitches[b];
        for (std::size_t i = 0; i < blipSamples; ++i) {
            const float phase = std::fmod(static_cast<float>(i), period) / period;
            // Bell-ish envelope: quick attack, gentle decay - avoids clicks
            // and makes the blips sound like syllables rather than beeps.
            const float t = static_cast<float>(i) / static_cast<float>(blipSamples);
            const float envelope = t < 0.15f ? t / 0.15f : (1.0f - (t - 0.15f) / 0.85f);
            const float wave = profile.soft
                ? (phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase)) // triangle
                : (phase < 0.5f ? 1.0f : -1.0f);                                 // square
            const float value = wave * envelope * 0.30f;
            const auto sample = static_cast<std::int16_t>(value * 32767.0f);
            pcm.push_back(static_cast<std::uint8_t>(sample & 0xFF));
            pcm.push_back(static_cast<std::uint8_t>((sample >> 8) & 0xFF));
        }
        for (std::size_t i = 0; i < gapSamples; ++i) {
            pcm.push_back(0);
            pcm.push_back(0);
        }
    }
    return pcm;
}

} // namespace vaiwu::game

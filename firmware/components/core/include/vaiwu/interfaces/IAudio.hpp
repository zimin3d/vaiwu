#pragma once

#include <cstdint>
#include <vector>

namespace vaiwu {

// Raw little-endian PCM16 mono samples at kAudioSampleRateHz. This is the
// single format contract every IAudio implementation (mock or real) and
// every caller (AssistantController's boot chime, future TTS output) agrees
// on, so buffers never need per-backend conversion.
using AudioBuffer = std::vector<std::uint8_t>;
constexpr std::uint32_t kAudioSampleRateHz = 24000;

/**
 * Purpose: abstracts microphone capture and speaker playback.
 * Responsibilities: initialize audio hardware, capture voice, play generated audio,
 *                    and expose an adjustable output volume.
 * Public API: init(), startCapture(), stopCapture(), play(), setVolume().
 */
class IAudio {
public:
    virtual ~IAudio() = default;

    [[nodiscard]] virtual bool init() = 0;
    [[nodiscard]] virtual bool startCapture() = 0;
    [[nodiscard]] virtual AudioBuffer stopCapture() = 0;
    [[nodiscard]] virtual bool play(const AudioBuffer& buffer) = 0;
    // percent is 0-100; implementations clamp out-of-range input.
    virtual void setVolume(std::uint8_t percent) = 0;
};

} // namespace vaiwu

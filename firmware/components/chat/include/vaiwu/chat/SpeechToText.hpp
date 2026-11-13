#pragma once

#include "vaiwu/game/I18n.hpp"
#include "vaiwu/interfaces/ILogger.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace vaiwu::chat {

// Builds a minimal 44-byte PCM16 mono WAV header for the given payload.
// Exposed (and pure) so it can be unit-tested on the host without any
// networking. Returns the full WAV file bytes (header + pcm).
[[nodiscard]] std::vector<std::uint8_t> makeWav(const std::vector<std::uint8_t>& pcm,
    std::uint32_t sampleRateHz);

/**
 * Purpose: cloud speech-to-text for voice commands (hybrid design: the
 *          on-device wake word triggers a recording, this turns that
 *          recording into text, which then flows into the same intent
 *          parser / assistant chat as typed input).
 * Responsibilities: wrap PCM16 mono audio in a WAV container and POST it as
 *                    multipart/form-data to an OpenAI-compatible Whisper
 *                    transcription endpoint, returning the recognized text
 *                    (empty on any failure). Blocking; call from a worker
 *                    task, never the UI/LVGL task.
 * Public API: transcribe().
 */
class SpeechToText {
public:
    explicit SpeechToText(ILogger& logger);

    // pcm is raw little-endian PCM16 mono at sampleRateHz. lang is a hint
    // passed to the API ("ru"/"en"). Returns recognized text, or "" if STT
    // is not configured (no key), offline, or the request failed.
    [[nodiscard]] std::string transcribe(const std::vector<std::uint8_t>& pcm,
        std::uint32_t sampleRateHz, game::Lang lang);

    // True when an API key is compiled in (CONFIG_VAIWU_STT_API_KEY).
    [[nodiscard]] static bool configured();

private:
    ILogger& logger_;
#ifdef ESP_PLATFORM
    std::string responseBuffer_{};
#endif
};

} // namespace vaiwu::chat

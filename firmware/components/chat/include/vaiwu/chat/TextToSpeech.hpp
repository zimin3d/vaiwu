#pragma once

#include "vaiwu/game/I18n.hpp"
#include "vaiwu/interfaces/IAudio.hpp"
#include "vaiwu/interfaces/ILogger.hpp"

#include <string>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#endif

namespace vaiwu::chat {

/**
 * Purpose: speaks assistant/pet replies aloud (optional TTS skill), like
 *          xiaozhi's voice responses.
 * Responsibilities: own a worker task; the UI drops text to speak into a
 *                    queue (non-blocking). The worker POSTs it to an
 *                    OpenAI-compatible /audio/speech endpoint asking for raw
 *                    PCM16 mono at kAudioSampleRateHz (so no on-device audio
 *                    decoder is needed - matches the ES8311 playback rate),
 *                    strips a WAV header if the server sends one, and plays
 *                    it through IAudio. Disabled (no-op) when no TTS URL is
 *                    configured, so the firmware works unchanged without it.
 * Public API: init(), configured(), speak().
 */
class TextToSpeech {
public:
    TextToSpeech(ILogger& logger, IAudio& audio);

    [[nodiscard]] bool init();
    [[nodiscard]] static bool configured();

    // Queue one utterance (UTF-8). lang picks the request's language hint.
    // Non-blocking; silently ignored if TTS is not configured.
    void speak(const std::string& text, game::Lang lang);

private:
    ILogger& logger_;
    IAudio& audio_;

#ifdef ESP_PLATFORM
    struct Job {
        char text[320];
        int lang;
    };
    static void taskEntry(void* arg);
    void taskLoop();
    [[nodiscard]] bool synthesizeAndPlay(const Job& job);

    QueueHandle_t queue_{nullptr};
    std::string responseBuffer_{};
#endif
};

} // namespace vaiwu::chat

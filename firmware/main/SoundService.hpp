#pragma once

#include "vaiwu/game/Characters.hpp"
#include "vaiwu/game/SoundSynth.hpp"
#include "vaiwu/interfaces/IAudio.hpp"
#include "vaiwu/interfaces/ILogger.hpp"

#include <cstdio>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#endif

namespace vaiwu {

/**
 * Purpose: plays game sound effects without ever blocking the UI.
 * Responsibilities: own a small FreeRTOS queue + worker task; the UI drops
 *                    an Sfx id into the queue (non-blocking) and the worker
 *                    synthesizes the PCM on the fly and writes it to the
 *                    ES8311 codec. Effects that arrive while one is playing
 *                    queue up (up to 8), extras are dropped silently.
 * Public API: init(), play(), babble().
 */
class SoundService {
public:
    SoundService(ILogger& logger, IAudio& audio)
        : logger_(logger)
        , audio_(audio)
    {
    }

    [[nodiscard]] bool init()
    {
#ifdef ESP_PLATFORM
        queue_ = xQueueCreate(8, sizeof(int));
        if (queue_ == nullptr) {
            logger_.error("SoundService: queue creation failed");
            return false;
        }
        // Only one pending phrase: a new line replaces an unstarted one
        // instead of queueing a chorus.
        voiceQueue_ = xQueueCreate(1, sizeof(VoiceRequest));
        if (voiceQueue_ == nullptr) {
            logger_.error("SoundService: voice queue creation failed");
            return false;
        }
        if (xTaskCreate(&SoundService::taskEntry, "vaiwu_sfx", 4096, this, 2, nullptr) != pdPASS) {
            logger_.error("SoundService: task creation failed");
            return false;
        }
#endif
        return true;
    }

    void play(game::Sfx effect)
    {
#ifdef ESP_PLATFORM
        if (queue_ != nullptr) {
            int value = static_cast<int>(effect);
            static_cast<void>(xQueueSend(queue_, &value, 0));
        }
#else
        static_cast<void>(effect);
#endif
    }

    // "Speaks" a phrase in the character's own babble voice (offline, no
    // assets). The text is copied into the queue item, so the caller may
    // free it immediately; longer lines are truncated - renderVoice caps
    // the number of blips anyway.
    void babble(const char* text, game::CharacterId character)
    {
#ifdef ESP_PLATFORM
        if (voiceQueue_ == nullptr || text == nullptr) {
            return;
        }
        VoiceRequest request{};
        request.character = character;
        std::snprintf(request.text, sizeof(request.text), "%s", text);
        static_cast<void>(xQueueSend(voiceQueue_, &request, 0));
#else
        static_cast<void>(text);
        static_cast<void>(character);
#endif
    }

private:
#ifdef ESP_PLATFORM
    static void taskEntry(void* arg)
    {
        static_cast<SoundService*>(arg)->taskLoop();
    }

    void taskLoop()
    {
        for (;;) {
            // Effects have priority (they are instant feedback for a tap);
            // a queued phrase plays whenever no effect is waiting.
            int value = 0;
            if (xQueueReceive(queue_, &value, pdMS_TO_TICKS(20)) == pdTRUE) {
                const AudioBuffer pcm =
                    game::renderSfx(static_cast<game::Sfx>(value), kAudioSampleRateHz);
                if (!pcm.empty() && !audio_.play(pcm)) {
                    logger_.warn("SoundService: playback failed");
                }
                continue;
            }
            VoiceRequest request{};
            if (xQueueReceive(voiceQueue_, &request, pdMS_TO_TICKS(80)) != pdTRUE) {
                continue;
            }
            const AudioBuffer pcm = game::renderVoice(request.text,
                game::voiceProfileFor(request.character), kAudioSampleRateHz);
            if (!pcm.empty() && !audio_.play(pcm)) {
                logger_.warn("SoundService: voice playback failed");
            }
        }
    }

    struct VoiceRequest {
        game::CharacterId character{game::CharacterId::Aoi};
        char text[96]{};
    };

    QueueHandle_t queue_{nullptr};
    QueueHandle_t voiceQueue_{nullptr};
#endif

    ILogger& logger_;
    IAudio& audio_;
};

} // namespace vaiwu

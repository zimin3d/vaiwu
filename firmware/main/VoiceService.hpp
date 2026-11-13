#pragma once

#include "vaiwu/chat/SpeechToText.hpp"
#include "vaiwu/game/I18n.hpp"
#include "vaiwu/hal/esp/Es8311Audio.hpp"
#include "vaiwu/interfaces/ILogger.hpp"

#include <mutex>
#include <string>
#include <vector>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#endif

namespace vaiwu {

/**
 * Purpose: push-to-talk voice capture for the assistant (hybrid design's
 *          cloud half): records from the ES8311 microphone while the user
 *          holds the talk button, then transcribes via SpeechToText and
 *          hands the text to the UI, which routes it through the same intent
 *          parser / AI chat as typed input.
 * Responsibilities: own a worker task so neither microphone reads nor the
 *                    (blocking) STT HTTP request ever run on the LVGL task;
 *                    accumulate PCM while recording, cap the length, then
 *                    publish {state, transcript} through a mutex-guarded
 *                    mailbox the UI polls once per second.
 * Public API: init(), startRecording(), stopRecording(), state(),
 *             takeTranscript().
 *
 * The recognition language is chosen at record time so switching the UI
 * language also switches STT.
 */
class VoiceService {
public:
    enum class State : int {
        Idle = 0,
        Recording,
        Transcribing,
        Done,
        Error,
    };

    VoiceService(ILogger& logger, hal::esp::Es8311Audio& audio, chat::SpeechToText& stt)
        : logger_(logger)
        , audio_(audio)
        , stt_(stt)
    {
    }

    [[nodiscard]] bool available() const { return chat::SpeechToText::configured(); }

    [[nodiscard]] bool init()
    {
#ifdef ESP_PLATFORM
        wakeSem_ = xSemaphoreCreateBinary();
        if (wakeSem_ == nullptr) {
            logger_.error("VoiceService: failed to create semaphore");
            return false;
        }
        // 8 KB stack: TLS + multipart upload happen on this task.
        if (xTaskCreate(&VoiceService::taskEntry, "vaiwu_voice", 8192, this, 4, nullptr) != pdPASS) {
            logger_.error("VoiceService: failed to create worker task");
            return false;
        }
#endif
        return true;
    }

    void startRecording(game::Lang lang)
    {
#ifdef ESP_PLATFORM
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == State::Recording || state_ == State::Transcribing) {
                return;
            }
            lang_ = lang;
            wantRecord_ = true;
            state_ = State::Recording;
        }
        xSemaphoreGive(wakeSem_);
#else
        static_cast<void>(lang);
#endif
    }

    void stopRecording()
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        wantRecord_ = false;
    }

    [[nodiscard]] State state() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    // Returns the recognized text once (moving it out), then empty.
    [[nodiscard]] std::string takeTranscript()
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != State::Done) {
            return {};
        }
        state_ = State::Idle;
        std::string out = transcript_;
        transcript_.clear();
        return out;
    }

private:
#ifdef ESP_PLATFORM
    static constexpr std::size_t kChunkBytes = 2048;                 // ~43 ms @ 24 kHz
    static constexpr std::size_t kMaxBytes = 24000 * 2 * 6;          // 6 s cap
    static constexpr std::size_t kMinBytes = 24000 * 2 / 2;          // 0.5 s floor

    static void taskEntry(void* arg) { static_cast<VoiceService*>(arg)->taskLoop(); }

    void taskLoop()
    {
        std::vector<std::uint8_t> pcm;
        std::uint8_t chunk[kChunkBytes];
        for (;;) {
            if (xSemaphoreTake(wakeSem_, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            pcm.clear();
            pcm.reserve(kMaxBytes);

            for (;;) {
                {
                    const std::lock_guard<std::mutex> lock(mutex_);
                    if (!wantRecord_ || pcm.size() >= kMaxBytes) {
                        break;
                    }
                }
                if (audio_.readMic(chunk, sizeof(chunk))) {
                    pcm.insert(pcm.end(), chunk, chunk + sizeof(chunk));
                } else {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }

            game::Lang lang;
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                lang = lang_;
                state_ = State::Transcribing;
            }

            std::string text;
            if (pcm.size() >= kMinBytes) {
                text = stt_.transcribe(pcm, audio_.sampleRateHz(), lang);
            }

            {
                const std::lock_guard<std::mutex> lock(mutex_);
                transcript_ = text;
                state_ = text.empty() ? State::Error : State::Done;
            }
        }
    }

    SemaphoreHandle_t wakeSem_{nullptr};
#endif

    ILogger& logger_;
    hal::esp::Es8311Audio& audio_;
    chat::SpeechToText& stt_;
    mutable std::mutex mutex_;
    State state_{State::Idle};
    bool wantRecord_{false};
    game::Lang lang_{game::Lang::En};
    std::string transcript_{};
};

} // namespace vaiwu

#pragma once

#include "vaiwu/interfaces/ILogger.hpp"
#include "vaiwu/interfaces/IWeatherProvider.hpp"

#include <mutex>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#endif

namespace vaiwu {

/**
 * Purpose: fetches weather off the LVGL task for the assistant's "weather"
 *          skill (Open-Meteo, free, no key).
 * Responsibilities: run IWeatherProvider::refresh() (blocking HTTPS) on a
 *                    worker task when the UI asks, and publish the result
 *                    through a mutex-guarded mailbox the UI polls once a
 *                    second. State mirrors what the UI needs: 0 Idle,
 *                    1 Fetching, 2 Done, 3 Error.
 * Public API: init(), request(), state(), take().
 */
class WeatherService {
public:
    enum class State : int { Idle = 0, Fetching, Done, Error };

    WeatherService(ILogger& logger, IWeatherProvider& provider)
        : logger_(logger)
        , provider_(provider)
    {
    }

    [[nodiscard]] bool init()
    {
#ifdef ESP_PLATFORM
        wakeSem_ = xSemaphoreCreateBinary();
        if (wakeSem_ == nullptr) {
            logger_.error("WeatherService: semaphore creation failed");
            return false;
        }
        if (xTaskCreate(&WeatherService::taskEntry, "vaiwu_wx", 6144, this, 3, nullptr) != pdPASS) {
            logger_.error("WeatherService: task creation failed");
            return false;
        }
#endif
        return true;
    }

    void request()
    {
#ifdef ESP_PLATFORM
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == State::Fetching) {
                return;
            }
            state_ = State::Fetching;
        }
        xSemaphoreGive(wakeSem_);
#endif
    }

    [[nodiscard]] State state() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    // Returns the last reading once (after Done), then resets to Idle.
    [[nodiscard]] WeatherReading take()
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == State::Done || state_ == State::Error) {
            state_ = State::Idle;
        }
        return reading_;
    }

private:
#ifdef ESP_PLATFORM
    static void taskEntry(void* arg) { static_cast<WeatherService*>(arg)->taskLoop(); }

    void taskLoop()
    {
        for (;;) {
            if (xSemaphoreTake(wakeSem_, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            const bool ok = provider_.refresh();
            const WeatherReading reading = provider_.current();
            const std::lock_guard<std::mutex> lock(mutex_);
            reading_ = reading;
            state_ = (ok && reading.available) ? State::Done : State::Error;
        }
    }

    SemaphoreHandle_t wakeSem_{nullptr};
#endif

    ILogger& logger_;
    IWeatherProvider& provider_;
    mutable std::mutex mutex_;
    State state_{State::Idle};
    WeatherReading reading_{};
};

} // namespace vaiwu

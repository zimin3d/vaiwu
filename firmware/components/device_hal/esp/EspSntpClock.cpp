#include "vaiwu/hal/esp/EspSntpClock.hpp"

#include <ctime>

#ifdef ESP_PLATFORM
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include <cstdlib>
#endif

namespace vaiwu::hal::esp {

namespace {
#ifdef ESP_PLATFORM
// This project only targets a Moscow deployment (see docs/UI.md) - Moscow
// has been UTC+3 year-round (no DST) since 2014, so a fixed POSIX TZ string
// is enough; no timezone database lookup needed.
constexpr const char* kTimezone = "MSK-3";
constexpr const char* kNtpServer = "pool.ntp.org";
constexpr int kInitialSyncWaitMs = 5000;

// Anything before this (roughly Nov 2023) means the system clock is still at
// its power-on default, i.e. SNTP has not completed a sync yet. Checking the
// clock's own sanity is more robust across ESP-IDF versions than depending on
// a specific SNTP-internal status API.
constexpr std::time_t kSaneEpochThreshold = 1700000000;
#endif
} // namespace

EspSntpClock::EspSntpClock(ILogger& logger)
    : logger_(logger)
{
}

bool EspSntpClock::init()
{
#ifndef ESP_PLATFORM
    logger_.error("EspSntpClock requires ESP-IDF; not available on host builds");
    return false;
#else
    if (initialized_) {
        return true;
    }

    setenv("TZ", kTimezone, 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(kNtpServer);
    if (esp_netif_sntp_init(&config) != ESP_OK) {
        logger_.error("SNTP init failed");
        return false;
    }

    // Wait briefly for the first sync so an early render can show a real
    // clock instead of a placeholder; a timeout here is not fatal - SNTP
    // keeps retrying/resyncing in the background, and now()'s synced flag
    // will simply stay false until it eventually succeeds.
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(kInitialSyncWaitMs)) != ESP_OK) {
        logger_.warn("SNTP has not synced yet; clock will show as unsynced until it does");
    }

    initialized_ = true;
    return true;
#endif
}

ClockReading EspSntpClock::now() const
{
    ClockReading reading{};
#ifdef ESP_PLATFORM
    if (!initialized_) {
        return reading;
    }

    const std::time_t rawTime = std::time(nullptr);
    if (rawTime < kSaneEpochThreshold) {
        return reading; // synced stays false
    }

    std::tm localTm{};
    localtime_r(&rawTime, &localTm);
    reading.synced = true;
    reading.hour = localTm.tm_hour;
    reading.minute = localTm.tm_min;
    reading.day = localTm.tm_mday;
    reading.month = localTm.tm_mon + 1;
    reading.weekday = localTm.tm_wday;
#endif
    return reading;
}

} // namespace vaiwu::hal::esp

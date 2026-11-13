#pragma once

#include "vaiwu/interfaces/ILogger.hpp"
#include "vaiwu/interfaces/IWeatherProvider.hpp"

#include <string>

#ifdef ESP_PLATFORM
#include "esp_http_client.h"
#endif

namespace vaiwu::hal::esp {

/**
 * Purpose: fetches current weather for a fixed location from Open-Meteo
 *          (https://open-meteo.com), a free HTTPS API that needs no API key.
 * Responsibilities: perform the HTTPS GET (refresh()), parse the small JSON
 *                    response with cJSON, and map Open-Meteo's numeric WMO
 *                    weather code to a short display condition string. Only
 *                    exposes whatever the last refresh() produced; does not
 *                    fetch automatically on a timer itself (see main.cpp's
 *                    loop and docs/UI.md).
 * Public API: init(), refresh(), current().
 */
class OpenMeteoWeatherProvider final : public IWeatherProvider {
public:
    // Defaults are Moscow's coordinates - this project only targets a Moscow
    // deployment (see docs/UI.md).
    explicit OpenMeteoWeatherProvider(ILogger& logger,
        std::string cityName = "Moscow",
        double latitude = 55.7558,
        double longitude = 37.6173);

    [[nodiscard]] bool init() override;
    [[nodiscard]] bool refresh() override;
    [[nodiscard]] WeatherReading current() const override;

private:
    [[nodiscard]] static std::string conditionForWmoCode(int code);

    ILogger& logger_;
    std::string cityName_;
    double latitude_;
    double longitude_;
    WeatherReading reading_{};

#ifdef ESP_PLATFORM
    static esp_err_t httpEventHandler(esp_http_client_event_t* event);

    std::string responseBuffer_{};
#endif
};

} // namespace vaiwu::hal::esp

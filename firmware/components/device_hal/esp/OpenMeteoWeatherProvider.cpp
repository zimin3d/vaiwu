#include "vaiwu/hal/esp/OpenMeteoWeatherProvider.hpp"

#include <cstdio>
#include <utility>

#ifdef ESP_PLATFORM
#include "esp_crt_bundle.h"
#include <cJSON.h>
#endif

namespace vaiwu::hal::esp {

OpenMeteoWeatherProvider::OpenMeteoWeatherProvider(ILogger& logger,
    std::string cityName,
    double latitude,
    double longitude)
    : logger_(logger)
    , cityName_(std::move(cityName))
    , latitude_(latitude)
    , longitude_(longitude)
{
}

bool OpenMeteoWeatherProvider::init()
{
    // Nothing to set up ahead of time - refresh() performs the real HTTPS
    // call and is expected to be invoked periodically by the caller (see
    // main.cpp's loop and docs/UI.md), not here.
    return true;
}

#ifdef ESP_PLATFORM
esp_err_t OpenMeteoWeatherProvider::httpEventHandler(esp_http_client_event_t* event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && event->user_data != nullptr) {
        auto* self = static_cast<OpenMeteoWeatherProvider*>(event->user_data);
        self->responseBuffer_.append(static_cast<const char*>(event->data), static_cast<std::size_t>(event->data_len));
    }
    return ESP_OK;
}
#endif

bool OpenMeteoWeatherProvider::refresh()
{
#ifndef ESP_PLATFORM
    return false;
#else
    responseBuffer_.clear();

    char url[256];
    std::snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code&timezone=auto",
        latitude_, longitude_);

    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = &OpenMeteoWeatherProvider::httpEventHandler;
    config.user_data = this;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 8000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        logger_.error("Failed to create weather HTTP client");
        return false;
    }

    const esp_err_t result = esp_http_client_perform(client);
    const int statusCode = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (result != ESP_OK) {
        // esp_err_to_name() gives a real reason (ESP_ERR_ESP_TLS_*, DNS
        // failure via ESP_FAIL, timeout, etc.) instead of a generic message -
        // needed to tell apart "Wi-Fi isn't actually connected yet",
        // "DNS/TLS to api.open-meteo.com failed", and "server unreachable"
        // from a serial log alone.
        logger_.error(std::string("Weather HTTP request failed: ") + esp_err_to_name(result));
        return false;
    }
    if (statusCode != 200) {
        logger_.error("Weather HTTP request returned status " + std::to_string(statusCode));
        return false;
    }
    if (responseBuffer_.empty()) {
        logger_.error("Weather HTTP request returned an empty body");
        return false;
    }

    cJSON* root = cJSON_Parse(responseBuffer_.c_str());
    if (root == nullptr) {
        logger_.error("Failed to parse weather JSON response: " + responseBuffer_);
        return false;
    }

    bool ok = false;
    const cJSON* current = cJSON_GetObjectItemCaseSensitive(root, "current");
    if (cJSON_IsObject(current)) {
        const cJSON* temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
        const cJSON* weatherCode = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
        if (cJSON_IsNumber(temperature) && cJSON_IsNumber(weatherCode)) {
            reading_.available = true;
            reading_.temperatureCelsius = temperature->valuedouble;
            reading_.condition = conditionForWmoCode(weatherCode->valueint);
            reading_.cityName = cityName_;
            ok = true;
        }
    }

    cJSON_Delete(root);
    if (!ok) {
        logger_.error("Weather JSON response missing expected fields");
    }
    return ok;
#endif
}

WeatherReading OpenMeteoWeatherProvider::current() const
{
    return reading_;
}

std::string OpenMeteoWeatherProvider::conditionForWmoCode(int code)
{
    // WMO weather interpretation codes as documented by Open-Meteo
    // (https://open-meteo.com/en/docs) - mapped to short labels so they fit
    // the Home/Mascot layouts' weather widget (see docs/UI.md).
    if (code == 0) {
        return "CLEAR";
    }
    if (code == 1 || code == 2) {
        return "PARTLY CLOUDY";
    }
    if (code == 3) {
        return "OVERCAST";
    }
    if (code == 45 || code == 48) {
        return "FOG";
    }
    if (code >= 51 && code <= 57) {
        return "DRIZZLE";
    }
    if (code >= 61 && code <= 67) {
        return "RAIN";
    }
    if (code >= 71 && code <= 77) {
        return "SNOW";
    }
    if (code >= 80 && code <= 82) {
        return "SHOWERS";
    }
    if (code == 85 || code == 86) {
        return "SNOW SHOWERS";
    }
    if (code >= 95) {
        return "STORM";
    }
    return "UNKNOWN";
}

} // namespace vaiwu::hal::esp

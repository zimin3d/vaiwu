#pragma once

#include <string>

namespace vaiwu {

// A single weather reading. available is false until the first successful
// refresh() (or if every attempt so far has failed, e.g. no network yet) -
// callers should show a placeholder instead of trusting the other fields
// while available is false. condition is a short pre-formatted label (e.g.
// "CLEAR", "RAIN") rather than a raw weather code, so ui/device_hal never
// need to know about the underlying provider's own coding scheme.
struct WeatherReading {
    bool available{false};
    double temperatureCelsius{0.0};
    std::string condition{};
    std::string cityName{};
};

/**
 * Purpose: abstracts a weather data source for a fixed location.
 * Responsibilities: initialize the provider and fetch/expose the latest
 *                    weather reading. refresh() performs the actual network
 *                    call (blocking) and is expected to be invoked
 *                    periodically by a caller (e.g. main.cpp's loop), not on
 *                    every UI render - current() just returns whatever the
 *                    last refresh() produced.
 * Public API: init(), refresh(), current().
 */
class IWeatherProvider {
public:
    virtual ~IWeatherProvider() = default;

    [[nodiscard]] virtual bool init() = 0;
    [[nodiscard]] virtual bool refresh() = 0;
    [[nodiscard]] virtual WeatherReading current() const = 0;
};

} // namespace vaiwu

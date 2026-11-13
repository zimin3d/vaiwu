#pragma once

namespace vaiwu {

// A single wall-clock reading. hour/minute are 24h local time; day/month are
// day-of-month (1-31) and month-of-year (1-12); weekday is 0=Sunday..6=Saturday
// (matches std::tm::tm_wday), so callers can index a 7-entry name table
// directly. synced is false until the clock actually has a real time (e.g.
// before the first successful SNTP sync on real hardware) - callers should
// show a placeholder instead of trusting hour/minute/day/month/weekday while
// synced is false.
struct ClockReading {
    bool synced{false};
    int hour{0};
    int minute{0};
    int day{0};
    int month{0};
    int weekday{0};
};

/**
 * Purpose: abstracts the wall clock.
 * Responsibilities: initialize the time source (e.g. start SNTP sync on real
 *                    hardware) and expose the current local time reading.
 * Public API: init(), now().
 */
class IClock {
public:
    virtual ~IClock() = default;

    [[nodiscard]] virtual bool init() = 0;
    [[nodiscard]] virtual ClockReading now() const = 0;
};

} // namespace vaiwu

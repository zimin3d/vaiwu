#pragma once

#include "vaiwu/interfaces/IClock.hpp"
#include "vaiwu/interfaces/ILogger.hpp"

namespace vaiwu::hal::esp {

/**
 * Purpose: provides the board's wall clock via ESP-IDF's SNTP client.
 * Responsibilities: set the local timezone (fixed to Moscow, UTC+3, no DST
 *                    since 2014 - this project only targets Moscow per
 *                    docs/UI.md), start SNTP sync against a public NTP pool,
 *                    and expose whatever the system clock currently reads.
 *                    init() waits briefly for the first sync so early
 *                    renders can show a real time instead of a placeholder,
 *                    but does not fail if that first wait times out (SNTP
 *                    keeps retrying/resyncing in the background - see
 *                    docs/UI.md) - only now()'s synced flag reflects whether
 *                    a sync has actually completed yet.
 * Public API: init(), now().
 */
class EspSntpClock final : public IClock {
public:
    explicit EspSntpClock(ILogger& logger);

    [[nodiscard]] bool init() override;
    [[nodiscard]] ClockReading now() const override;

private:
    ILogger& logger_;
    bool initialized_{false};
};

} // namespace vaiwu::hal::esp

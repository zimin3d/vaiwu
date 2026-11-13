#pragma once

#include "vaiwu/interfaces/IClock.hpp"

#include <cstdint>

namespace vaiwu {

/**
 * Purpose: xiaozhi-style alarms and reminders for the assistant.
 * Responsibilities: hold up to kMaxAlarms pending triggers - absolute
 *                    wall-clock alarms (HH:MM, checked against IClock) and
 *                    relative reminders (fire after a monotonic-ms deadline).
 *                    due() returns the first alarm that should fire now and
 *                    consumes it. Pure logic, host-testable (clock/now are
 *                    passed in), so the UI can poll it from its 1 s timer.
 * Public API: addAt(), addAfter(), cancelAll(), count(), due().
 */
class AlarmService {
public:
    static constexpr int kMaxAlarms = 4;

    [[nodiscard]] bool addAt(int hour, int minute)
    {
        if (count_ >= kMaxAlarms || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
            return false;
        }
        alarms_[count_++] = Alarm{Type::Absolute, hour, minute, 0, false};
        return true;
    }

    [[nodiscard]] bool addAfter(int afterMinutes, std::uint64_t nowMs)
    {
        if (count_ >= kMaxAlarms || afterMinutes <= 0) {
            return false;
        }
        const std::uint64_t deadline = nowMs + static_cast<std::uint64_t>(afterMinutes) * 60000ULL;
        alarms_[count_++] = Alarm{Type::Relative, 0, 0, deadline, false};
        return true;
    }

    void cancelAll() { count_ = 0; }
    [[nodiscard]] int count() const { return count_; }

    // Returns true and removes one alarm if it is due. `reading` is the
    // current wall clock (only used for absolute alarms; ignored/allowed to
    // be unsynced for relative ones). Absolute alarms fire once when the
    // clock first reaches their minute.
    [[nodiscard]] bool due(const ClockReading& reading, std::uint64_t nowMs)
    {
        for (int i = 0; i < count_; ++i) {
            Alarm& alarm = alarms_[i];
            bool fire = false;
            if (alarm.type == Type::Relative) {
                fire = nowMs >= alarm.deadlineMs;
            } else if (reading.synced) {
                const bool match = reading.hour == alarm.hour && reading.minute == alarm.minute;
                if (match && !alarm.armedThisMinute) {
                    fire = true;
                }
                alarm.armedThisMinute = match;
            }
            if (fire) {
                for (int j = i; j < count_ - 1; ++j) {
                    alarms_[j] = alarms_[j + 1];
                }
                --count_;
                return true;
            }
        }
        return false;
    }

private:
    enum class Type : std::uint8_t { Absolute, Relative };
    struct Alarm {
        Type type;
        int hour;
        int minute;
        std::uint64_t deadlineMs;
        bool armedThisMinute;
    };

    Alarm alarms_[kMaxAlarms]{};
    int count_{0};
};

} // namespace vaiwu

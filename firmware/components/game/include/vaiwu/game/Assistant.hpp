#pragma once

#include "vaiwu/game/I18n.hpp"

#include <cstdint>

namespace vaiwu::game {

// What the user asked the assistant to do, recognized locally (no network).
enum class IntentKind : std::uint8_t {
    None = 0,         // free-form question -> goes to the AI
    SetAlarm,         // absolute HH:MM alarm
    AlarmMissingTime, // alarm requested but no parseable time in the text
    SetReminderIn,    // "remind me in N minutes/hours ..."
    Joke,
    AskTime,
    AskWeather,
    CancelAlarms,
    Pomodoro,     // start a 25-minute focus timer
    Convert,      // unit conversion, result in `convertResult`
};

struct Intent {
    IntentKind kind{IntentKind::None};
    int hour{-1};        // SetAlarm
    int minute{0};       // SetAlarm
    int afterMinutes{0}; // SetReminderIn / Pomodoro
    char convertResult[64]{}; // Convert: preformatted "5 km = 3.1 mi"
};

/**
 * Purpose: xiaozhi-style assistant intents, recognized on-device so alarms,
 *          reminders, jokes and the current time work even fully offline.
 * Responsibilities: lowercase the UTF-8 request (ASCII + Cyrillic), match
 *                    RU/EN keyword patterns and extract times ("7:30",
 *                    "через 10 минут", "in 2 hours"). Pure C++,
 *                    host-testable.
 * Public API: parseIntent(), jokeText().
 */
[[nodiscard]] Intent parseIntent(const char* utf8Text, Lang lang);

// Short built-in jokes (offline fallback for the "tell me a joke" intent).
[[nodiscard]] const char* jokeText(Lang lang, std::uint32_t salt);

} // namespace vaiwu::game

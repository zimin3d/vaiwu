#pragma once

#include <cstdint>
#include <optional>

namespace vaiwu {

enum class TouchAction : std::uint8_t {
    Down,
    Up,
    Move,
    Tap,
    LongPress,
};

struct TouchEvent {
    std::int16_t x{0};
    std::int16_t y{0};
    TouchAction action{TouchAction::Tap};
};

/**
 * Purpose: abstracts touch input.
 * Responsibilities: poll normalized touch events independent of touch controller hardware.
 * Public API: init(), poll().
 */
class ITouch {
public:
    virtual ~ITouch() = default;

    [[nodiscard]] virtual bool init() = 0;
    [[nodiscard]] virtual std::optional<TouchEvent> poll() = 0;
};

} // namespace vaiwu

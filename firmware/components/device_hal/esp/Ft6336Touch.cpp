#include "vaiwu/hal/esp/Ft6336Touch.hpp"

#include <algorithm>

#ifdef ESP_PLATFORM
#include <esp_timer.h>
#endif

namespace vaiwu::hal::esp {

namespace {
// A release held longer than this counts as a long-press (used to open/close
// the settings screen); anything shorter is a plain tap (used to adjust a
// setting). This is our own UI convention, not part of the touch protocol.
constexpr std::uint32_t kLongPressMs = 600;
} // namespace

Ft6336Touch::Ft6336Touch(ILogger& logger, void* i2cBusHandle, std::uint8_t i2cAddr,
    bool mirrorX, bool mirrorY)
    : logger_(logger)
    , i2cBusHandle_(i2cBusHandle)
    , i2cAddr_(i2cAddr)
    , mirrorX_(mirrorX)
    , mirrorY_(mirrorY)
{
}

Ft6336Touch::~Ft6336Touch()
{
#ifdef ESP_PLATFORM
    if (dev_ != nullptr) {
        i2c_master_bus_rm_device(dev_);
    }
#endif
}

bool Ft6336Touch::init()
{
#ifndef ESP_PLATFORM
    logger_.error("Ft6336Touch requires ESP-IDF; not available on host builds");
    return false;
#else
    if (i2cBusHandle_ == nullptr) {
        logger_.error("Ft6336Touch initialized with no shared I2C bus");
        return false;
    }

    i2c_device_config_t devConfig = {};
    devConfig.device_address = i2cAddr_;
    devConfig.scl_speed_hz = 400000;

    const auto bus = static_cast<i2c_master_bus_handle_t>(i2cBusHandle_);
    if (i2c_master_bus_add_device(bus, &devConfig, &dev_) != ESP_OK) {
        logger_.error("Failed to add touch controller to shared I2C bus");
        return false;
    }

    return true;
#endif
}

bool Ft6336Touch::readRaw(bool& touched, std::uint16_t& x, std::uint16_t& y)
{
    touched = false;
    x = 0;
    y = 0;

#ifndef ESP_PLATFORM
    return false;
#else
    if (dev_ == nullptr) {
        return false;
    }

    std::uint8_t reg = 0x02;
    std::uint8_t buf[5] = {};
    if (i2c_master_transmit_receive(dev_, &reg, 1, buf, sizeof(buf), 50) != ESP_OK) {
        return false;
    }

    const std::uint8_t points = buf[0] & 0x0F;
    if (points == 0) {
        return true;
    }

    touched = true;
    // Raw FT6336 registers, still in the panel's native (pre-swap) pixel
    // grid - see toLogicalEvent() for the transform into AssistantUi's
    // logical 320x240 frame.
    x = static_cast<std::uint16_t>(((buf[1] & 0x0F) << 8) | buf[2]);
    y = static_cast<std::uint16_t>(((buf[3] & 0x0F) << 8) | buf[4]);
    return true;
#endif
}

TouchEvent Ft6336Touch::toLogicalEvent(std::uint16_t rawX, std::uint16_t rawY, TouchAction action) const
{
    // Swap first (matches Ili9341Display's esp_lcd_panel_swap_xy(true) - see
    // the class comment in Ft6336Touch.hpp), then optionally mirror each
    // logical axis within the post-swap 320x240 frame.
    std::uint16_t logicalX = rawY;
    std::uint16_t logicalY = rawX;

    logicalX = std::min(logicalX, static_cast<std::uint16_t>(kLogicalWidth - 1));
    logicalY = std::min(logicalY, static_cast<std::uint16_t>(kLogicalHeight - 1));

    if (mirrorX_) {
        logicalX = static_cast<std::uint16_t>(kLogicalWidth - 1 - logicalX);
    }
    if (mirrorY_) {
        logicalY = static_cast<std::uint16_t>(kLogicalHeight - 1 - logicalY);
    }

    return TouchEvent{static_cast<std::int16_t>(logicalX), static_cast<std::int16_t>(logicalY), action};
}

bool Ft6336Touch::readLogical(bool& touched, std::int16_t& x, std::int16_t& y)
{
    touched = false;
    x = 0;
    y = 0;
    bool rawTouched = false;
    std::uint16_t rawX = 0;
    std::uint16_t rawY = 0;
    if (!readRaw(rawTouched, rawX, rawY)) {
        return false;
    }
    touched = rawTouched;
    if (rawTouched) {
        const TouchEvent event = toLogicalEvent(rawX, rawY, TouchAction::Down);
        x = event.x;
        y = event.y;
    }
    return true;
}

std::optional<TouchEvent> Ft6336Touch::poll()
{
#ifndef ESP_PLATFORM
    return std::nullopt;
#else
    bool touched = false;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    if (!readRaw(touched, x, y)) {
        return std::nullopt;
    }

    const auto nowMs = static_cast<std::uint32_t>(esp_timer_get_time() / 1000);

    if (touched) {
        if (!wasDown_) {
            wasDown_ = true;
            downStartMs_ = nowMs;
        }
        lastRawX_ = x;
        lastRawY_ = y;
        return toLogicalEvent(x, y, TouchAction::Down);
    }

    if (wasDown_) {
        wasDown_ = false;
        const std::uint32_t heldMs = nowMs - downStartMs_;
        const TouchAction action = heldMs >= kLongPressMs ? TouchAction::LongPress : TouchAction::Tap;
        return toLogicalEvent(lastRawX_, lastRawY_, action);
    }

    return std::nullopt;
#endif
}

} // namespace vaiwu::hal::esp

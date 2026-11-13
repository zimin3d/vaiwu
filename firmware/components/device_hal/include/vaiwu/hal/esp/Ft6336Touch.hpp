#pragma once

#include "vaiwu/interfaces/ILogger.hpp"
#include "vaiwu/interfaces/ITouch.hpp"

#include <cstdint>

#ifdef ESP_PLATFORM
#include <driver/i2c_master.h>
#endif

namespace vaiwu::hal::esp {

/**
 * Purpose: drives the board's FocalTech-protocol capacitive touch controller.
 * Responsibilities: poll touch point + pressed/released state over the shared
 *                    I2C bus (register 0x02, matching the confirmed working
 *                    xiaozhi-esp32 reference firmware's TouchDriver for this
 *                    exact board) and classify press duration into Tap vs
 *                    LongPress so callers get the same ITouch contract other
 *                    boards would use.
 *
 *                    Coordinate mapping: the FT6336 reports raw touch
 *                    coordinates in the panel's native (pre-swap) pixel grid
 *                    (240 wide x 320 tall - see BoardConfig's documented
 *                    native resolution), but `Ili9341Display` calls
 *                    `esp_lcd_panel_swap_xy(panel_, true)`, so every drawn
 *                    pixel - and every touch zone `AssistantUi` computes -
 *                    lives in the *post-swap* 320x240 logical frame instead.
 *                    `esp_lcd_panel_swap_xy` only affects how the LCD
 *                    controller is told to address pixels; it has no effect
 *                    on the touch chip's own I2C register values, so without
 *                    an equivalent transform here, raw touch reports would be
 *                    off by a 90-degree rotation from what's on screen (and
 *                    `AssistantUi`'s x/y-based touch-zone math would never
 *                    line up with what the user actually sees). `poll()`
 *                    swaps raw x/y before returning a `TouchEvent`, and
 *                    `mirrorX`/`mirrorY` are exposed for the (unverified from
 *                    the sandbox) case where the touch panel also needs an
 *                    axis flip once the swap is confirmed against real
 *                    hardware - see docs/HARDWARE_BRINGUP.md.
 * Public API: init(), poll().
 */
class Ft6336Touch final : public ITouch {
public:
    // Post-swap logical frame Ft6336Touch outputs coordinates in - must match
    // Ili9341Display's logical width/height (the display's constructor swaps
    // BoardConfig's native 240x320 to 320x240 internally - see its comment).
    static constexpr std::uint16_t kLogicalWidth = 320;
    static constexpr std::uint16_t kLogicalHeight = 240;

    // i2cBusHandle is an opaque i2c_master_bus_handle_t owned by SharedI2cBus;
    // Ft6336Touch does not create or delete the bus itself, only adds its own
    // device on it, so it can safely share sda=16/scl=15 with the ES8311 codec.
    // mirrorX/mirrorY flip the corresponding axis *after* the swap described
    // above - flip whichever one feels backwards on real hardware (e.g. tap
    // near the top-left BACK button does nothing but tapping near top-right
    // closes Settings instead => mirrorX=true).
    Ft6336Touch(ILogger& logger, void* i2cBusHandle, std::uint8_t i2cAddr = 0x38,
        bool mirrorX = false, bool mirrorY = false);
    ~Ft6336Touch() override;

    [[nodiscard]] bool init() override;
    [[nodiscard]] std::optional<TouchEvent> poll() override;

    // Direct state read for an LVGL pointer indev: reports whether the panel
    // is currently pressed and, if so, the position already transformed into
    // the same post-swap logical 320x240 frame poll() uses. Returns false if
    // the raw I2C read failed (caller should keep the previous state).
    [[nodiscard]] bool readLogical(bool& touched, std::int16_t& x, std::int16_t& y);

private:
    [[nodiscard]] bool readRaw(bool& touched, std::uint16_t& x, std::uint16_t& y);
    [[nodiscard]] TouchEvent toLogicalEvent(std::uint16_t rawX, std::uint16_t rawY, TouchAction action) const;

    ILogger& logger_;
    void* i2cBusHandle_;
    std::uint8_t i2cAddr_;
    bool mirrorX_;
    bool mirrorY_;
    bool wasDown_{false};
    std::uint32_t downStartMs_{0};
    std::uint16_t lastRawX_{0};
    std::uint16_t lastRawY_{0};

#ifdef ESP_PLATFORM
    i2c_master_dev_handle_t dev_{nullptr};
#endif
};

} // namespace vaiwu::hal::esp

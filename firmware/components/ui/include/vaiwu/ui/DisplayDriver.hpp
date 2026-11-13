#pragma once

#include "vaiwu/interfaces/ILogger.hpp"

#include <cstdint>

namespace vaiwu::ui {

// Display pin/bus description for the ILI9341 SPI panel. Values come from
// the Freenove ESP32-S3 2.8" board profile from xiaozhi-esp32.
struct DisplayPins {
    int mosi{11};
    int miso{13};
    int sclk{12};
    int cs{10};
    int dc{46};
    int reset{-1};
    int backlight{45};
    int spiHost{1}; // SPI2_HOST
};

/**
 * Purpose: brings up the board's ILI9341 SPI panel and the LVGL runtime.
 * Responsibilities: SPI bus + esp_lcd panel init (20 MHz, mode 0,
 *                    swap_xy=true, invert_color=true, BGR order - the
 *                    combination confirmed working on this hardware by the
 *                    xiaozhi-esp32), then esp_lvgl_port setup
 *                    so the UI layer can create plain LVGL widgets. The
 *                    logical frame after swap is 320x240 (landscape).
 * Public API: init(), ready().
 */
class DisplayDriver {
public:
    static constexpr std::uint16_t kNativeWidth = 240;  // panel native
    static constexpr std::uint16_t kNativeHeight = 320;
    static constexpr std::uint16_t kWidth = 320;  // post-swap logical frame
    static constexpr std::uint16_t kHeight = 240;

    DisplayDriver(ILogger& logger, const DisplayPins& pins);
    ~DisplayDriver();

    DisplayDriver(const DisplayDriver&) = delete;
    DisplayDriver& operator=(const DisplayDriver&) = delete;

    [[nodiscard]] bool init();
    [[nodiscard]] bool ready() const { return ready_; }

    // Backlight brightness 0-100% via LEDC PWM on the BL pin. Values are
    // clamped; if PWM setup failed the backlight falls back to plain on/off.
    void setBrightness(std::uint8_t percent);

private:
    ILogger& logger_;
    DisplayPins pins_;
    bool ready_{false};
    bool backlightPwm_{false};

#ifdef ESP_PLATFORM
    void* panelIo_{nullptr}; // esp_lcd_panel_io_handle_t
    void* panel_{nullptr};   // esp_lcd_panel_handle_t
#endif
};

} // namespace vaiwu::ui

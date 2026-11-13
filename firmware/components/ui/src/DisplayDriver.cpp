#include "vaiwu/ui/DisplayDriver.hpp"

#ifdef ESP_PLATFORM
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/spi_common.h>
#endif

namespace vaiwu::ui {

namespace {
constexpr int kBufferRows = 20;
}

DisplayDriver::DisplayDriver(ILogger& logger, const DisplayPins& pins)
    : logger_(logger)
    , pins_(pins)
{
}

DisplayDriver::~DisplayDriver()
{
#ifdef ESP_PLATFORM
    // The display lives for the whole firmware lifetime; no teardown path.
#endif
}

void DisplayDriver::setBrightness(std::uint8_t percent)
{
#ifdef ESP_PLATFORM
    if (percent > 100U) {
        percent = 100U;
    }
    if (pins_.backlight < 0) {
        return;
    }
    if (!backlightPwm_) {
        gpio_set_level(static_cast<gpio_num_t>(pins_.backlight), percent > 0U ? 1 : 0);
        return;
    }
    const std::uint32_t duty = (1023U * percent) / 100U;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
#else
    static_cast<void>(percent);
#endif
}

bool DisplayDriver::init()
{
#ifndef ESP_PLATFORM
    logger_.error("DisplayDriver requires ESP-IDF; not available on host builds");
    return false;
#else
    if (pins_.backlight >= 0) {
        gpio_config_t backlightConfig = {};
        backlightConfig.pin_bit_mask = 1ULL << pins_.backlight;
        backlightConfig.mode = GPIO_MODE_OUTPUT;
        gpio_config(&backlightConfig);
        gpio_set_level(static_cast<gpio_num_t>(pins_.backlight), 0);
    }

    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = pins_.mosi;
    busConfig.miso_io_num = pins_.miso;
    busConfig.sclk_io_num = pins_.sclk;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz = static_cast<int>(kWidth) * kBufferRows * 2;

    if (spi_bus_initialize(static_cast<spi_host_device_t>(pins_.spiHost), &busConfig, SPI_DMA_CH_AUTO) != ESP_OK) {
        logger_.error("DisplayDriver: SPI bus init failed");
        return false;
    }

    esp_lcd_panel_io_spi_config_t ioConfig = {};
    ioConfig.cs_gpio_num = pins_.cs;
    ioConfig.dc_gpio_num = pins_.dc;
    ioConfig.spi_mode = 0;
    ioConfig.pclk_hz = 20 * 1000 * 1000;
    ioConfig.trans_queue_depth = 10;
    ioConfig.lcd_cmd_bits = 8;
    ioConfig.lcd_param_bits = 8;

    auto* panelIo = static_cast<esp_lcd_panel_io_handle_t>(panelIo_);
    if (esp_lcd_new_panel_io_spi(static_cast<spi_host_device_t>(pins_.spiHost), &ioConfig, &panelIo) != ESP_OK) {
        logger_.error("DisplayDriver: panel IO init failed");
        return false;
    }
    panelIo_ = panelIo;

    esp_lcd_panel_dev_config_t panelConfig = {};
    panelConfig.reset_gpio_num = pins_.reset;
    panelConfig.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panelConfig.bits_per_pixel = 16;

    esp_lcd_panel_handle_t panel = nullptr;
    if (esp_lcd_new_panel_ili9341(panelIo, &panelConfig, &panel) != ESP_OK) {
        logger_.error("DisplayDriver: ILI9341 driver init failed");
        return false;
    }
    panel_ = panel;

    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, true);
    esp_lcd_panel_swap_xy(panel, true);
    esp_lcd_panel_mirror(panel, false, false);

    // Some ILI9341 modules stay dark after init without an explicit DISPON.
    const esp_err_t dispOnErr = esp_lcd_panel_disp_on_off(panel, true);
    if (dispOnErr != ESP_OK && dispOnErr != ESP_ERR_NOT_SUPPORTED) {
        logger_.warn("DisplayDriver: disp_on_off failed");
    }

    if (pins_.backlight >= 0) {
        // Backlight via LEDC PWM so Settings can dim the screen. Falls back
        // to a plain always-on GPIO if the PWM setup fails for any reason.
        ledc_timer_config_t timerConfig = {};
        timerConfig.speed_mode = LEDC_LOW_SPEED_MODE;
        timerConfig.duty_resolution = LEDC_TIMER_10_BIT;
        timerConfig.timer_num = LEDC_TIMER_0;
        timerConfig.freq_hz = 5000;
        timerConfig.clk_cfg = LEDC_AUTO_CLK;
        ledc_channel_config_t channelConfig = {};
        channelConfig.gpio_num = pins_.backlight;
        channelConfig.speed_mode = LEDC_LOW_SPEED_MODE;
        channelConfig.channel = LEDC_CHANNEL_0;
        channelConfig.intr_type = LEDC_INTR_DISABLE;
        channelConfig.timer_sel = LEDC_TIMER_0;
        channelConfig.duty = 1023; // full brightness until Settings says otherwise
        channelConfig.hpoint = 0;
        if (ledc_timer_config(&timerConfig) == ESP_OK
            && ledc_channel_config(&channelConfig) == ESP_OK) {
            backlightPwm_ = true;
        } else {
            logger_.warn("DisplayDriver: backlight PWM setup failed; always-on");
            gpio_set_level(static_cast<gpio_num_t>(pins_.backlight), 1);
        }
    }

    lv_init();

    lvgl_port_cfg_t portConfig = ESP_LVGL_PORT_INIT_CONFIG();
    portConfig.task_priority = 1;
    if (lvgl_port_init(&portConfig) != ESP_OK) {
        logger_.error("DisplayDriver: lvgl_port init failed");
        return false;
    }

    lvgl_port_display_cfg_t displayConfig = {};
    displayConfig.io_handle = panelIo;
    displayConfig.panel_handle = panel;
    displayConfig.buffer_size = static_cast<std::uint32_t>(kWidth) * kBufferRows;
    displayConfig.double_buffer = false;
    displayConfig.hres = kWidth;
    displayConfig.vres = kHeight;
    displayConfig.monochrome = false;
    displayConfig.rotation.swap_xy = true;
    displayConfig.rotation.mirror_x = false;
    displayConfig.rotation.mirror_y = false;
    displayConfig.color_format = LV_COLOR_FORMAT_RGB565;
    displayConfig.flags.buff_dma = true;
    displayConfig.flags.swap_bytes = true;

    if (lvgl_port_add_disp(&displayConfig) == nullptr) {
        logger_.error("DisplayDriver: lvgl_port_add_disp failed");
        return false;
    }

    ready_ = true;
    return true;
#endif
}

} // namespace vaiwu::ui

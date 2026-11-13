/* Host-check stub (IDF 5.5 API subset). */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct esp_lcd_panel_io_t* esp_lcd_panel_io_handle_t;
typedef int esp_lcd_spi_bus_handle_t;
typedef struct {
    int cs_gpio_num;
    int dc_gpio_num;
    int spi_mode;
    unsigned int pclk_hz;
    int lcd_cmd_bits;
    int lcd_param_bits;
    size_t trans_queue_depth;
    void* on_color_trans_done;
    void* user_ctx;
    struct {
        unsigned int dc_high_on_cmd : 1;
        unsigned int dc_low_on_data : 1;
        unsigned int dc_low_on_param : 1;
        unsigned int octal_mode : 1;
        unsigned int quad_mode : 1;
        unsigned int sio_mode : 1;
        unsigned int lsb_first : 1;
        unsigned int cs_high_active : 1;
    } flags;
} esp_lcd_panel_io_spi_config_t;
esp_err_t esp_lcd_new_panel_io_spi(esp_lcd_spi_bus_handle_t bus,
    const esp_lcd_panel_io_spi_config_t* io_config, esp_lcd_panel_io_handle_t* ret_io);
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t io);
#ifdef __cplusplus
}
#endif

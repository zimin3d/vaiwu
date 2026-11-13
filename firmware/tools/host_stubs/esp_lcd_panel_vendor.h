/* Host-check stub (IDF 5.5 API subset). */
#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    LCD_RGB_ELEMENT_ORDER_RGB = 0,
    LCD_RGB_ELEMENT_ORDER_BGR,
} lcd_rgb_element_order_t;
typedef struct {
    int reset_gpio_num;
    lcd_rgb_element_order_t rgb_ele_order;
    unsigned int bits_per_pixel;
    struct {
        unsigned int reset_active_high : 1;
    } flags;
    void* vendor_config;
} esp_lcd_panel_dev_config_t;
#ifdef __cplusplus
}
#endif

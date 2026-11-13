/* Host-check stub (IDF 5.5 API subset). */
#pragma once
#include "esp_err.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int gpio_num_t;
typedef enum { GPIO_MODE_DISABLE = 0, GPIO_MODE_INPUT, GPIO_MODE_OUTPUT } gpio_mode_t;
typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;
esp_err_t gpio_config(const gpio_config_t* config);
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);
#ifdef __cplusplus
}
#endif

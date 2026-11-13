/* Minimal esp_lvgl_port stub (see lvgl.h in this directory). */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int task_priority;
    int task_stack;
    int task_affinity;
    int task_max_sleep_ms;
    int timer_period_ms;
} lvgl_port_cfg_t;

#define ESP_LVGL_PORT_INIT_CONFIG() \
    (lvgl_port_cfg_t) { .task_priority = 4, .task_stack = 6144, .task_affinity = -1, \
        .task_max_sleep_ms = 500, .timer_period_ms = 5 }

typedef struct {
    void* io_handle;
    void* panel_handle;
    void* control_handle;
    uint32_t buffer_size;
    bool double_buffer;
    uint32_t trans_size;
    uint32_t hres;
    uint32_t vres;
    bool monochrome;
    struct {
        bool swap_xy;
        bool mirror_x;
        bool mirror_y;
    } rotation;
    int color_format;
    struct {
        unsigned int buff_dma : 1;
        unsigned int buff_spiram : 1;
        unsigned int sw_rotate : 1;
        unsigned int swap_bytes : 1;
        unsigned int full_refresh : 1;
        unsigned int direct_mode : 1;
    } flags;
} lvgl_port_display_cfg_t;

esp_err_t lvgl_port_init(const lvgl_port_cfg_t* cfg);
lv_display_t* lvgl_port_add_disp(const lvgl_port_display_cfg_t* cfg);
bool lvgl_port_lock(uint32_t timeout_ms);
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif

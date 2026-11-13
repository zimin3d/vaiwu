/* Minimal LVGL 9 API stub for host syntax checks of the UI's ESP branch.
 * Used ONLY when VAIWU_LVGL_DIR is not set (see run_host_checks.py); the
 * real headers always win when available. Signatures follow LVGL 9.x. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t blue, green, red; } lv_color_t;
typedef uint8_t lv_opa_t;
typedef int32_t lv_coord_t;
typedef uint32_t lv_style_selector_t;

typedef struct { int _; } lv_obj_t;
typedef struct { int _; } lv_event_t;
typedef struct { int _; } lv_timer_t;
typedef struct { int _; } lv_indev_t;
typedef struct { int _; } lv_display_t;
typedef struct { int _; } lv_font_t;

typedef struct {
    struct {
        uint32_t magic;
        uint32_t cf;
        uint32_t w;
        uint32_t h;
        uint32_t stride;
    } header;
    uint32_t data_size;
    const uint8_t* data;
} lv_image_dsc_t;

enum { LV_IMAGE_HEADER_MAGIC = 0x19 };
enum { LV_COLOR_FORMAT_RGB565 = 0x12, LV_COLOR_FORMAT_RGB565A8 = 0x14,
       LV_COLOR_FORMAT_ARGB8888 = 0x10 };

typedef enum { LV_ALIGN_TOP_LEFT = 0, LV_ALIGN_TOP_MID, LV_ALIGN_TOP_RIGHT,
    LV_ALIGN_LEFT_MID, LV_ALIGN_CENTER, LV_ALIGN_RIGHT_MID,
    LV_ALIGN_BOTTOM_MID } lv_align_t;
typedef enum { LV_ANIM_OFF = 0, LV_ANIM_ON } lv_anim_enable_t;
typedef enum { LV_EVENT_ALL = 0, LV_EVENT_PRESSED, LV_EVENT_CLICKED,
    LV_EVENT_RELEASED, LV_EVENT_LONG_PRESSED, LV_EVENT_VALUE_CHANGED,
    LV_EVENT_FOCUSED, LV_EVENT_DEFOCUSED, LV_EVENT_READY,
    LV_EVENT_CANCEL } lv_event_code_t;
typedef enum { LV_FLEX_FLOW_COLUMN = 1 } lv_flex_flow_t;
typedef enum { LV_INDEV_TYPE_POINTER = 1 } lv_indev_type_t;
typedef enum { LV_INDEV_STATE_RELEASED = 0, LV_INDEV_STATE_PRESSED } lv_indev_state_t;
typedef enum { LV_LABEL_LONG_WRAP = 0, LV_LABEL_LONG_DOT,
    LV_LABEL_LONG_SCROLL } lv_label_long_mode_t;
typedef enum { LV_KEYBOARD_MODE_TEXT_LOWER = 0, LV_KEYBOARD_MODE_TEXT_UPPER,
    LV_KEYBOARD_MODE_SPECIAL, LV_KEYBOARD_MODE_NUMBER,
    LV_KEYBOARD_MODE_USER_1, LV_KEYBOARD_MODE_USER_2 } lv_keyboard_mode_t;
typedef enum { LV_TEXT_ALIGN_AUTO = 0, LV_TEXT_ALIGN_LEFT,
    LV_TEXT_ALIGN_CENTER, LV_TEXT_ALIGN_RIGHT } lv_text_align_t;
typedef uint32_t lv_buttonmatrix_ctrl_t;

enum { LV_OPA_TRANSP = 0, LV_OPA_90 = 229, LV_OPA_COVER = 255 };
enum { LV_PART_MAIN = 0x000000, LV_PART_INDICATOR = 0x010000, LV_PART_KNOB = 0x020000 };
enum { LV_STATE_PRESSED = 0x0020 };
enum { LV_OBJ_FLAG_HIDDEN = 1, LV_OBJ_FLAG_CLICKABLE = 2, LV_OBJ_FLAG_SCROLLABLE = 16 };
enum { LV_RADIUS_CIRCLE = 0x7FFF };
enum { LV_KEYBOARD_CTRL_BUTTON_FLAGS = 0x8000 };
enum { LV_BUTTONMATRIX_BUTTON_NONE = 0xFFFF };

#define LV_SYMBOL_AUDIO "\xEF\x80\x81"
#define LV_SYMBOL_BACKSPACE "\xEF\x95\x9A"
#define LV_SYMBOL_BELL "\xEF\x83\xB3"
#define LV_SYMBOL_KEYBOARD "\xEF\x84\x9C"
#define LV_SYMBOL_LEFT "\xEF\x81\x93"
#define LV_SYMBOL_NEW_LINE "\xEF\xA2\xA2"
#define LV_SYMBOL_OK "\xEF\x80\x8C"
#define LV_SYMBOL_RIGHT "\xEF\x81\x94"
#define LV_SYMBOL_SETTINGS "\xEF\x80\x93"
#define LV_SYMBOL_WIFI "\xEF\x87\xAB"

typedef void (*lv_event_cb_t)(lv_event_t*);
typedef void (*lv_timer_cb_t)(lv_timer_t*);
typedef struct { lv_indev_state_t state; struct { int32_t x, y; } point; } lv_indev_data_t;
typedef void (*lv_indev_read_cb_t)(lv_indev_t*, lv_indev_data_t*);

extern const lv_font_t lv_font_montserrat_14;
extern const lv_font_t lv_font_montserrat_24;

void lv_init(void);
lv_color_t lv_color_hex(uint32_t c);

lv_obj_t* lv_obj_create(lv_obj_t* parent);
lv_obj_t* lv_button_create(lv_obj_t* parent);
lv_obj_t* lv_label_create(lv_obj_t* parent);
lv_obj_t* lv_image_create(lv_obj_t* parent);
lv_obj_t* lv_bar_create(lv_obj_t* parent);
lv_obj_t* lv_slider_create(lv_obj_t* parent);
lv_obj_t* lv_textarea_create(lv_obj_t* parent);
lv_obj_t* lv_keyboard_create(lv_obj_t* parent);

void lv_obj_set_pos(lv_obj_t* o, int32_t x, int32_t y);
void lv_obj_set_size(lv_obj_t* o, int32_t w, int32_t h);
void lv_obj_set_width(lv_obj_t* o, int32_t w);
void lv_obj_set_y(lv_obj_t* o, int32_t y);
void lv_obj_align(lv_obj_t* o, lv_align_t a, int32_t x, int32_t y);
void lv_obj_center(lv_obj_t* o);
void lv_obj_add_flag(lv_obj_t* o, int f);
void lv_obj_remove_flag(lv_obj_t* o, int f);
void lv_obj_add_event_cb(lv_obj_t* o, lv_event_cb_t cb, lv_event_code_t c, void* d);
void lv_obj_remove_event_cb(lv_obj_t* o, lv_event_cb_t cb);
void lv_obj_set_user_data(lv_obj_t* o, void* d);
void* lv_obj_get_user_data(lv_obj_t* o);
lv_obj_t* lv_obj_get_child(const lv_obj_t* o, int32_t idx);
void lv_obj_scroll_to_view(lv_obj_t* o, lv_anim_enable_t a);
void lv_obj_set_flex_flow(lv_obj_t* o, lv_flex_flow_t f);

void lv_obj_set_style_bg_color(lv_obj_t* o, lv_color_t c, lv_style_selector_t s);
void lv_obj_set_style_bg_opa(lv_obj_t* o, lv_opa_t v, lv_style_selector_t s);
void lv_obj_set_style_border_color(lv_obj_t* o, lv_color_t c, lv_style_selector_t s);
void lv_obj_set_style_border_width(lv_obj_t* o, int32_t v, lv_style_selector_t s);
void lv_obj_set_style_pad_all(lv_obj_t* o, int32_t v, lv_style_selector_t s);
void lv_obj_set_style_pad_bottom(lv_obj_t* o, int32_t v, lv_style_selector_t s);
void lv_obj_set_style_pad_row(lv_obj_t* o, int32_t v, lv_style_selector_t s);
void lv_obj_set_style_radius(lv_obj_t* o, int32_t v, lv_style_selector_t s);
void lv_obj_set_style_shadow_width(lv_obj_t* o, int32_t v, lv_style_selector_t s);
void lv_obj_set_style_text_align(lv_obj_t* o, lv_text_align_t v, lv_style_selector_t s);
void lv_obj_set_style_text_color(lv_obj_t* o, lv_color_t c, lv_style_selector_t s);
void lv_obj_set_style_text_font(lv_obj_t* o, const lv_font_t* f, lv_style_selector_t s);

void lv_label_set_text(lv_obj_t* o, const char* t);
void lv_label_set_long_mode(lv_obj_t* o, lv_label_long_mode_t m);
void lv_image_set_src(lv_obj_t* o, const void* src);
void lv_image_set_pivot(lv_obj_t* o, int32_t x, int32_t y);
void lv_image_set_scale(lv_obj_t* o, uint32_t z);
void lv_image_set_scale_x(lv_obj_t* o, uint32_t z);
void lv_image_set_scale_y(lv_obj_t* o, uint32_t z);
void lv_bar_set_range(lv_obj_t* o, int32_t lo, int32_t hi);
void lv_bar_set_value(lv_obj_t* o, int32_t v, lv_anim_enable_t a);
void lv_slider_set_range(lv_obj_t* o, int32_t lo, int32_t hi);
void lv_slider_set_value(lv_obj_t* o, int32_t v, lv_anim_enable_t a);
int32_t lv_slider_get_value(const lv_obj_t* o);
void lv_textarea_set_one_line(lv_obj_t* o, bool en);
void lv_textarea_set_max_length(lv_obj_t* o, uint32_t n);
void lv_textarea_set_placeholder_text(lv_obj_t* o, const char* t);
void lv_textarea_set_text(lv_obj_t* o, const char* t);
const char* lv_textarea_get_text(const lv_obj_t* o);
void lv_keyboard_set_map(lv_obj_t* kb, lv_keyboard_mode_t m, const char* const map[],
    const lv_buttonmatrix_ctrl_t ctrl[]);
void lv_keyboard_set_mode(lv_obj_t* kb, lv_keyboard_mode_t m);
lv_keyboard_mode_t lv_keyboard_get_mode(const lv_obj_t* kb);
void lv_keyboard_set_popovers(lv_obj_t* kb, bool en);
void lv_keyboard_set_textarea(lv_obj_t* kb, lv_obj_t* ta);
void lv_keyboard_def_event_cb(lv_event_t* e);
uint32_t lv_buttonmatrix_get_selected_button(const lv_obj_t* o);
const char* lv_buttonmatrix_get_button_text(const lv_obj_t* o, uint32_t id);

lv_event_code_t lv_event_get_code(lv_event_t* e);
void* lv_event_get_current_target(lv_event_t* e);
void* lv_event_get_user_data(lv_event_t* e);

lv_obj_t* lv_screen_active(void);
void lv_screen_load(lv_obj_t* scr);
lv_timer_t* lv_timer_create(lv_timer_cb_t cb, uint32_t ms, void* d);
lv_indev_t* lv_indev_create(void);
void lv_indev_set_type(lv_indev_t* i, lv_indev_type_t t);
void lv_indev_set_read_cb(lv_indev_t* i, lv_indev_read_cb_t cb);

#ifdef __cplusplus
}
#endif

#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <lvgl.h>

// Declare the custom font so the linker can find it
extern "C" {
    LV_FONT_DECLARE(mdi_30);
    LV_FONT_DECLARE(mdi_44);
    LV_FONT_DECLARE(custom_font_30);
    LV_FONT_DECLARE(custom_font_44);
}

// --- UI Colors (declared here, defined in ui.cpp) ---
extern const lv_color_t COLOR_GREEN;
extern const lv_color_t COLOR_MID;
extern const lv_color_t COLOR_HIGH;
extern const lv_color_t COLOR_HA_BLUE;
extern const lv_color_t COLOR_DISCONNECTED;
extern const lv_color_t COLOR_DEFAULT_ICON;

// Enum for Fan Status used across UI and main logic
enum FanStatus {
    FAN_STATUS_OFF,
    FAN_STATUS_NORMAL,
    FAN_STATUS_ALERT
};

// Struct to group fan alert icons for blinking
typedef struct {
    lv_obj_t* status_bar_icon;
    lv_obj_t* main_screen_icon;
} FanAlertIcons_t;

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Helper to update label text only if it has actually changed, avoiding costly heap reallocations and invalidations
static inline void ui_set_label_text(lv_obj_t* label, const char* text) {
    if (!label || !text) return;
    const char* current = lv_label_get_text(label);
    if (current && strcmp(current, text) == 0) return;
    lv_label_set_text(label, text);
}

// Helper to update text style color only if it has changed, avoiding style recalculations and invalidations
static inline void ui_set_text_color(lv_obj_t* obj, lv_color_t color) {
    if (!obj) return;
    lv_color_t current = lv_obj_get_style_text_color(obj, 0);
    if (current.full == color.full) return;
    lv_obj_set_style_text_color(obj, color, 0);
}

// Formatted version that formats into a stack buffer and checks for string equality before invalidating
static inline void ui_set_label_text_fmt(lv_obj_t* label, const char* fmt, ...) {
    if (!label || !fmt) return;
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ui_set_label_text(label, buf);
}

#endif // UI_COMMON_H
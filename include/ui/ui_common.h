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

#endif // UI_COMMON_H
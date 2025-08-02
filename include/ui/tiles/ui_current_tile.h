#ifndef UI_CURRENT_TILE_H
#define UI_CURRENT_TILE_H

#include "../ui_common.h"
#include "ConfigManager.h"

class UICurrentTile {
public:
    UICurrentTile(ConfigManager* config);
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_fan_current(float amps, FanStatus status);
    void update_compressor_current(float amps);
    void update_pump_current(float amps);
    void update_fan_status(FanStatus status);
    void clear_readings();
    void set_fan_status_icon(lv_obj_t* icon);
private:
    static void fan_alert_blink_cb(lv_timer_t* timer);
    ConfigManager* _config;
    lv_obj_t* fan_icon, *fan_label;
    lv_obj_t* compressor_icon, *compressor_label;
    lv_obj_t* pump_icon, *pump_label;
    lv_timer_t* fan_blink_timer = nullptr;
    FanAlertIcons_t fan_alert_icons;
};

#endif // UI_CURRENT_TILE_H 
#ifndef UI_SECONDARY_TILE_H
#define UI_SECONDARY_TILE_H

#include "../ui_common.h"
#include "ConfigManager.h"

class UISecondaryTile {
public:
    UISecondaryTile(ConfigManager* config);
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void set_fan_status_icon(lv_obj_t* icon);
    void update_pm_values(float pm1, float pm25, float pm4, float pm10);
    void update_fan_current(float amps, FanStatus status);
    void update_fan_status(FanStatus status);
    void clear_readings();
    void update_pm1(float pm1);
    void update_pm25(float pm25);
    void update_pm4(float pm4);
    void update_pm10(float pm10);
    void update_fan_amps(float amps);
    void update_sps30_fan_interval(unsigned long fan_interval);
    void update_sps30_fan_days(unsigned long fan_days);
    void update_sgp41_test_status(int ret_status);
    void update_sgp41_test_value(uint16_t raw_value);
private:
    static void fan_alert_blink_cb(lv_timer_t* timer);
    ConfigManager* _config;
    lv_obj_t* pm1_icon, *pm1_label;
    lv_obj_t* pm25_icon, *pm25_label;
    lv_obj_t* pm4_icon, *pm4_label;
    lv_obj_t* pm10_icon, *pm10_label;
    lv_obj_t* fan_icon, *fan_label;
    lv_timer_t* fan_blink_timer = nullptr;
    FanAlertIcons_t fan_alert_icons;
};

#endif // UI_SECONDARY_TILE_H
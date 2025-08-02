#ifndef UI_SECONDARY_TILE_H
#define UI_SECONDARY_TILE_H

#include "../ui_common.h"

class UISecondaryTile {
public:
    UISecondaryTile();
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_pm_values(float pm1, float pm25, float pm4, float pm10);
    void clear_readings();
    void update_pm1(float pm1);
    void update_pm25(float pm25);
    void update_pm4(float pm4);
    void update_pm10(float pm10);
    void update_sps30_fan_interval(unsigned long fan_interval);
    void update_sps30_fan_days(unsigned long fan_days);
    void update_sgp41_test_status(int ret_status);
    void update_sgp41_test_value(uint16_t raw_value);
private:
    lv_obj_t* pm1_icon, *pm1_label;
    lv_obj_t* pm25_icon, *pm25_label;
    lv_obj_t* pm4_icon, *pm4_label;
    lv_obj_t* pm10_icon, *pm10_label;
};

#endif // UI_SECONDARY_TILE_H
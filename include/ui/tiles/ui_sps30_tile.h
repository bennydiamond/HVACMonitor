#ifndef UI_SPS30_TILE_H
#define UI_SPS30_TILE_H

#include "../ui_common.h"

class UISPS30Tile {
public:
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_sps30_info(uint32_t fan_interval, uint8_t fan_days);
    void update_sps30_fan_interval(unsigned long fan_interval);
    void update_sps30_fan_days(unsigned long fan_days);
    static void manual_clean_cb(lv_event_t* e);
private:
    lv_obj_t* interval_label;
    lv_obj_t* days_label;
};

#endif // UI_SPS30_TILE_H
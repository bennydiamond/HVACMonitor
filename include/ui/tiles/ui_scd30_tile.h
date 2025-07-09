#ifndef UI_SCD30_TILE_H
#define UI_SCD30_TILE_H

#include "../ui_common.h"

class UISCD30Tile {
public:
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_autocal_state(bool enabled);
    void update_forcecal_value(uint16_t ppm);
    static void autocal_switch_cb(lv_event_t* e);
private:
    lv_obj_t* autocal_switch;
    lv_obj_t* forcecal_label;
};

#endif // UI_SCD30_TILE_H
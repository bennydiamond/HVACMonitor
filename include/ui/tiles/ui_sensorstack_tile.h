#ifndef UI_SENSORSTACK_TILE_H
#define UI_SENSORSTACK_TILE_H

#include "../ui_common.h"

class UISensorStackTile {
public:
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_sensorstack_info(const char* version, uint32_t uptime, uint16_t free_ram, bool connected);
private:
    lv_obj_t* version_label;
    lv_obj_t* uptime_label;
    lv_obj_t* ram_label;
    lv_obj_t* status_label;
    lv_obj_t* status_icon;
};

#endif // UI_SENSORSTACK_TILE_H
#ifndef UI_SENSORSTACK_TILE_H
#define UI_SENSORSTACK_TILE_H

#include "../ui_common.h"

class UISensorStackTile {
public:
    UISensorStackTile();
    ~UISensorStackTile();
    
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_sensorstack_uptime(uint32_t uptime);
    void update_sensorstack_ram(uint16_t ram);
    void update_sensor_status(bool connected);
    void update_fw_version(const char* fw);
    void set_visible(bool visible);
private:
    lv_obj_t* tile_obj;
    lv_obj_t* version_label;
    lv_obj_t* uptime_label;
    lv_obj_t* ram_label;
    lv_obj_t* status_label;
    lv_obj_t* status_icon;
};

#endif // UI_SENSORSTACK_TILE_H
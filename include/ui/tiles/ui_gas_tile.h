#ifndef UI_GAS_TILE_H
#define UI_GAS_TILE_H

#include "../ui_common.h"
#include "ConfigManager.h"

class UIGasTile {
public:
    UIGasTile();
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_no2(float no2);
    void update_o3(float o3);
    void update_nox(float nox);
    void update_co(float co);
    void clear_readings();
private:
    // ConfigManager pointer removed - using ConfigManagerAccessor instead
    lv_obj_t* no2_icon, *no2_label;
    lv_obj_t* o3_icon, *o3_label;
    lv_obj_t* nox_icon, *nox_label;
    lv_obj_t* co_icon, *co_label;
};

#endif // UI_GAS_TILE_H 
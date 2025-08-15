#ifndef UI_MAIN_TILE_H
#define UI_MAIN_TILE_H

#include "../ui_common.h"
#include "ConfigManager.h"

class UIMainTile {
public:
    UIMainTile();
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_pressure(float p);
    void update_co2(float co2);
    void update_voc(int32_t voc);
    void update_geiger_cpm(int cpm);
    void update_geiger_usvh(float usv);
    void update_temp(float temp);
    void update_humi(float humi);
    void clear_readings();
private:
    void create_pressure_widgets(lv_obj_t* parent);
    void create_co2_widgets(lv_obj_t* parent);
    void create_voc_widgets(lv_obj_t* parent);
    void create_geiger_widgets(lv_obj_t* parent);
    void create_temp_humi_widgets(lv_obj_t* parent);

    // ConfigManager pointer removed - using ConfigManagerAccessor instead
    lv_obj_t* pressure_icon, *pressure_label;
    lv_obj_t* co2_icon, *co2_label;
    lv_obj_t* voc_icon, *voc_label;
    lv_obj_t* usv_icon, *usv_label;
    lv_obj_t* temp_icon, *temp_label;
    lv_obj_t* humi_icon, *humi_label;
};

#endif // UI_MAIN_TILE_H
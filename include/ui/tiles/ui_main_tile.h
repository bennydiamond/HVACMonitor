#ifndef UI_MAIN_TILE_H
#define UI_MAIN_TILE_H

#include "../ui_common.h"
#include "ConfigManager.h"

class UIMainTile {
public:
    UIMainTile(ConfigManager* config);
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_pressure(float p);
    void update_co2(float co2);
    void update_voc(int32_t voc);
    void update_geiger(int cpm, float usv);
    void update_temp_humi(float temp, float humi);
    void clear_readings();
private:
    void create_pressure_widgets(lv_obj_t* parent);
    void create_co2_widgets(lv_obj_t* parent);
    void create_voc_widgets(lv_obj_t* parent);
    void create_geiger_widgets(lv_obj_t* parent);
    void create_temp_humi_widgets(lv_obj_t* parent);

    ConfigManager* _config;
    lv_obj_t* pressure_icon, *pressure_label;
    lv_obj_t* co2_icon, *co2_label;
    lv_obj_t* voc_icon, *voc_label;
    lv_obj_t* usv_icon, *usv_label;
    lv_obj_t* temp_icon, *temp_label;
    lv_obj_t* humi_icon, *humi_label;
};

#endif // UI_MAIN_TILE_H
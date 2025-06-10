#ifndef UI_TILE_MANAGER_H
#define UI_TILE_MANAGER_H

#include "ui_common.h"
#include "ConfigManager.h"
#include "tiles/ui_main_tile.h"
#include "tiles/ui_secondary_tile.h"
#include "tiles/ui_runtime_tile.h"
#include "tiles/ui_network_tile.h"

class UITileManager {
public:
    UITileManager();
    void create_all_tiles(lv_obj_t* parent_tv, ConfigManager* config, lv_obj_t* fan_status_icon);
    lv_obj_t* get_tile1() { return main_tile_obj; }
    lv_obj_t* get_tile3() { return runtime_tile_obj; }
    lv_obj_t* get_tile4() { return network_tile_obj; }
    void clear_all_readings();
    void update_pressure(float p);
    void update_co2(float co2);
    void update_voc(int32_t voc);
    void update_geiger(int cpm, float usv);
    void update_temp_humi(float temp, float humi);
    void update_pm_values(float pm1, float pm25, float pm4, float pm10);
    void update_fan_current(float amps, FanStatus status);
    void update_fan_status(FanStatus status);
    void set_initial_debug_info(const char* ver, const char* rst_reason);
    void update_runtime_info(uint32_t freemem, unsigned long uptime);
    void update_last_packet_time(uint32_t secs, bool connected);
    void update_network_info(const char* ip, const char* mac, int8_t rssi, const char* ssid, bool ha_conn);
private:
    UIMainTile* main_tile = nullptr;
    UISecondaryTile* secondary_tile = nullptr;
    UIRuntimeTile* runtime_tile = nullptr;
    UINetworkTile* network_tile = nullptr;
    lv_obj_t* main_tile_obj = nullptr;
    lv_obj_t* runtime_tile_obj = nullptr;
    lv_obj_t* network_tile_obj = nullptr;
};

#endif // UI_TILE_MANAGER_H
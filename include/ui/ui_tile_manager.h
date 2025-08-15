#ifndef UI_TILE_MANAGER_H
#define UI_TILE_MANAGER_H

#include "ui_common.h"
#include "ConfigManager.h"
#include "tiles/ui_main_tile.h"
#include "tiles/ui_secondary_tile.h"
#include "tiles/ui_gas_tile.h"
#include "tiles/ui_powerdraw_tile.h"
#include "tiles/ui_runtime_tile.h"
#include "tiles/ui_network_tile.h"
#include "tiles/ui_sensorstack_tile.h"
#include "tiles/ui_scd30_tile.h"
#include "tiles/ui_sps30_tile.h"
#include "tiles/ui_sgp41_tile.h"

class UITileManager {
public:
    UITileManager();
    void create_all_tiles(lv_obj_t* parent_tv, lv_obj_t* fan_status_icon);
    lv_obj_t* get_tile1() { return main_tile_obj; }
    lv_obj_t* get_secondary_tile() { return secondary_tile_obj; }
    lv_obj_t* get_gas_tile() { return gas_tile_obj; }
    lv_obj_t* get_powerdraw_tile() { return powerdraw_tile_obj; }
    lv_obj_t* get_tile3() { return runtime_tile_obj; }
    lv_obj_t* get_tile4() { return network_tile_obj; }
    lv_obj_t* get_sensorstack_tile() { return sensorstack_tile_obj; }
    lv_obj_t* get_scd30_tile() { return scd30_tile_obj; }
    lv_obj_t* get_sps30_tile() { return sps30_tile_obj; }
    lv_obj_t* get_sgp41_tile() { return sgp41_tile_obj; }
    void clear_all_readings();
    void update_pressure(float p);
    void update_co2(float co2);
    void update_voc(int32_t voc);
    void update_no2(float no2);
    void update_o3(float o3);
    void update_nox(float nox);
    void update_co(float co);
    void update_geiger_cpm(int cpm);
    void update_geiger_usvh(float usv);
    void update_temp(float temp);
    void update_humi(float humi);
    void update_pm1(float pm1);
    void update_pm25(float pm25);
    void update_pm4(float pm4);
    void update_pm10(float pm10);
    void update_fan_amps(float amps);
    void update_compressor_amps(float amps);
    void update_pump_amps(float amps);
    void update_water_sensor(bool water_ok);
    void update_fan_status(FanStatus status);
    void set_initial_debug_info(const char* ver, const char* rst_reason);
    void update_scd30_autocal(bool enabled);
    void update_scd30_forcecal(uint16_t ppm);
    void clear_sgp41_results();
    void update_sps30_fan_interval(unsigned long fan_interval);
    void update_sps30_fan_days(unsigned long fan_days);
    void update_sgp41_test_status(int ret_status);
    void update_sgp41_test_value(uint16_t raw_value);
    void update_runtime_free_heap(uint32_t free_heap);
    void update_runtime_uptime(unsigned long system_uptime);
    void update_sensorstack_uptime(uint32_t uptime);
    void update_sensorstack_ram(uint16_t ram);
    void update_network_rssi(int8_t rssi);
    void update_network_ha_conn(bool ha_conn);
    void update_sensor_status(bool connected);
    void update_last_packet_time(uint32_t seconds_since_packet);
    void update_ssid(const char* ssid);
    void update_ip(const char* ip);
    void update_mac(const char* mac);
    void update_fw_version(const char* fw);
private:
    UIMainTile* main_tile = nullptr;
    UISecondaryTile* secondary_tile = nullptr;
    UIGasTile* gas_tile = nullptr;
    UIPowerdrawTile* powerdraw_tile = nullptr;
    UIRuntimeTile* runtime_tile = nullptr;
    UINetworkTile* network_tile = nullptr;
    UISensorStackTile* sensorstack_tile = nullptr;
    UISCD30Tile* scd30_tile = nullptr;
    UISPS30Tile* sps30_tile = nullptr;
    UISGP41Tile* sgp41_tile = nullptr;
    lv_obj_t* main_tile_obj = nullptr;
    lv_obj_t* secondary_tile_obj = nullptr;
    lv_obj_t* gas_tile_obj = nullptr;
    lv_obj_t* powerdraw_tile_obj = nullptr;
    lv_obj_t* runtime_tile_obj = nullptr;
    lv_obj_t* network_tile_obj = nullptr;
    lv_obj_t* sensorstack_tile_obj = nullptr;
    lv_obj_t* scd30_tile_obj = nullptr;
    lv_obj_t* sps30_tile_obj = nullptr;
    lv_obj_t* sgp41_tile_obj = nullptr;
};

#endif // UI_TILE_MANAGER_H
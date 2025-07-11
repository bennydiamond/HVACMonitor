#include "ui/ui_tile_manager.h"

UITileManager::UITileManager() {}

void UITileManager::create_all_tiles(lv_obj_t* parent_tv, ConfigManager* config, lv_obj_t* fan_status_icon) {
    main_tile = new UIMainTile(config);
    main_tile_obj = main_tile->create_tile(parent_tv);

    secondary_tile = new UISecondaryTile(config);
    secondary_tile_obj = secondary_tile->create_tile(parent_tv);
    secondary_tile->set_fan_status_icon(fan_status_icon);

    runtime_tile = new UIRuntimeTile();
    runtime_tile_obj = runtime_tile->create_tile(parent_tv);
    
    network_tile = new UINetworkTile();
    network_tile_obj = network_tile->create_tile(parent_tv);
    
    sensorstack_tile = new UISensorStackTile();
    sensorstack_tile_obj = sensorstack_tile->create_tile(parent_tv);
    
    scd30_tile = new UISCD30Tile();
    scd30_tile_obj = scd30_tile->create_tile(parent_tv);
    
    sps30_tile = new UISPS30Tile();
    sps30_tile_obj = sps30_tile->create_tile(parent_tv);
    
    sgp40_tile = new UISGP40Tile();
    sgp40_tile_obj = sgp40_tile->create_tile(parent_tv);
}

void UITileManager::clear_all_readings() {
    if (main_tile) main_tile->clear_readings();
    if (secondary_tile) secondary_tile->clear_readings();
}

void UITileManager::update_pressure(float p) { if (main_tile) main_tile->update_pressure(p); }
void UITileManager::update_co2(float co2) { if (main_tile) main_tile->update_co2(co2); }
void UITileManager::update_voc(int32_t voc) { if (main_tile) main_tile->update_voc(voc); }
void UITileManager::update_geiger(int cpm, float usv) { if (main_tile) main_tile->update_geiger(cpm, usv); }
void UITileManager::update_temp_humi(float temp, float humi) { if (main_tile) main_tile->update_temp_humi(temp, humi); }
void UITileManager::update_pm_values(float p1, float p25, float p4, float p10) { if (secondary_tile) secondary_tile->update_pm_values(p1, p25, p4, p10); }
void UITileManager::update_fan_current(float amps, FanStatus status) { if (secondary_tile) secondary_tile->update_fan_current(amps, status); }
void UITileManager::update_fan_status(FanStatus status) { if (secondary_tile) secondary_tile->update_fan_status(status); }
void UITileManager::set_initial_debug_info(const char* ver, const char* rst_reason) { if (runtime_tile) runtime_tile->set_initial_info(ver, rst_reason); }
void UITileManager::update_runtime_info(uint32_t freemem, unsigned long uptime) { if (runtime_tile) runtime_tile->update_runtime_info(freemem, uptime); }
void UITileManager::update_last_packet_time(uint32_t secs, bool connected) { if (runtime_tile) runtime_tile->update_last_packet_time(secs, connected); }
void UITileManager::update_network_info(const char* ip, const char* mac, int8_t rssi, const char* ssid, bool ha_conn) { if (network_tile) network_tile->update_network_info(ip, mac, rssi, ssid, ha_conn); }
void UITileManager::update_sensorstack_info(const char* version, uint32_t uptime, uint16_t free_ram, bool connected) { if (sensorstack_tile) sensorstack_tile->update_sensorstack_info(version, uptime, free_ram, connected); }
void UITileManager::update_scd30_autocal(bool enabled) { if (scd30_tile) scd30_tile->update_autocal_state(enabled); }
void UITileManager::update_scd30_forcecal(uint16_t ppm) { if (scd30_tile) scd30_tile->update_forcecal_value(ppm); }
void UITileManager::update_sps30_info(uint32_t fan_interval, uint8_t fan_days) { if (sps30_tile) sps30_tile->update_sps30_info(fan_interval, fan_days); }
void UITileManager::update_sgp40_test(int result, uint16_t value) { if (sgp40_tile) sgp40_tile->update_test_result(result, value); }
void UITileManager::clear_sgp40_results() { if (sgp40_tile) sgp40_tile->clear_test_results(); }

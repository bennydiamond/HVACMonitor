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
    
    sgp41_tile = new UISGP41Tile();
    sgp41_tile_obj = sgp41_tile->create_tile(parent_tv);
}

void UITileManager::clear_all_readings() {
    if (main_tile) main_tile->clear_readings();
    if (secondary_tile) secondary_tile->clear_readings();
}

void UITileManager::update_pressure(float p) { if (main_tile) main_tile->update_pressure(p); }
void UITileManager::update_co2(float co2) { if (main_tile) main_tile->update_co2(co2); }
void UITileManager::update_voc(int32_t voc) { if (main_tile) main_tile->update_voc(voc); }
void UITileManager::update_fan_status(FanStatus status) { if (secondary_tile) secondary_tile->update_fan_status(status); }
void UITileManager::set_initial_debug_info(const char* ver, const char* rst_reason) { if (runtime_tile) runtime_tile->set_initial_info(ver, rst_reason); }
void UITileManager::update_scd30_autocal(bool enabled) { if (scd30_tile) scd30_tile->update_autocal_state(enabled); }
void UITileManager::update_scd30_forcecal(uint16_t ppm) { if (scd30_tile) scd30_tile->update_forcecal_value(ppm); }
void UITileManager::clear_sgp41_results() { if (sgp41_tile) sgp41_tile->clear_test_results(); }
void UITileManager::update_geiger_cpm(int cpm) { if (main_tile) main_tile->update_geiger_cpm(cpm); }
void UITileManager::update_geiger_usvh(float usv) { if (main_tile) main_tile->update_geiger_usvh(usv); }
void UITileManager::update_temp(float temp) { if (main_tile) main_tile->update_temp(temp); }
void UITileManager::update_humi(float humi) { if (main_tile) main_tile->update_humi(humi); }
void UITileManager::update_pm1(float pm1) { if (secondary_tile) secondary_tile->update_pm1(pm1); }
void UITileManager::update_pm25(float pm25) { if (secondary_tile) secondary_tile->update_pm25(pm25); }
void UITileManager::update_pm4(float pm4) { if (secondary_tile) secondary_tile->update_pm4(pm4); }
void UITileManager::update_pm10(float pm10) { if (secondary_tile) secondary_tile->update_pm10(pm10); }
void UITileManager::update_fan_amps(float amps) { if (secondary_tile) secondary_tile->update_fan_amps(amps); }
void UITileManager::update_sps30_fan_interval(unsigned long v) { if (sps30_tile) sps30_tile->update_sps30_fan_interval(v); }
void UITileManager::update_sps30_fan_days(unsigned long v) { if (sps30_tile) sps30_tile->update_sps30_fan_days(v); }
void UITileManager::update_sgp41_test_status(int v) { if (sgp41_tile) sgp41_tile->update_sgp41_test_status(v); }
void UITileManager::update_sgp41_test_value(uint16_t v) { if (sgp41_tile) sgp41_tile->update_sgp41_test_value(v); }
void UITileManager::update_runtime_free_heap(uint32_t v) { if (runtime_tile) runtime_tile->update_runtime_free_heap(v); }
void UITileManager::update_runtime_uptime(unsigned long v) { if (runtime_tile) runtime_tile->update_runtime_uptime(v); }
void UITileManager::update_sensorstack_uptime(uint32_t v) { if (sensorstack_tile) sensorstack_tile->update_sensorstack_uptime(v); }
void UITileManager::update_sensorstack_ram(uint16_t v) { if (sensorstack_tile) sensorstack_tile->update_sensorstack_ram(v); }
void UITileManager::update_network_rssi(int8_t v) { if (network_tile) network_tile->update_network_rssi(v); }
void UITileManager::update_network_ha_conn(bool v) { if (network_tile) network_tile->update_network_ha_conn(v); }
void UITileManager::update_sensor_status(bool v) { if (runtime_tile) runtime_tile->update_sensor_status(v); if (sensorstack_tile) sensorstack_tile->update_sensor_status(v); }
void UITileManager::update_last_packet_time(uint32_t v) { if (runtime_tile) runtime_tile->update_last_packet_time(v); }
void UITileManager::update_ssid(const char* ssid) { if (network_tile) network_tile->update_ssid(ssid); }
void UITileManager::update_ip(const char* ip) { if (network_tile) network_tile->update_ip(ip); }
void UITileManager::update_mac(const char* mac) { if (network_tile) network_tile->update_mac(mac); }
void UITileManager::update_fw_version(const char* fw) { if (sensorstack_tile) sensorstack_tile->update_fw_version(fw); }

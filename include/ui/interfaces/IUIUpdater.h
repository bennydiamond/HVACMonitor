#ifndef IUI_UPDATER_H
#define IUI_UPDATER_H

#include "ui/ui_common.h" // For FanStatus
#include <stdint.h>       // For standard integer types like int8_t, uint32_t

class IUIUpdater {
public:
    virtual ~IUIUpdater() = default;

    virtual void clearSensorReadings(void) = 0;
    virtual void update_pressure(float p) = 0;
    virtual void update_geiger_cpm(int cpm) = 0;
    virtual void update_geiger_usvh(float usv) = 0;
    virtual void update_temp(float temp) = 0;
    virtual void update_humi(float humi) = 0;
    virtual void update_pm1(float pm1) = 0;
    virtual void update_pm25(float pm25) = 0;
    virtual void update_pm4(float pm4) = 0;
    virtual void update_pm10(float pm10) = 0;
    virtual void update_fan_amps(float amps) = 0;
    virtual void update_compressor_amps(float amps) = 0;
    virtual void update_pump_amps(float amps) = 0;
    virtual void update_water_sensor(bool water_ok) = 0;
    virtual void update_fan_status(FanStatus status) = 0;
    virtual void update_high_pressure_status(bool is_high) = 0;
    virtual void update_wifi_status(bool connected, int8_t rssi) = 0;
    virtual void update_ha_status(bool connected) = 0;
    virtual void update_sensor_status(bool connected) = 0;
    virtual void update_co2(float co2_ppm) = 0;
    virtual void update_voc(int32_t voc_index) = 0;
    virtual void update_no2(float no2_ppb) = 0;
    virtual void update_o3(float o3_ppb) = 0;
    virtual void update_nox(float nox_ppb) = 0;
    virtual void update_co(float co_ppm) = 0;
    virtual void set_initial_debug_info(const char* version, const char* reason) = 0;
    virtual void update_sps30_fan_interval(unsigned long fan_interval) = 0;
    virtual void update_sps30_fan_days(unsigned long fan_days) = 0;
    virtual void update_sgp41_test_status(int ret_status) = 0;
    virtual void update_sgp41_test_value(uint16_t raw_value) = 0;
    virtual void update_runtime_free_heap(uint32_t free_heap) = 0;
    virtual void update_runtime_uptime(unsigned long system_uptime) = 0;
    virtual void update_sensorstack_uptime(uint32_t uptime) = 0;
    virtual void update_sensorstack_ram(uint16_t ram) = 0;
    virtual void update_scd30_autocal(bool enabled) = 0;
    virtual void update_scd30_forcecal(uint16_t value) = 0;
    virtual void update_network_rssi(int8_t rssi) = 0;
    virtual void update_network_ha_conn(bool ha_conn) = 0;
    virtual void update_last_packet_time(uint32_t seconds_since_packet) = 0;
    virtual void update_ssid(const char* ssid) = 0;
    virtual void update_ip(const char* ip) = 0;
    virtual void update_mac(const char* mac) = 0;
    virtual void update_fw_version(const char* fw) = 0;
};

#endif // IUI_UPDATER_H
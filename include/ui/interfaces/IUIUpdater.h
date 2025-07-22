#ifndef IUI_UPDATER_H
#define IUI_UPDATER_H

#include "ui/ui_common.h" // For FanStatus
#include <stdint.h>       // For standard integer types like int8_t, uint32_t

class IUIUpdater {
public:
    virtual ~IUIUpdater() = default;

    virtual void clearSensorReadings(void) = 0;
    virtual void update_pressure(float p) = 0;
    virtual void update_geiger_reading(int cpm, float usv) = 0;
    virtual void update_temp_humi(float temp, float humi) = 0;
    virtual void update_pm_values(float pm1, float pm25, float pm4, float pm10) = 0;
    virtual void update_fan_current(float amps, FanStatus status) = 0;
    virtual void update_fan_status(FanStatus status) = 0;
    virtual void update_high_pressure_status(bool is_high) = 0;
    virtual void update_wifi_status(bool connected, int8_t rssi) = 0;
    virtual void update_ha_status(bool connected) = 0;
    virtual void update_sensor_status(bool connected) = 0;
    virtual void update_co2(float co2_ppm) = 0;
    virtual void update_voc(int32_t voc_index) = 0;
    virtual void set_initial_debug_info(const char* version, const char* reason) = 0;
    virtual void update_runtime_info(uint32_t freemem, unsigned long uptime_ms) = 0;
    virtual void update_network_info(const char* ip, const char* mac, int8_t rssi, const char* ssid, bool ha_connected) = 0;
    virtual void update_last_packet_time(uint32_t seconds_since_packet, bool connected) = 0;
    virtual void update_sensorstack_info(const char* version, uint32_t uptime, uint16_t free_ram, bool connected) = 0;
    virtual void update_scd30_autocal(bool enabled) = 0;
    virtual void update_scd30_forcecal(uint16_t ppm) = 0;
    virtual void update_sps30_info(uint32_t fan_interval, uint8_t fan_days) = 0;
    virtual void update_sgp41_test(int result, uint16_t value) = 0;
};

#endif // IUI_UPDATER_H
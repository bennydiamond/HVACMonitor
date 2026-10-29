#pragma once

#include "BaseAccessor.h"
#include "SensorDefines.h"
#include "Logger.h"
#include <cstdint>
#include <cstring>
#include <cmath>

namespace SharedDataDefaults {
    // Control & configuration defaults
    constexpr bool SCD30_AUTOCAL_STATE = false;
    constexpr uint16_t SCD30_FORCECAL_VALUE = SensorDefines::SCD30_FORCECAL_MIN_PPM;
    constexpr bool BACKLIGHT_STATE = true;
    constexpr uint8_t BACKLIGHT_BRIGHTNESS = 255;
    constexpr int INACTIVITY_TIMER_DELAY = 30; // seconds
    constexpr AppLogLevel LOG_LEVEL = APP_LOG_INFO;

    // Fixed-length string buffer capacities
    constexpr size_t SENSOR_STACK_VERSION_BUFFER_SIZE = 16;
    constexpr size_t NANO_RESET_CAUSE_BUFFER_SIZE = 16;
    constexpr size_t WIFI_SSID_BUFFER_SIZE = 33;
    constexpr size_t WIFI_IP_BUFFER_SIZE = 16;
}

struct SharedStateData {
    // Control & configuration settings polled across tasks (MqttTask, WebServer, SensorStack, UITask)
    bool scd30AutoCalState = SharedDataDefaults::SCD30_AUTOCAL_STATE;
    bool scd30AutoCalForce = false;
    uint32_t scd30AutoCalVersion = 0;

    uint16_t scd30ForceCalValue = SharedDataDefaults::SCD30_FORCECAL_VALUE;
    bool scd30ForceCalForce = false;
    uint32_t scd30ForceCalVersion = 0;

    bool backlightState = SharedDataDefaults::BACKLIGHT_STATE;
    uint8_t backlightBrightness = SharedDataDefaults::BACKLIGHT_BRIGHTNESS;
    uint32_t backlightVersion = 0;

    int inactivityTimerDelay = SharedDataDefaults::INACTIVITY_TIMER_DELAY;
    bool inactivityTimerDelayForce = false;
    uint32_t inactivityTimerDelayVersion = 0;

    AppLogLevel logLevel = SharedDataDefaults::LOG_LEVEL;
    uint32_t logLevelVersion = 0;

    // Sensor Stack Status (from Nano)
    bool sensorConnectionStatus = false;
    uint32_t sensorConnectionStatusVersion = 0;

    char sensorStackVersion[SharedDataDefaults::SENSOR_STACK_VERSION_BUFFER_SIZE] = {0};
    bool sensorStackVersionUnavailable = false;
    uint32_t sensorStackVersionCount = 0;

    uint32_t sensorStackUptime = 0;
    bool sensorStackUptimeForce = false;
    uint32_t sensorStackUptimeVersion = 0;

    uint16_t sensorStackFreeRam = 0;
    uint32_t sensorStackFreeRamVersion = 0;

    char nanoResetCause[SharedDataDefaults::NANO_RESET_CAUSE_BUFFER_SIZE] = {0};
    uint32_t nanoResetCauseVersion = 0;

    // Environmental & Electrical Sensor Telemetry
    float pressure = 0.0f;
    float filter_pressure = 0.0f;
    int cpm = 0;
    float temp = 0.0f;
    float humi = 0.0f;
    float co2 = 0.0f;
    int32_t voc_index = 0;
    int32_t nox_index = 0;
    float fan_amps = 0.0f;
    float compressor_amps = 0.0f;
    float geothermal_pump_amps = 0.0f;
    bool liquid_level_sensor_state = false;
    float co_ppm = 0.0f;
    float pm1 = 0.0f;
    float pm25 = 0.0f;
    float pm4 = 0.0f;
    float pm10 = 0.0f;
    bool high_pressure = false;
    bool fan_status = false;
    uint32_t sensorDataVersion = 0;

    // ZMOD4510 Gas & AQI
    float o3_conc_ug_per_m3 = 0.0f;
    float no2_conc_ug_per_m3 = 0.0f;
    uint16_t fast_aqi = 0;
    uint16_t epa_aqi = 0;
    uint32_t zmodDataVersion = 0;

#ifdef BMP280_ENABLED
    float bmp_pressure_pa = 0.0f;
    float bmp_temp_c = 0.0f;
    uint32_t bmpDataVersion = 0;
#endif

#ifdef AHT20_ENABLED
    float aht_temp_c = 0.0f;
    float aht_humi_pct = 0.0f;
    uint32_t ahtDataVersion = 0;
#endif

    // WiFi & Network Status
    bool wifiConnected = false;
    int8_t wifiRssi = 0;
    char wifiSsid[SharedDataDefaults::WIFI_SSID_BUFFER_SIZE] = {0};
    char wifiIp[SharedDataDefaults::WIFI_IP_BUFFER_SIZE] = {0};
    uint32_t wifiStatusVersion = 0;

    // ESP32 System Health
    uint32_t esp32FreeHeap = 0;
    uint32_t esp32Uptime = 0;
    uint32_t esp32HealthVersion = 0;

    // MQTT connection status
    bool mqttConnected = false;
};

/**
 * @brief Thread-safe accessor inheriting from BaseAccessor, providing centralized
 *        read/write access to shared state across all tasks.
 */
class SharedDataAccessor : public BaseAccessor {
public:
    SharedDataAccessor();
    ~SharedDataAccessor() override = default;

    SharedStateData* operator->() { return &_state; }
    SharedStateData& operator*() { return _state; }
    const SharedStateData* operator->() const { return &_state; }
    const SharedStateData& operator*() const { return _state; }

    // SCD30 Calibration Controls
    bool getScd30AutoCalState() const { return _state.scd30AutoCalState; }
    void setScd30AutoCalState(bool state, bool force = false);

    uint16_t getScd30ForceCalValue() const { return _state.scd30ForceCalValue; }
    void setScd30ForceCalValue(uint16_t value, bool force = false);

    // Backlight Controls
    bool getBacklightState() const { return _state.backlightState; }
    uint8_t getBacklightBrightness() const { return _state.backlightBrightness; }
    void setBacklightState(bool state, uint8_t brightness);

    // Inactivity Timer Delay
    int getInactivityTimerDelay() const { return _state.inactivityTimerDelay; }
    void setInactivityTimerDelay(int delay, bool force = false);

    // Log Level
    AppLogLevel getLogLevel() const { return _state.logLevel; }
    void setLogLevel(AppLogLevel level);

    // MQTT Connection State
    bool isMqttConnected() const { return _state.mqttConnected; }
    void setMqttConnected(bool connected);

    // Sensor Telemetry updates
    void setSensorData(
        float pressure, int cpm, float temp, float humi,
        float co2, int32_t voc_index, int32_t nox_index,
        float amps,
        float pm1, float pm25, float pm4, float pm10,
        float compressor_amps, float geothermal_pump_amps, bool liquid_level_sensor_state,
        float co_ppm,
        float filter_pressure = 0.0f
    );
    void setHighPressureStatus(bool is_high);
    void setFanStatus(bool is_on);

    void setZmodData(float o3_conc_ug_per_m3, float no2_conc_ug_per_m3, uint16_t fast_aqi, uint16_t epa_aqi);

#ifdef BMP280_ENABLED
    void setBmpData(float pressure_pa, float temp_c);
#endif

#ifdef AHT20_ENABLED
    void setAhtData(float temp_c, float humi_pct);
#endif

    void setWifiStatus(bool connected, int8_t rssi, const char* ssid, const char* ip);
    void setSensorStackStatus(bool connected);
    void setSensorStackVersion(const char* version);
    void setSensorStackVersionUnavailable();
    void setSensorStackUptime(uint32_t uptime_seconds, bool force = false);
    void setSensorStackFreeRam(uint16_t free_ram);
    void setNanoResetCause(const char* cause);

    void setEsp32FreeHeap(uint32_t free_heap);
    void setEsp32Uptime(uint32_t uptime_seconds);

    // Static initialization of the underlying mutex
    static void init();

private:
    static SemaphoreHandle_t _sharedMutex;
    static StaticSemaphore_t _mutexBuffer;
    static SharedStateData _state;
};

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoHA.h>
#include "secrets.h"
#include "ConfigManager.h"
#include "SensorDefines.h"
#include "Logger.h"
#include "SensorDataAccessor.h"
#include "SystemStatusAccessor.h"
#include "utils/HADictionary.h"
#include <atomic>

// Binary sensor that can also report an unknown state, like HASensor::setValue(nullptr).
// After setUnknown(), the next setState() must use force: the cached state is unchanged.
class HABinarySensorWithUnknown : public HABinarySensor {
public:
    using HABinarySensor::HABinarySensor;
    bool setUnknown() {
        return publishOnDataTopic(AHATOFSTR(HAStateTopic), AHATOFSTR(HAStateNone), true);
    }
};

class HomeAssistantManager {
public:
    HomeAssistantManager();
    ~HomeAssistantManager();
    
    static HomeAssistantManager* getInstance() { return _instance; }

    void init(const char* firmware_version);

    bool isMqttConnected();

private:
    // FreeRTOS Task
    TaskHandle_t _taskHandle = nullptr;

    char _firmwareVersion[32] = {0};
    char _configurationUrl[56] = {0};

    // Tracking last published control values to HA
    bool _hasPublishedScd30AutoCal = false;
    bool _lastPublishedScd30AutoCal = false;
    uint8_t _lastSeenScd30AutoCalSeq = 0;
    bool _hasPublishedScd30ForceCal = false;
    uint16_t _lastPublishedScd30ForceCal = 0;
    uint8_t _lastSeenScd30ForceCalSeq = 0;
    bool _hasPublishedBacklight = false;
    bool _lastPublishedBacklightState = false;
    uint8_t _lastPublishedBacklightBrightness = 0;
    bool _hasPublishedInactivityDelay = false;
    int _lastPublishedInactivityDelay = 0;
    uint8_t _lastSeenInactivityDelaySeq = 0;
    uint8_t _lastSeenSensorStackUptimeSeq = 0;
    bool _hasPublishedLogLevel = false;
    AppLogLevel _lastPublishedLogLevel = APP_LOG_INFO;
    // Written by MqttTask, read by other tasks through isMqttConnected()
    std::atomic<bool> _lastMqttConnected{false};

    // Dedicated task functions
    static void taskFunction(void* pvParameters);
    void initMqtt();
    void taskLoop();
    void resetAllPublishTimes();

    // Entities grouped by what can make their readings unknown in Home Assistant
    enum EntityGroup : uint8_t {
        GROUP_STACK = 1 << 0,  // SensorStack readings without a fault report of their own
        GROUP_SCD30 = 1 << 1,  // Temperature, humidity, CO2
        GROUP_SPS30 = 1 << 2,  // PM
        GROUP_SGP41 = 1 << 3,  // VOC, NOx indexes
        GROUP_ZMOD = 1 << 4,   // O3, NO2, AQIs (I2C bridge)
        GROUP_BMP280 = 1 << 5, // (I2C bridge)
        GROUP_AHT20 = 1 << 6,  // (I2C bridge)
        GROUP_ALL = 0x7F,
    };
    // Groups currently published as unknown (cleared on MQTT reconnect, so they are
    // published again)
    uint8_t _unknownGroups = 0;
    // Publishes newly unknown groups once, and lets recovered ones publish at once
    void updateUnknownGroups(uint8_t unknown);
    void publishGroupsUnknown(uint8_t groups);
    void resetGroupPublishTimes(uint8_t groups);

    // MQTT and Home Assistant objects
    WiFiClient _wifiClient;
    HADevice _device;
    HAMqtt _mqtt;

    // Home Assistant Entities
    HASensorNumber _pressureSensor;
    HASensorNumber _filterPressureSensor;
    HALight _backlight;
    HASensorNumber _wifi_rssi;
    HASensor _wifi_ssid;
    HASensor _wifi_ip;
    HASensorNumber _geiger_cpm;
    HASensorNumber _geiger_dose;
    HASensorNumber _temperatureSensor;
    HASensorNumber _humiditySensor;
#ifdef BMP280_ENABLED
    HASensorNumber _bmp280PressureSensor;
    HASensorNumber _bmp280TemperatureSensor;
#endif
#ifdef AHT20_ENABLED
    HASensorNumber _aht20TemperatureSensor;
    HASensorNumber _aht20HumiditySensor;
#endif
    HABinarySensor _sensorStatus;
    HABinarySensorWithUnknown _highPressureSensor;
    HABinarySensorWithUnknown _fanStatus;
    HAButton _rebootButton; 
    HAButton _rebootSensorStackButton;
    HASensorNumber _pm1_0_sensor, _pm2_5_sensor, _pm4_0_sensor, _pm10_0_sensor;
    HASensorNumber _o3_sensor, _no2_sensor, _fast_aqi_sensor, _epa_aqi_sensor;
    HASensorNumber _co2_sensor;
    HASensorNumber _co_sensor;
    HASensorNumber _sensorStackUptimeSensor;
    HASensor _sensorStackVersionSensor;
    HASensorNumber _sensorStackFreeRamSensor;
    HASensorNumber _voc_index_sensor;
    HASensorNumber _nox_index_sensor;
    HASensorNumber _currentSensor;
    HASensorNumber _compressorCurrentSensor;
    HASensorNumber _geothermalPumpCurrentSensor;
    HABinarySensorWithUnknown _liquidLevelSensor;
    HASensor _sensorStackResetCauseSensor;
    HANumber _inactivityTimerDelay;
    HAButton _getSps30InfoButton;
    HAButton _Sps30ManualCleanButton;
    HAButton _getSgp41SelftestButton;
    HASelect _logLevelSelect;
    HASwitch _scd30AutoCalSwitch;
    HANumber _scd30ForceCalNumber;
    HASensorNumber _esp32FreeRamSensor;
    HASensorNumber _esp32UptimeSensor;

    // State tracking sentinels for uninitialized publish values
    static constexpr float UNPUBLISHED_FLOAT = -9999.0f;
    static constexpr int UNPUBLISHED_INT = -9999;
    static constexpr int32_t UNPUBLISHED_INT32 = -9999;
    static constexpr uint16_t UNPUBLISHED_AQI = 0xFFFF;

    // State tracking for publishing
    float _lastPublishedPressure = UNPUBLISHED_FLOAT;
    float _lastPublishedTemp = UNPUBLISHED_FLOAT;
    float _lastPublishedHumi = UNPUBLISHED_FLOAT;
    float _lastPublishedCo2 = UNPUBLISHED_FLOAT;
    float _lastPublishedCo = UNPUBLISHED_FLOAT;
    float _lastPublishedAmps = UNPUBLISHED_FLOAT;
    float _lastPublishedCompressorAmps = UNPUBLISHED_FLOAT;
    float _lastPublishedGeothermalPumpAmps = UNPUBLISHED_FLOAT;
    bool _lastPublishedLiquidLevelState = false;
    int _lastPublishedCpm = UNPUBLISHED_INT;
    int32_t _lastPublishedVocIndex = UNPUBLISHED_INT32;
    int32_t _lastPublishedNOxIndex = UNPUBLISHED_INT32;
#ifdef BMP280_ENABLED
    float _lastPublishedBMP280Pressure = UNPUBLISHED_FLOAT;
    float _lastPublishedBMP280Temperature = UNPUBLISHED_FLOAT;
#endif
#ifdef AHT20_ENABLED
    float _lastPublishedAHT20Temperature = UNPUBLISHED_FLOAT;
    float _lastPublishedAHT20Humidity = UNPUBLISHED_FLOAT;
#endif
    long _lastPublishedWifiRssi = 0;
    bool _lastPublishedWifiConnected = false, _lastPublishedSensorConnected = false, _lastPublishedHighPressure = false;
    bool _lastPublishedFanStatus = false;
    char _lastPublishedWifiSSID[SystemStatusDefaults::WIFI_SSID_BUFFER_SIZE] = {0};
    char _lastPublishedWifiIP[SystemStatusDefaults::WIFI_IP_BUFFER_SIZE] = {0};
    String _lastPublishedSensorStackVersion;
    // Empty: not published since the last (re)connect
    uint32_t _lastSeenNanoUpdateMs = 0;
    char _lastPublishedResetCause[SystemStatusDefaults::NANO_RESET_CAUSE_BUFFER_SIZE] = {0};
    float _lastPublishedPm1_0 = UNPUBLISHED_FLOAT;
    float _lastPublishedPm2_5 = UNPUBLISHED_FLOAT;
    float _lastPublishedPm4_0 = UNPUBLISHED_FLOAT;
    float _lastPublishedPm10_0 = UNPUBLISHED_FLOAT;
    float _lastPublishedO3 = UNPUBLISHED_FLOAT;
    float _lastPublishedNO2 = UNPUBLISHED_FLOAT;
    uint16_t _lastPublishedFastAQI = UNPUBLISHED_AQI;
    uint16_t _lastPublishedEPAAQI = UNPUBLISHED_AQI;
    float _lastPublishedFilterPressure = UNPUBLISHED_FLOAT;
    unsigned long _lastFilterPressurePublishTime = 0;

    // Warmup unavailable state tracking
    bool _geigerUnavailablePublished = false;
    bool _vocUnavailablePublished = false;
    bool _noxUnavailablePublished = false;
    bool _o3UnavailablePublished = false;
    bool _no2UnavailablePublished = false;
    bool _fastAqiUnavailablePublished = false;
    bool _epaAqiUnavailablePublished = false;
    bool _zmodUnavailablePublished = false;

    // Timestamps for periodic publishing
    unsigned long _lastPressurePublishTime = 0, _lastCpmPublishTime = 0, _lastTempPublishTime = 0, _lastHumiPublishTime = 0;
    unsigned long _LastO3PublishTime = 0, _lastNO2PublishTime = 0, _lastFastAQIPublishTime = 0, _lastEPAAQIPublishTime = 0;
    unsigned long _lastWifiStatusPublishTime = 0, _lastSensorStatusPublishTime = 0, _lastHighPressurePublishTime = 0;
    unsigned long _lastPmPublishTime = 0, _lastCo2PublishTime = 0, _lastVocPublishTime = 0, _lastNOxPublishTime = 0;
    unsigned long _lastCoPublishTime = 0;
    unsigned long _lastAmpsPublishTime = 0, _lastFanStatusPublishTime = 0;
#ifdef BMP280_ENABLED
    unsigned long _lastBMP280PressurePublishTime = 0;
    unsigned long _lastBMP280TemperaturePublishTime = 0;
#endif
#ifdef AHT20_ENABLED
    unsigned long _lastAHT20TemperaturePublishTime = 0;
    unsigned long _lastAHT20HumidityPublishTime = 0;
#endif
    unsigned long _lastSensorStackUptimePublishTime = 0;
    unsigned long _lastSensorStackFreeRamPublishTime = 0;
    unsigned long _lastEsp32FreeRamPublishTime = 0;
    unsigned long _lastEsp32UptimePublishTime = 0;
    unsigned long _lastCompressorAmpsPublishTime = 0;
    unsigned long _lastGeothermalPumpAmpsPublishTime = 0;
    unsigned long _lastLiquidLevelPublishTime = 0;
    unsigned long _lastInactivityTimerDelayPublishTime = 0;

    // Private sensor publish helpers
    void publishDifferentialPressure(float pressure, unsigned long currentTime);
    void publishFilterPressure(float filter_pressure, unsigned long currentTime);
    void publishGeiger(int cpm, unsigned long currentTime);
    void publishTemperature(float temp, unsigned long currentTime);
    void publishHumidity(float humi, unsigned long currentTime);
    void publishFanCurrent(float amps, float off_threshold, unsigned long currentTime);
    void publishCO2(float co2, unsigned long currentTime);
    // new_packet: co_ppm comes from a packet not seen before
    void publishCO(float co_ppm, bool new_packet, unsigned long currentTime);
    void publishVOC(int32_t voc_index, unsigned long currentTime);
    void publishNOx(int32_t nox_index, unsigned long currentTime);
    void publishPM(float pm1, float pm25, float pm4, float pm10, unsigned long currentTime);
    void publishCompressorCurrent(float compressor_amps, float off_threshold, unsigned long currentTime);
    void publishGeothermalPumpCurrent(float geothermal_pump_amps, float off_threshold, unsigned long currentTime);
    void publishLiquidLevel(bool liquid_level_sensor_state, unsigned long currentTime);
    void publishO3(float o3, unsigned long currentTime);
    void publishNO2(float no2, unsigned long currentTime);
    void publishFastAQI(uint16_t fast_aqi, unsigned long currentTime);
    void publishEPAAQI(uint16_t epa_aqi, unsigned long currentTime);
    void publishZmodUnavailable(unsigned long currentTime);
#ifdef BMP280_ENABLED
    void publishBMP280Pressure(float pressure_pa, unsigned long currentTime);
    void publishBMP280Temperature(float temp_c, unsigned long currentTime);
#endif
#ifdef AHT20_ENABLED
    void publishAHT20Temperature(float temp_c, unsigned long currentTime);
    void publishAHT20Humidity(float humi_pct, unsigned long currentTime);
#endif
    void publishHighPressure(bool is_high);
    void publishFan(bool is_on);
    void publishSensorStackUptimeHelper(uint32_t uptime_seconds, bool force);

    // Static callbacks for ArduinoHA
    static void onMqttConnected();
    static void onMqttDisconnected();
    static void onMqttStateChanged(HAMqtt::ConnectionState state);

    uint32_t _mqttFailedAttempts = 0;
    static void onBrightnessCommand(uint8_t brightness, HALight* sender);
    static void onStateCommand(bool state, HALight* sender);
    static void onRebootCommand(HAButton* sender);
    static void onRebootSensorStackCommand(HAButton* sender);
    static void onGetSps30InfoCommand(HAButton* sender);
    static void onGetSps30ManualCleanCommand(HAButton* sender);
    static void onGetSgp41SelftestCommand(HAButton* sender);
    static void onLogLevelCommand(int8_t index, HASelect* sender);
    static void onScd30AutoCalCommand(bool state, HASwitch* sender);
    static void onScd30ForceCalCommand(HANumeric number, HANumber* sender);
    static void onInactivityTimerDelayCommand(HANumeric number, HANumber* sender);

    static HomeAssistantManager* _instance;
};

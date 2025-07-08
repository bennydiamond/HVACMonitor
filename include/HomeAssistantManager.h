#pragma once

#include <ArduinoHA.h>
#include <WiFi.h>

// Forward declarations to avoid circular dependencies
class LGFX;
class IUIUpdater;
class ConfigManager;

class HomeAssistantManager {
public:
    HomeAssistantManager();
    // MODIFIED: Added firmware_version parameter
    void init(LGFX* tft, IUIUpdater* uiUpdater, ConfigManager* config, const char* firmware_version);
    void loop();

    // Data Publishing Methods
    void publishSensorData(
        float pressure, int cpm, float temp, float humi,
        float co2, int32_t voc_index, float amps,
        float pm1, float pm25, float pm4, float pm10
    );
    void publishWiFiStatus(bool connected, long rssi, const char* ssid, const char* ip);
    void publishSensorConnectionStatus(bool connected);
    void publishHighPressureStatus(bool is_high);
    void publishFanStatus(bool is_on); // Renamed from publishNanoVersion
    void publishSensorStackVersion(const char* version); // Renamed from publishNanoVersion
    void setSensorStackVersionUnavailable(); // Renamed from setNanoVersionUnavailable
    void publishSensorStackUptime(uint32_t uptime_seconds, bool force = false);
    void publishSensorStackFreeRam(uint16_t free_ram);
    void resetSensorStackUptimePublishTime();
    
    // Method to get the MQTT client for the logger
    HAMqtt* getMqtt();

    // New public method to check MQTT connection status
    bool isMqttConnected();

    void publishNanoResetCause(const char* cause);
    void updateScd30AutoCalState(bool state);
    void updateScd30ForceCalValue(uint16_t value);

private:
    // Pointers to external hardware/UI classes
    LGFX* _tft;
    IUIUpdater* _uiUpdater;
    ConfigManager* _config;
    
    // MQTT and Home Assistant objects
    WiFiClient _wifiClient;
    HADevice _device;
    HAMqtt _mqtt;

    // Home Assistant Entities
    HASensor _pressureSensor;
    HALight _backlight;
    HASensor _wifi_rssi;
    HASensor _wifi_ssid;
    HASensor _wifi_ip;
    HASensor _geiger_cpm;
    HASensor _geiger_dose;
    HASensor _temperatureSensor;
    HASensor _humiditySensor;
    HABinarySensor _sensorStatus;
    HABinarySensor _highPressureSensor;
    HABinarySensor _fanStatus;
    HAButton _rebootButton; 
    HAButton _rebootSensorStackButton;
    HASensor _pm1_0_sensor, _pm2_5_sensor, _pm4_0_sensor, _pm10_0_sensor;
    HASensor _co2_sensor;
    HASensorNumber _sensorStackUptimeSensor;
    HASensor _sensorStackVersionSensor;
    HASensorNumber _sensorStackFreeRamSensor;
    HASensor _voc_index_sensor;
    HASensor _currentSensor;
    HASensor _sensorStackResetCauseSensor;
    HAButton _getSps30InfoButton;
    HAButton _Sps30ManualCleanButton;
    HAButton _getSgp40SelftestButton;
    HASelect _logLevelSelect;
    HASwitch _scd30AutoCalSwitch;
    HANumber _scd30ForceCalNumber;

    // State tracking for publishing
    float _lastPublishedPressure, _lastPublishedTemp, _lastPublishedHumi, _lastPublishedCo2;
    float _lastPublishedAmps;
    int _lastPublishedCpm;
    int32_t _lastPublishedVocIndex;
    long _lastPublishedWifiRssi;
    bool _lastPublishedWifiConnected, _lastPublishedSensorConnected, _lastPublishedHighPressure;
    bool _lastPublishedFanStatus;
    String _lastPublishedSensorStackVersion; // Tracks the last version string published
    // _lastPublishedSensorStackUptime is no longer needed as HASensorNumber handles its internal state.
    float _lastPublishedPm1_0, _lastPublishedPm2_5, _lastPublishedPm4_0, _lastPublishedPm10_0;
    
    // Timestamps for periodic publishing
    unsigned long _lastPressurePublishTime, _lastCpmPublishTime, _lastTempPublishTime, _lastHumiPublishTime;
    unsigned long _lastWifiStatusPublishTime, _lastSensorStatusPublishTime, _lastHighPressurePublishTime;
    unsigned long _lastPmPublishTime, _lastCo2PublishTime, _lastVocPublishTime;
    unsigned long _lastAmpsPublishTime, _lastFanStatusPublishTime;
    unsigned long _lastSensorStackUptimePublishTime;

    // Static callbacks for ArduinoHA
    static void onMqttConnected();
    static void onMqttDisconnected();
    static void onBrightnessCommand(uint8_t brightness, HALight* sender);
    static void onStateCommand(bool state, HALight* sender);
    static void onRebootCommand(HAButton* sender);
    static void onRebootSensorStackCommand(HAButton* sender);
    static void onGetSps30InfoCommand(HAButton* sender);
    static void onGetSps30ManualCleanCommand(HAButton* sender);
    static void onGetSgp40SelftestCommand(HAButton* sender);
    static void onLogLevelCommand(int8_t index, HASelect* sender);
    static void onScd30AutoCalCommand(bool state, HASwitch* sender);
    static void onScd30ForceCalCommand(HANumeric number, HANumber* sender);

    
    // Static pointer to the class instance for callbacks
    static HomeAssistantManager* _instance;
};

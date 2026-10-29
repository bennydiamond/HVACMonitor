#pragma once

#include <Preferences.h>
#include "Logger.h"

namespace ConfigLimits {
    // Wi-Fi string limits
    inline constexpr size_t MAX_WIFI_SSID_LEN = 32;
    inline constexpr size_t MIN_WIFI_PASSWORD_LEN = 8;
    inline constexpr size_t MAX_WIFI_PASSWORD_LEN = 64;

    // MQTT connection and device limits
    inline constexpr size_t MAX_MQTT_HOST_LEN = 63;
    inline constexpr size_t MAX_MQTT_USER_LEN = 31;
    inline constexpr size_t MAX_MQTT_PASSWORD_LEN = 63;
    inline constexpr size_t MAX_MQTT_DEVICE_ID_LEN = 31;
    inline constexpr size_t MAX_MQTT_DEVICE_NAME_LEN = 63;
    inline constexpr int MIN_MQTT_PORT = 1;
    inline constexpr int MAX_MQTT_PORT = 65535;

    // Environmental gas and particle bounds
    inline constexpr int MIN_GAS_INDEX = 0;
    inline constexpr int MAX_GAS_INDEX = 1000; // NO2, O3, NOx, VOC index

    inline constexpr int MIN_CO_PPM = 0;
    inline constexpr int MAX_CO_PPM = 100;

    inline constexpr int MIN_PM_UG = 0;
    inline constexpr int MAX_PM_UG = 1000; // PM1.0, PM2.5, PM4.0, PM10 in µg/m³

    inline constexpr int MIN_CO2_PPM = 0;
    inline constexpr int MAX_CO2_PPM = 10000;

    // Physical sensor bounds
    inline constexpr float MIN_PRESSURE_PA = 0.0f;
    inline constexpr float MAX_PRESSURE_PA = 1000.0f;

    inline constexpr float MIN_CURRENT_A = 0.0f;
    inline constexpr float MAX_CURRENT_A = 100.0f; // Fan, Compressor, Pump

    inline constexpr float MIN_GEIGER_USV_H = 0.0f;
    inline constexpr float MAX_GEIGER_USV_H = 10.0f;

    inline constexpr float MIN_TEMP_C = -50.0f;
    inline constexpr float MAX_TEMP_C = 50.0f;

    inline constexpr float MIN_HUMI_PCT = 0.0f;
    inline constexpr float MAX_HUMI_PCT = 100.0f;

    // UI and system bounds
    inline constexpr int MIN_INACTIVITY_DELAY_S = 5;
    inline constexpr int MAX_INACTIVITY_DELAY_S = 120;
    inline constexpr int INACTIVITY_DELAY_STEP_S = 5;

    inline constexpr int MIN_LOG_LEVEL = 0;
    inline constexpr int MAX_LOG_LEVEL = 3;

    inline constexpr int MIN_DISPLAY_BRIGHTNESS = 0;
    inline constexpr int MAX_DISPLAY_BRIGHTNESS = 255;

    inline constexpr int MIN_SWITCH_STATE = 0;
    inline constexpr int MAX_SWITCH_STATE = 1;
}

namespace ConfigDefaults {
    // Application & System Defaults
    inline constexpr AppLogLevel LOG_LEVEL = APP_LOG_INFO;
    inline constexpr int INACTIVITY_DELAY = 30; // seconds

    // Physical Sensor & Equipment Threshold Defaults
    inline constexpr float HIGH_PRESSURE = 150.0f;
    inline constexpr float FAN_ON_CURRENT = 1.0f;
    inline constexpr float FAN_OFF_CURRENT = 0.1f;
    inline constexpr float FAN_HIGH_CURRENT = 2.5f;
    inline constexpr float COMPRESSOR_ON_CURRENT = 2.0f;
    inline constexpr float COMPRESSOR_OFF_CURRENT = 0.1f;
    inline constexpr float COMPRESSOR_HIGH_CURRENT = 15.0f;
    inline constexpr float PUMP_ON_CURRENT = 1.5f;
    inline constexpr float PUMP_OFF_CURRENT = 0.1f;
    inline constexpr float PUMP_HIGH_CURRENT = 5.0f;
    inline constexpr float PRESSURE_LOW = 70.0f;
    inline constexpr float PRESSURE_MID = 130.0f;
    inline constexpr float GEIGER_ABNORMAL_LOW = 0.04f;
    inline constexpr float GEIGER_ABNORMAL_HIGH = 0.30f;
    inline constexpr float GEIGER_DANGER_HIGH = 0.90f;

    // Environmental Target & Comfort Defaults
    inline constexpr float TEMP_COMFORTABLE_LOW = 15.0f;
    inline constexpr float TEMP_COMFORTABLE_HIGH = 25.0f;
    inline constexpr float TEMP_ACCEPTABLE_LOW = 10.0f;
    inline constexpr float TEMP_ACCEPTABLE_HIGH = 30.0f;
    inline constexpr float HUMI_COMFORTABLE_LOW = 30.0f;
    inline constexpr float HUMI_COMFORTABLE_HIGH = 60.0f;
    inline constexpr float HUMI_ACCEPTABLE_LOW = 20.0f;
    inline constexpr float HUMI_ACCEPTABLE_HIGH = 70.0f;

    // Air Quality Warning & Danger Threshold Defaults
    inline constexpr int CO2_WARN = 1000;
    inline constexpr int CO2_DANGER = 2000;
    inline constexpr int VOC_WARN = 200;
    inline constexpr int VOC_DANGER = 300;
    inline constexpr int NO2_WARN = 40;
    inline constexpr int NO2_DANGER = 100;
    inline constexpr int O3_WARN = 60;
    inline constexpr int O3_DANGER = 120;
    inline constexpr int NOX_WARN = 100;
    inline constexpr int NOX_DANGER = 200;
    inline constexpr int CO_WARN = 4;
    inline constexpr int CO_DANGER = 9;
    inline constexpr int PM1_WARN = 25;
    inline constexpr int PM1_DANGER = 35;
    inline constexpr int PM25_WARN = 25;
    inline constexpr int PM25_DANGER = 50;
    inline constexpr int PM4_WARN = 35;
    inline constexpr int PM4_DANGER = 60;
    inline constexpr int PM10_WARN = 50;
    inline constexpr int PM10_DANGER = 100;
}

class ConfigManager {
public:
    ConfigManager();
    void init();
    
    AppLogLevel getLogLevel();
    void setLogLevel(AppLogLevel level);

    // --- Getters for physical sensor thresholds ---
    float getHighPressureThreshold();
    float getFanOnCurrentThreshold();
    float getFanOffCurrentThreshold();
    float getFanHighCurrentThreshold();
    float getCompressorOnCurrentThreshold();
    float getCompressorOffCurrentThreshold();
    float getCompressorHighCurrentThreshold();
    float getPumpOnCurrentThreshold();
    float getPumpOffCurrentThreshold();
    float getPumpHighCurrentThreshold();

    // --- Getters for UI thresholds ---
    float getPressureLowThreshold();
    float getPressureMidThreshold();
    float getGeigerAbnormalLowThreshold();
    float getGeigerAbnormalHighThreshold();
    float getGeigerDangerHighThreshold();
    float getTempComfortableLow();
    float getTempComfortableHigh();
    float getTempAcceptableLow();
    float getTempAcceptableHigh();
    float getHumiComfortableLow();
    float getHumiComfortableHigh();
    float getHumiAcceptableLow();
    float getHumiAcceptableHigh();
    uint32_t getInactivityTimerDelay();
    const char* getWifiSsid();
    const char* getWifiPassword();
    const char* getMqttHost();
    int getMqttPort();
    const char* getMqttUser();
    const char* getMqttPassword();
    const char* getMqttDeviceId();
    const char* getMqttDeviceName();
    int getCo2WarnThreshold();
    int getCo2DangerThreshold();
    int getVocWarnThreshold();
    int getVocDangerThreshold();
    int getNo2WarnThreshold();
    int getNo2DangerThreshold();
    int getO3WarnThreshold();
    int getO3DangerThreshold();
    int getNoxWarnThreshold();
    int getNoxDangerThreshold();
    int getCoWarnThreshold();
    int getCoDangerThreshold();
    int getPm1WarnThreshold();
    int getPm1DangerThreshold();
    int getPm25WarnThreshold();
    int getPm25DangerThreshold();
    int getPm4WarnThreshold();
    int getPm4DangerThreshold();
    int getPm10WarnThreshold();
    int getPm10DangerThreshold();


    // --- Setters for physical sensor thresholds ---
    void setHighPressureThreshold(float value);
    void setFanOnCurrentThreshold(float value);
    void setFanOffCurrentThreshold(float value);
    void setFanHighCurrentThreshold(float value);
    void setCompressorOnCurrentThreshold(float value);
    void setCompressorOffCurrentThreshold(float value);
    void setCompressorHighCurrentThreshold(float value);
    void setPumpOnCurrentThreshold(float value);
    void setPumpOffCurrentThreshold(float value);
    void setPumpHighCurrentThreshold(float value);

    // --- Setters for UI thresholds ---
    void setPressureLowThreshold(float value);
    void setPressureMidThreshold(float value);
    void setGeigerAbnormalLowThreshold(float value);
    void setGeigerAbnormalHighThreshold(float value);
    void setGeigerDangerHighThreshold(float value);
    void setTempComfortableLow(float value);
    void setTempComfortableHigh(float value);
    void setTempAcceptableLow(float value);
    void setTempAcceptableHigh(float value);
    void setHumiComfortableLow(float value);
    void setHumiComfortableHigh(float value);
    void setHumiAcceptableLow(float value);
    void setHumiAcceptableHigh(float value);
    void setInactivityTimerDelay(int value);
    bool setWifiSsid(const char* value);
    bool setWifiPassword(const char* value);
    bool setWifiCredentials(const char* ssid, const char* password, bool updatePassword = true);
    void setMqttHost(const char* value);
    void setMqttPort(int value);
    void setMqttUser(const char* value);
    void setMqttPassword(const char* value);
    void setMqttDeviceId(const char* value);
    void setMqttDeviceName(const char* value);
    void setCo2WarnThreshold(int value);
    void setCo2DangerThreshold(int value);
    void setVocWarnThreshold(int value);
    void setVocDangerThreshold(int value);
    void setNo2WarnThreshold(int value);
    void setNo2DangerThreshold(int value);
    void setO3WarnThreshold(int value);
    void setO3DangerThreshold(int value);
    void setNoxWarnThreshold(int value);
    void setNoxDangerThreshold(int value);
    void setCoWarnThreshold(int value);
    void setCoDangerThreshold(int value);
    void setPm1WarnThreshold(int value);
    void setPm1DangerThreshold(int value);
    void setPm25WarnThreshold(int value);
    void setPm25DangerThreshold(int value);
    void setPm4WarnThreshold(int value);
    void setPm4DangerThreshold(int value);
    void setPm10WarnThreshold(int value);
    void setPm10DangerThreshold(int value);

protected:
    friend class ConfigManagerAccessor;
    SemaphoreHandle_t _mutex;
    static ConfigManager* _instance;

private:
    Preferences preferences;

    // In-memory cache
    AppLogLevel logLevel;
    float highPressureThreshold, fanOnCurrentThreshold, fanOffCurrentThreshold, fanHighCurrentThreshold;
    float compressorOnCurrentThreshold, compressorOffCurrentThreshold, compressorHighCurrentThreshold;
    float pumpOnCurrentThreshold, pumpOffCurrentThreshold, pumpHighCurrentThreshold;
    float pressureLowThreshold, pressureMidThreshold;
    float geigerAbnormalLowThreshold, geigerAbnormalHighThreshold, geigerDangerHighThreshold;
    float tempComfortableLow, tempComfortableHigh, tempAcceptableLow, tempAcceptableHigh;
    float humiComfortableLow, humiComfortableHigh, humiAcceptableLow, humiAcceptableHigh;
    int32_t inactivityTimerDelay;
    int co2WarnThreshold, co2DangerThreshold, vocWarnThreshold, vocDangerThreshold;
    int no2WarnThreshold, no2DangerThreshold, o3WarnThreshold, o3DangerThreshold, noxWarnThreshold, noxDangerThreshold, coWarnThreshold, coDangerThreshold;
    int pm1WarnThreshold, pm1DangerThreshold, pm25WarnThreshold, pm25DangerThreshold, pm4WarnThreshold, pm4DangerThreshold, pm10WarnThreshold, pm10DangerThreshold;
    char mqttHost[ConfigLimits::MAX_MQTT_HOST_LEN + 1];
    int mqttPort;
    char mqttUser[ConfigLimits::MAX_MQTT_USER_LEN + 1];
    char mqttPassword[ConfigLimits::MAX_MQTT_PASSWORD_LEN + 1];
    char mqttDeviceId[ConfigLimits::MAX_MQTT_DEVICE_ID_LEN + 1];
    char mqttDeviceName[ConfigLimits::MAX_MQTT_DEVICE_NAME_LEN + 1];
    char wifiSsid[ConfigLimits::MAX_WIFI_SSID_LEN + 1];
    char wifiPassword[ConfigLimits::MAX_WIFI_PASSWORD_LEN + 1];
    
    // NVS Keys
    static const char* KEY_LOG_LEVEL;
    static const char* KEY_HIGH_PRESSURE;
    static const char* KEY_FAN_ON;
    static const char* KEY_FAN_OFF;
    static const char* KEY_FAN_HIGH;
    static const char* KEY_COMPRESSOR_ON;
    static const char* KEY_COMPRESSOR_OFF;
    static const char* KEY_COMPRESSOR_HIGH;
    static const char* KEY_PUMP_ON;
    static const char* KEY_PUMP_OFF;
    static const char* KEY_PUMP_HIGH;
    static const char* KEY_P_LOW;
    static const char* KEY_P_MID;
    static const char* KEY_GEIGER_ABNORMAL_LOW;
    static const char* KEY_GEIGER_ABNORMAL_HIGH;
    static const char* KEY_GEIGER_DANGER_HIGH;
    static const char* KEY_TEMP_COMFORTABLE_LOW;
    static const char* KEY_TEMP_COMFORTABLE_HIGH;
    static const char* KEY_TEMP_ACCEPTABLE_LOW;
    static const char* KEY_TEMP_ACCEPTABLE_HIGH;
    static const char* KEY_HUMI_COMFORTABLE_LOW;
    static const char* KEY_HUMI_COMFORTABLE_HIGH;
    static const char* KEY_HUMI_ACCEPTABLE_LOW;
    static const char* KEY_HUMI_ACCEPTABLE_HIGH;
    static const char* KEY_INACTIVITY_TIMER_DELAY;
    static const char* KEY_WIFI_SSID;
    static const char* KEY_WIFI_PASSWORD;
    static const char* KEY_MQTT_HOST;
    static const char* KEY_MQTT_PORT;
    static const char* KEY_MQTT_USER;
    static const char* KEY_MQTT_PASSWORD;
    static const char* KEY_MQTT_DEVICE_ID;
    static const char* KEY_MQTT_DEVICE_NAME;
    static const char* KEY_CO2_WARN;
    static const char* KEY_CO2_DANGER;
    static const char* KEY_VOC_WARN;
    static const char* KEY_VOC_DANGER;
    static const char* KEY_NO2_WARN;
    static const char* KEY_NO2_DANGER;
    static const char* KEY_O3_WARN;
    static const char* KEY_O3_DANGER;
    static const char* KEY_NOX_WARN;
    static const char* KEY_NOX_DANGER;
    static const char* KEY_CO_WARN;
    static const char* KEY_CO_DANGER;
    static const char* KEY_PM1_WARN;
    static const char* KEY_PM1_DANGER;
    static const char* KEY_PM25_WARN;
    static const char* KEY_PM25_DANGER;
    static const char* KEY_PM4_WARN;
    static const char* KEY_PM4_DANGER;
    static const char* KEY_PM10_WARN;
    static const char* KEY_PM10_DANGER;
};

#include "BaseAccessor.h"

// Thread-safe accessor class for ConfigManager
class ConfigManagerAccessor : public BaseAccessor {
public:
    ConfigManagerAccessor();
    ~ConfigManagerAccessor() override = default;
    
    ConfigManager* operator->() { return &_instance; }
    ConfigManager& operator*() { return _instance; }
    
private:
    ConfigManager& _instance;
};

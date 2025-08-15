#pragma once

#include <Preferences.h>
#include "Logger.h"

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
    int getInactivityTimerDelay();
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
    int inactivityTimerDelay;
    int co2WarnThreshold, co2DangerThreshold, vocWarnThreshold, vocDangerThreshold;
    int no2WarnThreshold, no2DangerThreshold, o3WarnThreshold, o3DangerThreshold, noxWarnThreshold, noxDangerThreshold, coWarnThreshold, coDangerThreshold;
    int pm1WarnThreshold, pm1DangerThreshold, pm25WarnThreshold, pm25DangerThreshold, pm4WarnThreshold, pm4DangerThreshold, pm10WarnThreshold, pm10DangerThreshold;
    char mqttHost[64];
    int mqttPort;
    char mqttUser[32];
    char mqttPassword[64];
    char mqttDeviceId[32];
    char mqttDeviceName[64];
    
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
    
    // Default values
    static const AppLogLevel DEFAULT_LOG_LEVEL;
    static const float DEFAULT_HIGH_PRESSURE_THRESHOLD;
    static const float DEFAULT_FAN_ON_CURRENT_THRESHOLD;
    static const float DEFAULT_FAN_OFF_CURRENT_THRESHOLD;
    static const float DEFAULT_FAN_HIGH_CURRENT_THRESHOLD;
    static const float DEFAULT_COMPRESSOR_ON_CURRENT_THRESHOLD;
    static const float DEFAULT_COMPRESSOR_OFF_CURRENT_THRESHOLD;
    static const float DEFAULT_COMPRESSOR_HIGH_CURRENT_THRESHOLD;
    static const float DEFAULT_PUMP_ON_CURRENT_THRESHOLD;
    static const float DEFAULT_PUMP_OFF_CURRENT_THRESHOLD;
    static const float DEFAULT_PUMP_HIGH_CURRENT_THRESHOLD;
    static const float DEFAULT_PRESSURE_LOW_THRESHOLD;
    static const float DEFAULT_PRESSURE_MID_THRESHOLD;
    static const float DEFAULT_GEIGER_ABNORMAL_LOW_THRESHOLD;
    static const float DEFAULT_GEIGER_ABNORMAL_HIGH_THRESHOLD;
    static const float DEFAULT_GEIGER_DANGER_HIGH_THRESHOLD;
    static const float DEFAULT_TEMP_COMFORTABLE_LOW;
    static const float DEFAULT_TEMP_COMFORTABLE_HIGH;
    static const float DEFAULT_TEMP_ACCEPTABLE_LOW;
    static const float DEFAULT_TEMP_ACCEPTABLE_HIGH;
    static const float DEFAULT_HUMI_COMFORTABLE_LOW;
    static const float DEFAULT_HUMI_COMFORTABLE_HIGH;
    static const float DEFAULT_HUMI_ACCEPTABLE_LOW;
    static const float DEFAULT_HUMI_ACCEPTABLE_HIGH;
    static const int DEFAULT_INACTIVITY_TIMER_DELAY;
    static const char* DEFAULT_MQTT_HOST;
    static const int DEFAULT_MQTT_PORT;
    static const char* DEFAULT_MQTT_USER;
    static const char* DEFAULT_MQTT_PASSWORD;
    static const char* DEFAULT_MQTT_DEVICE_ID;
    static const char* DEFAULT_MQTT_DEVICE_NAME;
    static const int DEFAULT_CO2_WARN_THRESHOLD;
    static const int DEFAULT_CO2_DANGER_THRESHOLD;
    static const int DEFAULT_VOC_WARN_THRESHOLD;
    static const int DEFAULT_VOC_DANGER_THRESHOLD;
    static const int DEFAULT_NO2_WARN_THRESHOLD;
    static const int DEFAULT_NO2_DANGER_THRESHOLD;
    static const int DEFAULT_O3_WARN_THRESHOLD;
    static const int DEFAULT_O3_DANGER_THRESHOLD;
    static const int DEFAULT_NOX_WARN_THRESHOLD;
    static const int DEFAULT_NOX_DANGER_THRESHOLD;
    static const int DEFAULT_CO_WARN_THRESHOLD;
    static const int DEFAULT_CO_DANGER_THRESHOLD;
    static const int DEFAULT_PM1_WARN_THRESHOLD;
    static const int DEFAULT_PM1_DANGER_THRESHOLD;
    static const int DEFAULT_PM25_WARN_THRESHOLD;
    static const int DEFAULT_PM25_DANGER_THRESHOLD;
    static const int DEFAULT_PM4_WARN_THRESHOLD;
    static const int DEFAULT_PM4_DANGER_THRESHOLD;
    static const int DEFAULT_PM10_WARN_THRESHOLD;
    static const int DEFAULT_PM10_DANGER_THRESHOLD;
};

// Thread-safe accessor class for ConfigManager
class ConfigManagerAccessor {
public:
    ConfigManagerAccessor();
    ~ConfigManagerAccessor();
    
    ConfigManager* operator->() { return &_instance; }
    ConfigManager& operator*() { return _instance; }
    
private:
    ConfigManager& _instance;
    
    void lockMutex();
    void unlockMutex();
};

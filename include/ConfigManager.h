#pragma once

#include <Preferences.h>
#include "Logger.h"

class ConfigManager {
public:
    void init();

    AppLogLevel getLogLevel();
    void setLogLevel(AppLogLevel level);

    // --- Getters for physical sensor thresholds ---
    float getHighPressureThreshold();
    float getFanOnCurrentThreshold();
    float getFanOffCurrentThreshold();
    float getFanHighCurrentThreshold();

    // --- Getters for UI thresholds ---
    float getPressureLowThreshold();
    float getPressureMidThreshold();
    int getCpmWarnThreshold();
    int getCpmDangerThreshold();
    int getCo2WarnThreshold();
    int getCo2DangerThreshold();
    int getVocWarnThreshold();
    int getVocDangerThreshold();
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

    // --- Setters for UI thresholds ---
    void setPressureLowThreshold(float value);
    void setPressureMidThreshold(float value);
    void setCpmWarnThreshold(int value);
    void setCpmDangerThreshold(int value);
    void setCo2WarnThreshold(int value);
    void setCo2DangerThreshold(int value);
    void setVocWarnThreshold(int value);
    void setVocDangerThreshold(int value);
    void setPm1WarnThreshold(int value);
    void setPm1DangerThreshold(int value);
    void setPm25WarnThreshold(int value);
    void setPm25DangerThreshold(int value);
    void setPm4WarnThreshold(int value);
    void setPm4DangerThreshold(int value);
    void setPm10WarnThreshold(int value);
    void setPm10DangerThreshold(int value);


private:
    Preferences preferences;

    // In-memory cache
    AppLogLevel logLevel;
    float highPressureThreshold, fanOnCurrentThreshold, fanOffCurrentThreshold, fanHighCurrentThreshold;
    float pressureLowThreshold, pressureMidThreshold;
    int cpmWarnThreshold, cpmDangerThreshold, co2WarnThreshold, co2DangerThreshold, vocWarnThreshold, vocDangerThreshold;
    int pm1WarnThreshold, pm1DangerThreshold, pm25WarnThreshold, pm25DangerThreshold, pm4WarnThreshold, pm4DangerThreshold, pm10WarnThreshold, pm10DangerThreshold;
    
    // NVS Keys
    static const char* KEY_LOG_LEVEL;
    static const char* KEY_HIGH_PRESSURE;
    static const char* KEY_FAN_ON;
    static const char* KEY_FAN_OFF;
    static const char* KEY_FAN_HIGH;
    static const char* KEY_P_LOW;
    static const char* KEY_P_MID;
    static const char* KEY_CPM_WARN;
    static const char* KEY_CPM_DANGER;
    static const char* KEY_CO2_WARN;
    static const char* KEY_CO2_DANGER;
    static const char* KEY_VOC_WARN;
    static const char* KEY_VOC_DANGER;
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
    static const float DEFAULT_PRESSURE_LOW_THRESHOLD;
    static const float DEFAULT_PRESSURE_MID_THRESHOLD;
    static const int DEFAULT_CPM_WARN_THRESHOLD;
    static const int DEFAULT_CPM_DANGER_THRESHOLD;
    static const int DEFAULT_CO2_WARN_THRESHOLD;
    static const int DEFAULT_CO2_DANGER_THRESHOLD;
    static const int DEFAULT_VOC_WARN_THRESHOLD;
    static const int DEFAULT_VOC_DANGER_THRESHOLD;
    static const int DEFAULT_PM1_WARN_THRESHOLD;
    static const int DEFAULT_PM1_DANGER_THRESHOLD;
    static const int DEFAULT_PM25_WARN_THRESHOLD;
    static const int DEFAULT_PM25_DANGER_THRESHOLD;
    static const int DEFAULT_PM4_WARN_THRESHOLD;
    static const int DEFAULT_PM4_DANGER_THRESHOLD;
    static const int DEFAULT_PM10_WARN_THRESHOLD;
    static const int DEFAULT_PM10_DANGER_THRESHOLD;
};

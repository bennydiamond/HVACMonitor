#include "ConfigManager.h"
#include "Logger.h"
#include "secrets.h" 

// Static instance pointer
ConfigManager* ConfigManager::_instance = nullptr;

// Constructor - sets the static instance pointer and initializes mutex
ConfigManager::ConfigManager() {
    _instance = this;
    _mutex = xSemaphoreCreateRecursiveMutex();
}

// --- Define Keys ---
const char* ConfigManager::KEY_LOG_LEVEL = "log_level";
const char* ConfigManager::KEY_HIGH_PRESSURE = "p_high_thresh";
const char* ConfigManager::KEY_FAN_ON = "fan_on_thresh";
const char* ConfigManager::KEY_FAN_OFF = "fan_off_thresh";
const char* ConfigManager::KEY_FAN_HIGH = "fan_high_thresh";
const char* ConfigManager::KEY_COMPRESSOR_ON = "comp_on_thresh";
const char* ConfigManager::KEY_COMPRESSOR_OFF = "comp_off_thresh";
const char* ConfigManager::KEY_COMPRESSOR_HIGH = "comp_high_thresh";
const char* ConfigManager::KEY_PUMP_ON = "pump_on_thresh";
const char* ConfigManager::KEY_PUMP_OFF = "pump_off_thresh";
const char* ConfigManager::KEY_PUMP_HIGH = "pump_high_thresh";
const char* ConfigManager::KEY_P_LOW = "p_low_thresh";
const char* ConfigManager::KEY_P_MID = "p_mid_thresh";
const char* ConfigManager::KEY_GEIGER_ABNORMAL_LOW = "geiger_abnormal_low";
const char* ConfigManager::KEY_GEIGER_ABNORMAL_HIGH = "geiger_abnormal_high";
const char* ConfigManager::KEY_GEIGER_DANGER_HIGH = "geiger_danger_high";
const char* ConfigManager::KEY_TEMP_COMFORTABLE_LOW = "temp_comfortable_low";
const char* ConfigManager::KEY_TEMP_COMFORTABLE_HIGH = "temp_comfortable_high";
const char* ConfigManager::KEY_TEMP_ACCEPTABLE_LOW = "temp_acceptable_low";
const char* ConfigManager::KEY_TEMP_ACCEPTABLE_HIGH = "temp_acceptable_high";
const char* ConfigManager::KEY_HUMI_COMFORTABLE_LOW = "humi_comfortable_low";
const char* ConfigManager::KEY_HUMI_COMFORTABLE_HIGH = "humi_comfortable_high";
const char* ConfigManager::KEY_HUMI_ACCEPTABLE_LOW = "humi_acceptable_low";
const char* ConfigManager::KEY_HUMI_ACCEPTABLE_HIGH = "humi_acceptable_high";
const char* ConfigManager::KEY_INACTIVITY_TIMER_DELAY = "inactivity_timer_delay";
const char* ConfigManager::KEY_MQTT_HOST = "mqtt_host";
const char* ConfigManager::KEY_MQTT_PORT = "mqtt_port";
const char* ConfigManager::KEY_MQTT_USER = "mqtt_user";
const char* ConfigManager::KEY_MQTT_PASSWORD = "mqtt_password";
const char* ConfigManager::KEY_MQTT_DEVICE_ID = "mqtt_device_id";
const char* ConfigManager::KEY_MQTT_DEVICE_NAME = "mqtt_name";
const char* ConfigManager::KEY_CO2_WARN = "co2_warn_ui";
const char* ConfigManager::KEY_CO2_DANGER = "co2_danger_ui";
const char* ConfigManager::KEY_VOC_WARN = "voc_warn_ui";
const char* ConfigManager::KEY_VOC_DANGER = "voc_danger_ui";
const char* ConfigManager::KEY_NO2_WARN = "no2_warn_ui";
const char* ConfigManager::KEY_NO2_DANGER = "no2_danger_ui";
const char* ConfigManager::KEY_O3_WARN = "o3_warn_ui";
const char* ConfigManager::KEY_O3_DANGER = "o3_danger_ui";
const char* ConfigManager::KEY_NOX_WARN = "nox_warn_ui";
const char* ConfigManager::KEY_NOX_DANGER = "nox_danger_ui";
const char* ConfigManager::KEY_CO_WARN = "co_warn_ui";
const char* ConfigManager::KEY_CO_DANGER = "co_danger_ui";
const char* ConfigManager::KEY_PM1_WARN = "pm1_warn_ui";
const char* ConfigManager::KEY_PM1_DANGER = "pm1_danger_ui";
const char* ConfigManager::KEY_PM25_WARN = "pm25_warn_ui";
const char* ConfigManager::KEY_PM25_DANGER = "pm25_danger_ui";
const char* ConfigManager::KEY_PM4_WARN = "pm4_warn_ui";
const char* ConfigManager::KEY_PM4_DANGER = "pm4_danger_ui";
const char* ConfigManager::KEY_PM10_WARN = "pm10_warn_ui";
const char* ConfigManager::KEY_PM10_DANGER = "pm10_danger_ui";


// --- Define Default Values ---
const AppLogLevel ConfigManager::DEFAULT_LOG_LEVEL = APP_LOG_INFO;
const float ConfigManager::DEFAULT_HIGH_PRESSURE_THRESHOLD = 150.0f;
const float ConfigManager::DEFAULT_FAN_ON_CURRENT_THRESHOLD = 1.0f;
const float ConfigManager::DEFAULT_FAN_OFF_CURRENT_THRESHOLD = 0.1f;
const float ConfigManager::DEFAULT_FAN_HIGH_CURRENT_THRESHOLD = 2.5f;
const float ConfigManager::DEFAULT_COMPRESSOR_ON_CURRENT_THRESHOLD = 2.0f;
const float ConfigManager::DEFAULT_COMPRESSOR_OFF_CURRENT_THRESHOLD = 0.1f;
const float ConfigManager::DEFAULT_COMPRESSOR_HIGH_CURRENT_THRESHOLD = 15.0f;
const float ConfigManager::DEFAULT_PUMP_ON_CURRENT_THRESHOLD = 1.5f;
const float ConfigManager::DEFAULT_PUMP_OFF_CURRENT_THRESHOLD = 0.1f;
const float ConfigManager::DEFAULT_PUMP_HIGH_CURRENT_THRESHOLD = 5.0f;
const float ConfigManager::DEFAULT_PRESSURE_LOW_THRESHOLD = 70.0f;
const float ConfigManager::DEFAULT_PRESSURE_MID_THRESHOLD = 130.0f;
const float ConfigManager::DEFAULT_GEIGER_ABNORMAL_LOW_THRESHOLD = 0.04f;
const float ConfigManager::DEFAULT_GEIGER_ABNORMAL_HIGH_THRESHOLD = 0.30f;
const float ConfigManager::DEFAULT_GEIGER_DANGER_HIGH_THRESHOLD = 0.90f;
const float ConfigManager::DEFAULT_TEMP_COMFORTABLE_LOW = 15.0f;
const float ConfigManager::DEFAULT_TEMP_COMFORTABLE_HIGH = 25.0f;
const float ConfigManager::DEFAULT_TEMP_ACCEPTABLE_LOW = 10.0f;
const float ConfigManager::DEFAULT_TEMP_ACCEPTABLE_HIGH = 30.0f;
const float ConfigManager::DEFAULT_HUMI_COMFORTABLE_LOW = 30.0f;
const float ConfigManager::DEFAULT_HUMI_COMFORTABLE_HIGH = 60.0f;
const float ConfigManager::DEFAULT_HUMI_ACCEPTABLE_LOW = 20.0f;
const float ConfigManager::DEFAULT_HUMI_ACCEPTABLE_HIGH = 70.0f;
const int ConfigManager::DEFAULT_INACTIVITY_TIMER_DELAY = 30; // 30 seconds
const char* ConfigManager::DEFAULT_MQTT_HOST = MQTT_HOST;
const int ConfigManager::DEFAULT_MQTT_PORT = MQTT_PORT;
const char* ConfigManager::DEFAULT_MQTT_USER = MQTT_USER;
const char* ConfigManager::DEFAULT_MQTT_PASSWORD = MQTT_PASSWORD;
const char* ConfigManager::DEFAULT_MQTT_DEVICE_ID = MQTT_DEVICE_ID;
const char* ConfigManager::DEFAULT_MQTT_DEVICE_NAME = MQTT_DEVICE_NAME;
const int ConfigManager::DEFAULT_CO2_WARN_THRESHOLD = 1000;
const int ConfigManager::DEFAULT_CO2_DANGER_THRESHOLD = 2000;
const int ConfigManager::DEFAULT_VOC_WARN_THRESHOLD = 200;
const int ConfigManager::DEFAULT_VOC_DANGER_THRESHOLD = 300;
const int ConfigManager::DEFAULT_NO2_WARN_THRESHOLD = 40;
const int ConfigManager::DEFAULT_NO2_DANGER_THRESHOLD = 100;
const int ConfigManager::DEFAULT_O3_WARN_THRESHOLD = 60;
const int ConfigManager::DEFAULT_O3_DANGER_THRESHOLD = 120;
const int ConfigManager::DEFAULT_NOX_WARN_THRESHOLD = 100;
const int ConfigManager::DEFAULT_NOX_DANGER_THRESHOLD = 200;
const int ConfigManager::DEFAULT_CO_WARN_THRESHOLD = 4;
const int ConfigManager::DEFAULT_CO_DANGER_THRESHOLD = 9;
const int ConfigManager::DEFAULT_PM1_WARN_THRESHOLD = 25;
const int ConfigManager::DEFAULT_PM1_DANGER_THRESHOLD = 35;
const int ConfigManager::DEFAULT_PM25_WARN_THRESHOLD = 25;
const int ConfigManager::DEFAULT_PM25_DANGER_THRESHOLD = 50;
const int ConfigManager::DEFAULT_PM4_WARN_THRESHOLD = 35;
const int ConfigManager::DEFAULT_PM4_DANGER_THRESHOLD = 60;
const int ConfigManager::DEFAULT_PM10_WARN_THRESHOLD = 50;
const int ConfigManager::DEFAULT_PM10_DANGER_THRESHOLD = 100;


void ConfigManager::init() {
    if (!preferences.begin("hvac-config", false)) {
        logger.error("Failed to initialize NVS for ConfigManager. Using default values.");
        // Manually set defaults if NVS fails to open
        logLevel = DEFAULT_LOG_LEVEL;
        highPressureThreshold = DEFAULT_HIGH_PRESSURE_THRESHOLD;
        fanOnCurrentThreshold = DEFAULT_FAN_ON_CURRENT_THRESHOLD;
        fanOffCurrentThreshold = DEFAULT_FAN_OFF_CURRENT_THRESHOLD;
        fanHighCurrentThreshold = DEFAULT_FAN_HIGH_CURRENT_THRESHOLD;
        compressorOnCurrentThreshold = DEFAULT_COMPRESSOR_ON_CURRENT_THRESHOLD;
        compressorOffCurrentThreshold = DEFAULT_COMPRESSOR_OFF_CURRENT_THRESHOLD;
        compressorHighCurrentThreshold = DEFAULT_COMPRESSOR_HIGH_CURRENT_THRESHOLD;
        pumpOnCurrentThreshold = DEFAULT_PUMP_ON_CURRENT_THRESHOLD;
        pumpOffCurrentThreshold = DEFAULT_PUMP_OFF_CURRENT_THRESHOLD;
        pumpHighCurrentThreshold = DEFAULT_PUMP_HIGH_CURRENT_THRESHOLD;
        pressureLowThreshold = DEFAULT_PRESSURE_LOW_THRESHOLD;
        pressureMidThreshold = DEFAULT_PRESSURE_MID_THRESHOLD;
        geigerAbnormalLowThreshold = DEFAULT_GEIGER_ABNORMAL_LOW_THRESHOLD;
        geigerAbnormalHighThreshold = DEFAULT_GEIGER_ABNORMAL_HIGH_THRESHOLD;
        geigerDangerHighThreshold = DEFAULT_GEIGER_DANGER_HIGH_THRESHOLD;
        tempComfortableLow = DEFAULT_TEMP_COMFORTABLE_LOW;
        tempComfortableHigh = DEFAULT_TEMP_COMFORTABLE_HIGH;
        tempAcceptableLow = DEFAULT_TEMP_ACCEPTABLE_LOW;
        tempAcceptableHigh = DEFAULT_TEMP_ACCEPTABLE_HIGH;
        humiComfortableLow = DEFAULT_HUMI_COMFORTABLE_LOW;
        humiComfortableHigh = DEFAULT_HUMI_COMFORTABLE_HIGH;
        humiAcceptableLow = DEFAULT_HUMI_ACCEPTABLE_LOW;
        humiAcceptableHigh = DEFAULT_HUMI_ACCEPTABLE_HIGH;
        inactivityTimerDelay = DEFAULT_INACTIVITY_TIMER_DELAY;
        co2WarnThreshold = DEFAULT_CO2_WARN_THRESHOLD;
        co2DangerThreshold = DEFAULT_CO2_DANGER_THRESHOLD;
        vocWarnThreshold = DEFAULT_VOC_WARN_THRESHOLD;
        vocDangerThreshold = DEFAULT_VOC_DANGER_THRESHOLD;
        no2WarnThreshold = DEFAULT_NO2_WARN_THRESHOLD;
        no2DangerThreshold = DEFAULT_NO2_DANGER_THRESHOLD;
        o3WarnThreshold = DEFAULT_O3_WARN_THRESHOLD;
        o3DangerThreshold = DEFAULT_O3_DANGER_THRESHOLD;
        noxWarnThreshold = DEFAULT_NOX_WARN_THRESHOLD;
        noxDangerThreshold = DEFAULT_NOX_DANGER_THRESHOLD;
        coWarnThreshold = DEFAULT_CO_WARN_THRESHOLD;
        coDangerThreshold = DEFAULT_CO_DANGER_THRESHOLD;
        pm1WarnThreshold = DEFAULT_PM1_WARN_THRESHOLD;
        pm1DangerThreshold = DEFAULT_PM1_DANGER_THRESHOLD;
        pm25WarnThreshold = DEFAULT_PM25_WARN_THRESHOLD;
        pm25DangerThreshold = DEFAULT_PM25_DANGER_THRESHOLD;
        pm4WarnThreshold = DEFAULT_PM4_WARN_THRESHOLD;
        pm4DangerThreshold = DEFAULT_PM4_DANGER_THRESHOLD;
        pm10WarnThreshold = DEFAULT_PM10_WARN_THRESHOLD;
        pm10DangerThreshold = DEFAULT_PM10_DANGER_THRESHOLD;
        strcpy(mqttHost, DEFAULT_MQTT_HOST);
        mqttPort = DEFAULT_MQTT_PORT;
        strcpy(mqttUser, DEFAULT_MQTT_USER);
        strcpy(mqttPassword, DEFAULT_MQTT_PASSWORD);
        strcpy(mqttDeviceId, DEFAULT_MQTT_DEVICE_ID);
        strcpy(mqttDeviceName, DEFAULT_MQTT_DEVICE_NAME);
        return;
    }

    logLevel = static_cast<AppLogLevel>(preferences.getUChar(KEY_LOG_LEVEL, DEFAULT_LOG_LEVEL));

    logger.info("ConfigManager initializing and loading values from NVS...");

    // Load physical thresholds
    highPressureThreshold = preferences.getFloat(KEY_HIGH_PRESSURE, DEFAULT_HIGH_PRESSURE_THRESHOLD);
    fanOnCurrentThreshold = preferences.getFloat(KEY_FAN_ON, DEFAULT_FAN_ON_CURRENT_THRESHOLD);
    fanOffCurrentThreshold = preferences.getFloat(KEY_FAN_OFF, DEFAULT_FAN_OFF_CURRENT_THRESHOLD);
    fanHighCurrentThreshold = preferences.getFloat(KEY_FAN_HIGH, DEFAULT_FAN_HIGH_CURRENT_THRESHOLD);
    compressorOnCurrentThreshold = preferences.getFloat(KEY_COMPRESSOR_ON, DEFAULT_COMPRESSOR_ON_CURRENT_THRESHOLD);
    compressorOffCurrentThreshold = preferences.getFloat(KEY_COMPRESSOR_OFF, DEFAULT_COMPRESSOR_OFF_CURRENT_THRESHOLD);
    compressorHighCurrentThreshold = preferences.getFloat(KEY_COMPRESSOR_HIGH, DEFAULT_COMPRESSOR_HIGH_CURRENT_THRESHOLD);
    pumpOnCurrentThreshold = preferences.getFloat(KEY_PUMP_ON, DEFAULT_PUMP_ON_CURRENT_THRESHOLD);
    pumpOffCurrentThreshold = preferences.getFloat(KEY_PUMP_OFF, DEFAULT_PUMP_OFF_CURRENT_THRESHOLD);
    pumpHighCurrentThreshold = preferences.getFloat(KEY_PUMP_HIGH, DEFAULT_PUMP_HIGH_CURRENT_THRESHOLD);
    
    // Load UI thresholds
    pressureLowThreshold = preferences.getFloat(KEY_P_LOW, DEFAULT_PRESSURE_LOW_THRESHOLD);
    pressureMidThreshold = preferences.getFloat(KEY_P_MID, DEFAULT_PRESSURE_MID_THRESHOLD);
    geigerAbnormalLowThreshold = preferences.getFloat(KEY_GEIGER_ABNORMAL_LOW, DEFAULT_GEIGER_ABNORMAL_LOW_THRESHOLD);
    geigerAbnormalHighThreshold = preferences.getFloat(KEY_GEIGER_ABNORMAL_HIGH, DEFAULT_GEIGER_ABNORMAL_HIGH_THRESHOLD);
    geigerDangerHighThreshold = preferences.getFloat(KEY_GEIGER_DANGER_HIGH, DEFAULT_GEIGER_DANGER_HIGH_THRESHOLD);
    tempComfortableLow = preferences.getFloat(KEY_TEMP_COMFORTABLE_LOW, DEFAULT_TEMP_COMFORTABLE_LOW);
    tempComfortableHigh = preferences.getFloat(KEY_TEMP_COMFORTABLE_HIGH, DEFAULT_TEMP_COMFORTABLE_HIGH);
    tempAcceptableLow = preferences.getFloat(KEY_TEMP_ACCEPTABLE_LOW, DEFAULT_TEMP_ACCEPTABLE_LOW);
    tempAcceptableHigh = preferences.getFloat(KEY_TEMP_ACCEPTABLE_HIGH, DEFAULT_TEMP_ACCEPTABLE_HIGH);
    humiComfortableLow = preferences.getFloat(KEY_HUMI_COMFORTABLE_LOW, DEFAULT_HUMI_COMFORTABLE_LOW);
    humiComfortableHigh = preferences.getFloat(KEY_HUMI_COMFORTABLE_HIGH, DEFAULT_HUMI_COMFORTABLE_HIGH);
    humiAcceptableLow = preferences.getFloat(KEY_HUMI_ACCEPTABLE_LOW, DEFAULT_HUMI_ACCEPTABLE_LOW);
    humiAcceptableHigh = preferences.getFloat(KEY_HUMI_ACCEPTABLE_HIGH, DEFAULT_HUMI_ACCEPTABLE_HIGH);
    
    inactivityTimerDelay = preferences.getInt(KEY_INACTIVITY_TIMER_DELAY, DEFAULT_INACTIVITY_TIMER_DELAY);
    co2WarnThreshold = preferences.getInt(KEY_CO2_WARN, DEFAULT_CO2_WARN_THRESHOLD);
    co2DangerThreshold = preferences.getInt(KEY_CO2_DANGER, DEFAULT_CO2_DANGER_THRESHOLD);
    vocWarnThreshold = preferences.getInt(KEY_VOC_WARN, DEFAULT_VOC_WARN_THRESHOLD);
    vocDangerThreshold = preferences.getInt(KEY_VOC_DANGER, DEFAULT_VOC_DANGER_THRESHOLD);
    no2WarnThreshold = preferences.getInt(KEY_NO2_WARN, DEFAULT_NO2_WARN_THRESHOLD);
    no2DangerThreshold = preferences.getInt(KEY_NO2_DANGER, DEFAULT_NO2_DANGER_THRESHOLD);
    o3WarnThreshold = preferences.getInt(KEY_O3_WARN, DEFAULT_O3_WARN_THRESHOLD);
    o3DangerThreshold = preferences.getInt(KEY_O3_DANGER, DEFAULT_O3_DANGER_THRESHOLD);
    noxWarnThreshold = preferences.getInt(KEY_NOX_WARN, DEFAULT_NOX_WARN_THRESHOLD);
    noxDangerThreshold = preferences.getInt(KEY_NOX_DANGER, DEFAULT_NOX_DANGER_THRESHOLD);
    coWarnThreshold = preferences.getInt(KEY_CO_WARN, DEFAULT_CO_WARN_THRESHOLD);
    coDangerThreshold = preferences.getInt(KEY_CO_DANGER, DEFAULT_CO_DANGER_THRESHOLD);
    pm1WarnThreshold = preferences.getInt(KEY_PM1_WARN, DEFAULT_PM1_WARN_THRESHOLD);
    pm1DangerThreshold = preferences.getInt(KEY_PM1_DANGER, DEFAULT_PM1_DANGER_THRESHOLD);
    pm25WarnThreshold = preferences.getInt(KEY_PM25_WARN, DEFAULT_PM25_WARN_THRESHOLD);
    pm25DangerThreshold = preferences.getInt(KEY_PM25_DANGER, DEFAULT_PM25_DANGER_THRESHOLD);
    pm4WarnThreshold = preferences.getInt(KEY_PM4_WARN, DEFAULT_PM4_WARN_THRESHOLD);
    pm4DangerThreshold = preferences.getInt(KEY_PM4_DANGER, DEFAULT_PM4_DANGER_THRESHOLD);
    pm10WarnThreshold = preferences.getInt(KEY_PM10_WARN, DEFAULT_PM10_WARN_THRESHOLD);
    pm10DangerThreshold = preferences.getInt(KEY_PM10_DANGER, DEFAULT_PM10_DANGER_THRESHOLD);
    
    // Load MQTT configuration
    if (preferences.getString(KEY_MQTT_HOST, mqttHost, sizeof(mqttHost)) == 0) {
        snprintf(mqttHost, sizeof(mqttHost), "%s", DEFAULT_MQTT_HOST);
    }
    mqttPort = preferences.getInt(KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
    if (preferences.getString(KEY_MQTT_USER, mqttUser, sizeof(mqttUser)) == 0) {
        snprintf(mqttUser, sizeof(mqttUser), "%s", DEFAULT_MQTT_USER);
    }
    if (preferences.getString(KEY_MQTT_PASSWORD, mqttPassword, sizeof(mqttPassword)) == 0) {
        snprintf(mqttPassword, sizeof(mqttPassword), "%s", DEFAULT_MQTT_PASSWORD);
    }
    if (preferences.getString(KEY_MQTT_DEVICE_ID, mqttDeviceId, sizeof(mqttDeviceId)) == 0) {
        snprintf(mqttDeviceId, sizeof(mqttDeviceId), "%s", DEFAULT_MQTT_DEVICE_ID);
    }
    if (preferences.getString(KEY_MQTT_DEVICE_NAME, mqttDeviceName, sizeof(mqttDeviceName)) == 0) {
        snprintf(mqttDeviceName, sizeof(mqttDeviceName), "%s", DEFAULT_MQTT_DEVICE_NAME);
    }
    
    // Log NVS status
    size_t freeEntries = preferences.freeEntries();
    logger.infof("NVS status - Free entries: %d", freeEntries);
    logger.info("ConfigManager initialized");
}

AppLogLevel ConfigManager::getLogLevel() { return logLevel; }
void ConfigManager::setLogLevel(AppLogLevel level) {
    logLevel = level;
    preferences.putUChar(KEY_LOG_LEVEL, level);
}

// --- Physical Sensor Getters ---
float ConfigManager::getHighPressureThreshold() { return highPressureThreshold; }
float ConfigManager::getFanOnCurrentThreshold() { return fanOnCurrentThreshold; }
float ConfigManager::getFanOffCurrentThreshold() { return fanOffCurrentThreshold; }
float ConfigManager::getFanHighCurrentThreshold() { return fanHighCurrentThreshold; }
float ConfigManager::getCompressorOnCurrentThreshold() { return compressorOnCurrentThreshold; }
float ConfigManager::getCompressorOffCurrentThreshold() { return compressorOffCurrentThreshold; }
float ConfigManager::getCompressorHighCurrentThreshold() { return compressorHighCurrentThreshold; }
float ConfigManager::getPumpOnCurrentThreshold() { return pumpOnCurrentThreshold; }
float ConfigManager::getPumpOffCurrentThreshold() { return pumpOffCurrentThreshold; }
float ConfigManager::getPumpHighCurrentThreshold() { return pumpHighCurrentThreshold; }

// --- Physical Sensor Setters ---
void ConfigManager::setHighPressureThreshold(float value) {
    if (highPressureThreshold != value) {
        highPressureThreshold = value;
        if (preferences.putFloat(KEY_HIGH_PRESSURE, value) > 0) {
            logger.infof("Saved new high pressure threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_HIGH_PRESSURE);
        }
    }
}
void ConfigManager::setFanOnCurrentThreshold(float value) {
    if (fanOnCurrentThreshold != value) {
        fanOnCurrentThreshold = value;
        if (preferences.putFloat(KEY_FAN_ON, value) > 0) {
            logger.infof("Saved new fan ON threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_FAN_ON);
        }
    }
}
void ConfigManager::setFanOffCurrentThreshold(float value) {
    if (fanOffCurrentThreshold != value) {
        fanOffCurrentThreshold = value;
        if (preferences.putFloat(KEY_FAN_OFF, value) > 0) {
            logger.infof("Saved new fan OFF threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_FAN_OFF);
        }
    }
}
void ConfigManager::setFanHighCurrentThreshold(float value) {
    if (fanHighCurrentThreshold != value) {
        fanHighCurrentThreshold = value;
        if (preferences.putFloat(KEY_FAN_HIGH, value) > 0) {
            logger.infof("Saved new fan HIGH threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_FAN_HIGH);
        }
    }
}
void ConfigManager::setCompressorOnCurrentThreshold(float value) {
    if (compressorOnCurrentThreshold != value) {
        compressorOnCurrentThreshold = value;
        if (preferences.putFloat(KEY_COMPRESSOR_ON, value) > 0) {
            logger.infof("Saved new compressor ON threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_COMPRESSOR_ON);
        }
    }
}
void ConfigManager::setCompressorOffCurrentThreshold(float value) {
    if (compressorOffCurrentThreshold != value) {
        compressorOffCurrentThreshold = value;
        if (preferences.putFloat(KEY_COMPRESSOR_OFF, value) > 0) {
            logger.infof("Saved new compressor OFF threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_COMPRESSOR_OFF);
        }
    }
}
void ConfigManager::setCompressorHighCurrentThreshold(float value) {
    if (compressorHighCurrentThreshold != value) {
        compressorHighCurrentThreshold = value;
        if (preferences.putFloat(KEY_COMPRESSOR_HIGH, value) > 0) {
            logger.infof("Saved new compressor HIGH threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_COMPRESSOR_HIGH);
        }
    }
}
void ConfigManager::setPumpOnCurrentThreshold(float value) {
    if (pumpOnCurrentThreshold != value) {
        pumpOnCurrentThreshold = value;
        if (preferences.putFloat(KEY_PUMP_ON, value) > 0) {
            logger.infof("Saved new pump ON threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PUMP_ON);
        }
    }
}
void ConfigManager::setPumpOffCurrentThreshold(float value) {
    if (pumpOffCurrentThreshold != value) {
        pumpOffCurrentThreshold = value;
        if (preferences.putFloat(KEY_PUMP_OFF, value) > 0) {
            logger.infof("Saved new pump OFF threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PUMP_OFF);
        }
    }
}
void ConfigManager::setPumpHighCurrentThreshold(float value) {
    if (pumpHighCurrentThreshold != value) {
        pumpHighCurrentThreshold = value;
        if (preferences.putFloat(KEY_PUMP_HIGH, value) > 0) {
            logger.infof("Saved new pump HIGH threshold: %.2f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PUMP_HIGH);
        }
    }
}


// --- UI Threshold Getters ---
float ConfigManager::getPressureLowThreshold() { return pressureLowThreshold; }
float ConfigManager::getPressureMidThreshold() { return pressureMidThreshold; }
float ConfigManager::getGeigerAbnormalLowThreshold() { return geigerAbnormalLowThreshold; }
float ConfigManager::getGeigerAbnormalHighThreshold() { return geigerAbnormalHighThreshold; }
float ConfigManager::getGeigerDangerHighThreshold() { return geigerDangerHighThreshold; }
float ConfigManager::getTempComfortableLow() { return tempComfortableLow; }
float ConfigManager::getTempComfortableHigh() { return tempComfortableHigh; }
float ConfigManager::getTempAcceptableLow() { return tempAcceptableLow; }
float ConfigManager::getTempAcceptableHigh() { return tempAcceptableHigh; }
float ConfigManager::getHumiComfortableLow() { return humiComfortableLow; }
float ConfigManager::getHumiComfortableHigh() { return humiComfortableHigh; }
float ConfigManager::getHumiAcceptableLow() { return humiAcceptableLow; }
float ConfigManager::getHumiAcceptableHigh() { return humiAcceptableHigh; }
int ConfigManager::getInactivityTimerDelay() { return inactivityTimerDelay; }
const char* ConfigManager::getMqttHost() { return mqttHost; }
int ConfigManager::getMqttPort() { return mqttPort; }
const char* ConfigManager::getMqttUser() { return mqttUser; }
const char* ConfigManager::getMqttPassword() { return mqttPassword; }
const char* ConfigManager::getMqttDeviceId() { return mqttDeviceId; }
const char* ConfigManager::getMqttDeviceName() { 
    return mqttDeviceName; 
}
int ConfigManager::getCo2WarnThreshold() { return co2WarnThreshold; }
int ConfigManager::getCo2DangerThreshold() { return co2DangerThreshold; }
int ConfigManager::getVocWarnThreshold() { return vocWarnThreshold; }
int ConfigManager::getVocDangerThreshold() { return vocDangerThreshold; }
int ConfigManager::getNo2WarnThreshold() { return no2WarnThreshold; }
int ConfigManager::getNo2DangerThreshold() { return no2DangerThreshold; }
int ConfigManager::getO3WarnThreshold() { return o3WarnThreshold; }
int ConfigManager::getO3DangerThreshold() { return o3DangerThreshold; }
int ConfigManager::getNoxWarnThreshold() { return noxWarnThreshold; }
int ConfigManager::getNoxDangerThreshold() { return noxDangerThreshold; }
int ConfigManager::getCoWarnThreshold() { return coWarnThreshold; }
int ConfigManager::getCoDangerThreshold() { return coDangerThreshold; }
int ConfigManager::getPm1WarnThreshold() { return pm1WarnThreshold; }
int ConfigManager::getPm1DangerThreshold() { return pm1DangerThreshold; }
int ConfigManager::getPm25WarnThreshold() { return pm25WarnThreshold; }
int ConfigManager::getPm25DangerThreshold() { return pm25DangerThreshold; }
int ConfigManager::getPm4WarnThreshold() { return pm4WarnThreshold; }
int ConfigManager::getPm4DangerThreshold() { return pm4DangerThreshold; }
int ConfigManager::getPm10WarnThreshold() { return pm10WarnThreshold; }
int ConfigManager::getPm10DangerThreshold() { return pm10DangerThreshold; }

// --- UI Threshold Setters ---
void ConfigManager::setPressureLowThreshold(float value) {
    if (pressureLowThreshold != value) {
        pressureLowThreshold = value;
        if (preferences.putFloat(KEY_P_LOW, value) > 0) {
            logger.infof("Saved new pressure LOW threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_P_LOW);
        }
    }
}
void ConfigManager::setPressureMidThreshold(float value) {
    if (pressureMidThreshold != value) {
        pressureMidThreshold = value;
        if (preferences.putFloat(KEY_P_MID, value) > 0) {
            logger.infof("Saved new pressure MID threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_P_MID);
        }
    }
}
void ConfigManager::setGeigerAbnormalLowThreshold(float value) {
    if (geigerAbnormalLowThreshold != value) {
        geigerAbnormalLowThreshold = value;
        if (preferences.putFloat(KEY_GEIGER_ABNORMAL_LOW, value) > 0) {
            logger.infof("Saved new Geiger abnormal LOW threshold: %.3f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_GEIGER_ABNORMAL_LOW);
        }
    }
}
void ConfigManager::setGeigerAbnormalHighThreshold(float value) {
    if (geigerAbnormalHighThreshold != value) {
        geigerAbnormalHighThreshold = value;
        if (preferences.putFloat(KEY_GEIGER_ABNORMAL_HIGH, value) > 0) {
            logger.infof("Saved new Geiger abnormal HIGH threshold: %.3f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_GEIGER_ABNORMAL_HIGH);
        }
    }
}
void ConfigManager::setGeigerDangerHighThreshold(float value) {
    if (geigerDangerHighThreshold != value) {
        geigerDangerHighThreshold = value;
        if (preferences.putFloat(KEY_GEIGER_DANGER_HIGH, value) > 0) {
            logger.infof("Saved new Geiger danger HIGH threshold: %.3f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_GEIGER_DANGER_HIGH);
        }
    }
}
void ConfigManager::setTempComfortableLow(float value) {
    if (tempComfortableLow != value) {
        tempComfortableLow = value;
        if (preferences.putFloat(KEY_TEMP_COMFORTABLE_LOW, value) > 0) {
            logger.infof("Saved new temperature comfortable LOW threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_TEMP_COMFORTABLE_LOW);
        }
    }
}
void ConfigManager::setTempComfortableHigh(float value) {
    if (tempComfortableHigh != value) {
        tempComfortableHigh = value;
        if (preferences.putFloat(KEY_TEMP_COMFORTABLE_HIGH, value) > 0) {
            logger.infof("Saved new temperature comfortable HIGH threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_TEMP_COMFORTABLE_HIGH);
        }
    }
}
void ConfigManager::setTempAcceptableLow(float value) {
    if (tempAcceptableLow != value) {
        tempAcceptableLow = value;
        if (preferences.putFloat(KEY_TEMP_ACCEPTABLE_LOW, value) > 0) {
            logger.infof("Saved new temperature acceptable LOW threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_TEMP_ACCEPTABLE_LOW);
        }
    }
}
void ConfigManager::setTempAcceptableHigh(float value) {
    if (tempAcceptableHigh != value) {
        tempAcceptableHigh = value;
        if (preferences.putFloat(KEY_TEMP_ACCEPTABLE_HIGH, value) > 0) {
            logger.infof("Saved new temperature acceptable HIGH threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_TEMP_ACCEPTABLE_HIGH);
        }
    }
}
void ConfigManager::setHumiComfortableLow(float value) {
    if (humiComfortableLow != value) {
        humiComfortableLow = value;
        if (preferences.putFloat(KEY_HUMI_COMFORTABLE_LOW, value) > 0) {
            logger.infof("Saved new humidity comfortable LOW threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_HUMI_COMFORTABLE_LOW);
        }
    }
}
void ConfigManager::setHumiComfortableHigh(float value) {
    if (humiComfortableHigh != value) {
        humiComfortableHigh = value;
        if (preferences.putFloat(KEY_HUMI_COMFORTABLE_HIGH, value) > 0) {
            logger.infof("Saved new humidity comfortable HIGH threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_HUMI_COMFORTABLE_HIGH);
        }
    }
}
void ConfigManager::setHumiAcceptableLow(float value) {
    if (humiAcceptableLow != value) {
        humiAcceptableLow = value;
        if (preferences.putFloat(KEY_HUMI_ACCEPTABLE_LOW, value) > 0) {
            logger.infof("Saved new humidity acceptable LOW threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_HUMI_ACCEPTABLE_LOW);
        }
    }
}
void ConfigManager::setHumiAcceptableHigh(float value) {
    if (humiAcceptableHigh != value) {
        humiAcceptableHigh = value;
        if (preferences.putFloat(KEY_HUMI_ACCEPTABLE_HIGH, value) > 0) {
            logger.infof("Saved new humidity acceptable HIGH threshold: %.1f", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_HUMI_ACCEPTABLE_HIGH);
        }
    }
}
void ConfigManager::setInactivityTimerDelay(int value) {
    if (inactivityTimerDelay != value) {
        inactivityTimerDelay = value;
        if (preferences.putInt(KEY_INACTIVITY_TIMER_DELAY, value) > 0) {
            logger.infof("Saved new inactivity timer delay: %d seconds", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_INACTIVITY_TIMER_DELAY);
        }
    }
}
void ConfigManager::setCo2WarnThreshold(int value) {
    if (co2WarnThreshold != value) {
        co2WarnThreshold = value;
        if (preferences.putInt(KEY_CO2_WARN, value) > 0) {
            logger.infof("Saved new CO2 WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_CO2_WARN);
        }
    }
}
void ConfigManager::setCo2DangerThreshold(int value) {
    if (co2DangerThreshold != value) {
        co2DangerThreshold = value;
        if (preferences.putInt(KEY_CO2_DANGER, value) > 0) {
            logger.infof("Saved new CO2 DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_CO2_DANGER);
        }
    }
}
void ConfigManager::setVocWarnThreshold(int value) {
    if (vocWarnThreshold != value) {
        vocWarnThreshold = value;
        if (preferences.putInt(KEY_VOC_WARN, value) > 0) {
            logger.infof("Saved new VOC WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_VOC_WARN);
        }
    }
}
void ConfigManager::setVocDangerThreshold(int value) {
    if (vocDangerThreshold != value) {
        vocDangerThreshold = value;
        if (preferences.putInt(KEY_VOC_DANGER, value) > 0) {
            logger.infof("Saved new VOC DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_VOC_DANGER);
        }
    }
}
void ConfigManager::setNo2WarnThreshold(int value) {
    if (no2WarnThreshold != value) {
        no2WarnThreshold = value;
        if (preferences.putInt(KEY_NO2_WARN, value) > 0) {
            logger.infof("Saved new NO2 WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_NO2_WARN);
        }
    }
}
void ConfigManager::setNo2DangerThreshold(int value) {
    if (no2DangerThreshold != value) {
        no2DangerThreshold = value;
        if (preferences.putInt(KEY_NO2_DANGER, value) > 0) {
            logger.infof("Saved new NO2 DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_NO2_DANGER);
        }
    }
}
void ConfigManager::setO3WarnThreshold(int value) {
    if (o3WarnThreshold != value) {
        o3WarnThreshold = value;
        if (preferences.putInt(KEY_O3_WARN, value) > 0) {
            logger.infof("Saved new O3 WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_O3_WARN);
        }
    }
}
void ConfigManager::setO3DangerThreshold(int value) {
    if (o3DangerThreshold != value) {
        o3DangerThreshold = value;
        if (preferences.putInt(KEY_O3_DANGER, value) > 0) {
            logger.infof("Saved new O3 DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_O3_DANGER);
        }
    }
}
void ConfigManager::setNoxWarnThreshold(int value) {
    if (noxWarnThreshold != value) {
        noxWarnThreshold = value;
        if (preferences.putInt(KEY_NOX_WARN, value) > 0) {
            logger.infof("Saved new NOx WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_NOX_WARN);
        }
    }
}
void ConfigManager::setNoxDangerThreshold(int value) {
    if (noxDangerThreshold != value) {
        noxDangerThreshold = value;
        if (preferences.putInt(KEY_NOX_DANGER, value) > 0) {
            logger.infof("Saved new NOx DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_NOX_DANGER);
        }
    }
}
void ConfigManager::setCoWarnThreshold(int value) {
    if (coWarnThreshold != value) {
        coWarnThreshold = value;
        if (preferences.putInt(KEY_CO_WARN, value) > 0) {
            logger.infof("Saved new CO WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_CO_WARN);
        }
    }
}
void ConfigManager::setCoDangerThreshold(int value) {
    if (coDangerThreshold != value) {
        coDangerThreshold = value;
        if (preferences.putInt(KEY_CO_DANGER, value) > 0) {
            logger.infof("Saved new CO DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_CO_DANGER);
        }
    }
}
void ConfigManager::setPm1WarnThreshold(int value) {
    if (pm1WarnThreshold != value) {
        pm1WarnThreshold = value;
        if (preferences.putInt(KEY_PM1_WARN, value) > 0) {
            logger.infof("Saved new PM1 WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PM1_WARN);
        }
    }
}
void ConfigManager::setPm1DangerThreshold(int value) {
    if (pm1DangerThreshold != value) {
        pm1DangerThreshold = value;
        if (preferences.putInt(KEY_PM1_DANGER, value) > 0) {
            logger.infof("Saved new PM1 DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PM1_DANGER);
        }
    }
}
void ConfigManager::setPm25WarnThreshold(int value) {
    if (pm25WarnThreshold != value) {
        pm25WarnThreshold = value;
        if (preferences.putInt(KEY_PM25_WARN, value) > 0) {
            logger.infof("Saved new PM2.5 WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PM25_WARN);
        }
    }
}
void ConfigManager::setPm25DangerThreshold(int value) {
    if (pm25DangerThreshold != value) {
        pm25DangerThreshold = value;
        if (preferences.putInt(KEY_PM25_DANGER, value) > 0) {
            logger.infof("Saved new PM2.5 DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PM25_DANGER);
        }
    }
}
void ConfigManager::setPm4WarnThreshold(int value) {
    if (pm4WarnThreshold != value) {
        pm4WarnThreshold = value;
        if (preferences.putInt(KEY_PM4_WARN, value) > 0) {
            logger.infof("Saved new PM4 WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PM4_WARN);
        }
    }
}
void ConfigManager::setPm4DangerThreshold(int value) {
    if (pm4DangerThreshold != value) {
        pm4DangerThreshold = value;
        if (preferences.putInt(KEY_PM4_DANGER, value) > 0) {
            logger.infof("Saved new PM4 DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PM4_DANGER);
        }
    }
}
void ConfigManager::setPm10WarnThreshold(int value) {
    if (pm10WarnThreshold != value) {
        pm10WarnThreshold = value;
        if (preferences.putInt(KEY_PM10_WARN, value) > 0) {
            logger.infof("Saved new PM10 WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PM10_WARN);
        }
    }
}
void ConfigManager::setPm10DangerThreshold(int value) {
    if (pm10DangerThreshold != value) {
        pm10DangerThreshold = value;
        if (preferences.putInt(KEY_PM10_DANGER, value) > 0) {
            logger.infof("Saved new PM10 DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_PM10_DANGER);
        }
    }
}

void ConfigManager::setMqttHost(const char* value) {
    if (strcmp(mqttHost, value) != 0) {
        snprintf(mqttHost, sizeof(mqttHost), "%s", value);
        if (preferences.putString(KEY_MQTT_HOST, mqttHost) > 0) {
            logger.infof("Saved new MQTT host: %s", mqttHost);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_HOST);
        }
    }
}

void ConfigManager::setMqttPort(int value) {
    if (mqttPort != value) {
        mqttPort = value;
        if (preferences.putInt(KEY_MQTT_PORT, value) > 0) {
            logger.infof("Saved new MQTT port: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_PORT);
        }
    }
}

void ConfigManager::setMqttUser(const char* value) {
    if (strcmp(mqttUser, value) != 0) {
        snprintf(mqttUser, sizeof(mqttUser), "%s", value);
        if (preferences.putString(KEY_MQTT_USER, mqttUser) > 0) {
            logger.infof("Saved new MQTT user: %s", mqttUser);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_USER);
        }
    }
}

void ConfigManager::setMqttPassword(const char* value) {
    if (strcmp(mqttPassword, value) != 0) {
        snprintf(mqttPassword, sizeof(mqttPassword), "%s", value);
        if (preferences.putString(KEY_MQTT_PASSWORD, mqttPassword) > 0) {
            logger.infof("Saved new MQTT password: %s", mqttPassword);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_PASSWORD);
        }
    }
}

void ConfigManager::setMqttDeviceId(const char* value) {
    if (strcmp(mqttDeviceId, value) != 0) {
        snprintf(mqttDeviceId, sizeof(mqttDeviceId), "%s", value);
        if (preferences.putString(KEY_MQTT_DEVICE_ID, mqttDeviceId) > 0) {
            logger.infof("Saved new MQTT device ID: %s", mqttDeviceId);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_DEVICE_ID);
        }
    }
}

void ConfigManager::setMqttDeviceName(const char* value) {
    if (strcmp(mqttDeviceName, value) != 0) {
        snprintf(mqttDeviceName, sizeof(mqttDeviceName), "%s", value);
        if (preferences.putString(KEY_MQTT_DEVICE_NAME, mqttDeviceName) > 0) {
            logger.infof("Saved new MQTT device name: %s", mqttDeviceName);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_DEVICE_NAME);
        }
    }
}



// ConfigManagerAccessor implementation
void ConfigManagerAccessor::lockMutex() {
    xSemaphoreTakeRecursive(_instance._mutex, portMAX_DELAY);
}

void ConfigManagerAccessor::unlockMutex() {
    xSemaphoreGiveRecursive(_instance._mutex);
}

ConfigManagerAccessor::ConfigManagerAccessor() 
    : _instance(*ConfigManager::_instance) {
    lockMutex();
}

ConfigManagerAccessor::~ConfigManagerAccessor() {
    unlockMutex();
}

#include "ConfigManager.h"
#include "Logger.h"
#include "secrets.h"

// Static instance pointer
ConfigManager *ConfigManager::_instance = nullptr;

// Constructor - sets the static instance pointer and initializes mutex
ConfigManager::ConfigManager() {
  _instance = this;
  _mutex = xSemaphoreCreateRecursiveMutex();
}

// --- Define Keys ---
const char *ConfigManager::KEY_LOG_LEVEL = "log_level";
const char *ConfigManager::KEY_HIGH_PRESSURE = "p_high_thresh";
const char *ConfigManager::KEY_FAN_ON = "fan_on_thresh";
const char *ConfigManager::KEY_FAN_OFF = "fan_off_thresh";
const char *ConfigManager::KEY_FAN_HIGH = "fan_high_thresh";
const char *ConfigManager::KEY_COMPRESSOR_ON = "comp_on_thresh";
const char *ConfigManager::KEY_COMPRESSOR_OFF = "comp_off_thresh";
const char *ConfigManager::KEY_COMPRESSOR_HIGH = "comp_high_thresh";
const char *ConfigManager::KEY_PUMP_ON = "pump_on_thresh";
const char *ConfigManager::KEY_PUMP_OFF = "pump_off_thresh";
const char *ConfigManager::KEY_PUMP_HIGH = "pump_high_thresh";
const char *ConfigManager::KEY_P_LOW = "p_low_thresh";
const char *ConfigManager::KEY_P_MID = "p_mid_thresh";
const char *ConfigManager::KEY_GEIGER_ABNORMAL_LOW = "geiger_abnormal_low";
const char *ConfigManager::KEY_GEIGER_ABNORMAL_HIGH = "geiger_abnormal_high";
const char *ConfigManager::KEY_GEIGER_DANGER_HIGH = "geiger_danger_high";
const char *ConfigManager::KEY_TEMP_COMFORTABLE_LOW = "temp_comfortable_low";
const char *ConfigManager::KEY_TEMP_COMFORTABLE_HIGH = "temp_comfortable_high";
const char *ConfigManager::KEY_TEMP_ACCEPTABLE_LOW = "temp_acceptable_low";
const char *ConfigManager::KEY_TEMP_ACCEPTABLE_HIGH = "temp_acceptable_high";
const char *ConfigManager::KEY_HUMI_COMFORTABLE_LOW = "humi_comfortable_low";
const char *ConfigManager::KEY_HUMI_COMFORTABLE_HIGH = "humi_comfortable_high";
const char *ConfigManager::KEY_HUMI_ACCEPTABLE_LOW = "humi_acceptable_low";
const char *ConfigManager::KEY_HUMI_ACCEPTABLE_HIGH = "humi_acceptable_high";
const char *ConfigManager::KEY_INACTIVITY_TIMER_DELAY =
    "inactivity_timer_delay";
const char *ConfigManager::KEY_WIFI_SSID = "wifi_ssid";
const char *ConfigManager::KEY_WIFI_PASSWORD = "wifi_pass";
const char *ConfigManager::KEY_MQTT_HOST = "mqtt_host";
const char *ConfigManager::KEY_MQTT_PORT = "mqtt_port";
const char *ConfigManager::KEY_MQTT_USER = "mqtt_user";
const char *ConfigManager::KEY_MQTT_PASSWORD = "mqtt_password";
const char *ConfigManager::KEY_MQTT_DEVICE_ID = "mqtt_device_id";
const char *ConfigManager::KEY_MQTT_DEVICE_NAME = "mqtt_name";
const char *ConfigManager::KEY_CO2_WARN = "co2_warn_ui";
const char *ConfigManager::KEY_CO2_DANGER = "co2_danger_ui";
const char *ConfigManager::KEY_VOC_WARN = "voc_warn_ui";
const char *ConfigManager::KEY_VOC_DANGER = "voc_danger_ui";
const char *ConfigManager::KEY_NO2_WARN = "no2_warn_ui";
const char *ConfigManager::KEY_NO2_DANGER = "no2_danger_ui";
const char *ConfigManager::KEY_O3_WARN = "o3_warn_ui";
const char *ConfigManager::KEY_O3_DANGER = "o3_danger_ui";
const char *ConfigManager::KEY_NOX_WARN = "nox_warn_ui";
const char *ConfigManager::KEY_NOX_DANGER = "nox_danger_ui";
const char *ConfigManager::KEY_CO_WARN = "co_warn_ui";
const char *ConfigManager::KEY_CO_DANGER = "co_danger_ui";
const char *ConfigManager::KEY_PM1_WARN = "pm1_warn_ui";
const char *ConfigManager::KEY_PM1_DANGER = "pm1_danger_ui";
const char *ConfigManager::KEY_PM25_WARN = "pm25_warn_ui";
const char *ConfigManager::KEY_PM25_DANGER = "pm25_danger_ui";
const char *ConfigManager::KEY_PM4_WARN = "pm4_warn_ui";
const char *ConfigManager::KEY_PM4_DANGER = "pm4_danger_ui";
const char *ConfigManager::KEY_PM10_WARN = "pm10_warn_ui";
const char *ConfigManager::KEY_PM10_DANGER = "pm10_danger_ui";


void ConfigManager::init() {
  if (!preferences.begin("hvac-config", false)) {
    logger.error(
        "Failed to initialize NVS for ConfigManager. Using default values.");
    // Manually set defaults if NVS fails to open
    logLevel = ConfigDefaults::LOG_LEVEL;
    highPressureThreshold = ConfigDefaults::HIGH_PRESSURE;
    fanOnCurrentThreshold = ConfigDefaults::FAN_ON_CURRENT;
    fanOffCurrentThreshold = ConfigDefaults::FAN_OFF_CURRENT;
    fanHighCurrentThreshold = ConfigDefaults::FAN_HIGH_CURRENT;
    compressorOnCurrentThreshold = ConfigDefaults::COMPRESSOR_ON_CURRENT;
    compressorOffCurrentThreshold = ConfigDefaults::COMPRESSOR_OFF_CURRENT;
    compressorHighCurrentThreshold = ConfigDefaults::COMPRESSOR_HIGH_CURRENT;
    pumpOnCurrentThreshold = ConfigDefaults::PUMP_ON_CURRENT;
    pumpOffCurrentThreshold = ConfigDefaults::PUMP_OFF_CURRENT;
    pumpHighCurrentThreshold = ConfigDefaults::PUMP_HIGH_CURRENT;
    pressureLowThreshold = ConfigDefaults::PRESSURE_LOW;
    pressureMidThreshold = ConfigDefaults::PRESSURE_MID;
    geigerAbnormalLowThreshold = ConfigDefaults::GEIGER_ABNORMAL_LOW;
    geigerAbnormalHighThreshold = ConfigDefaults::GEIGER_ABNORMAL_HIGH;
    geigerDangerHighThreshold = ConfigDefaults::GEIGER_DANGER_HIGH;
    tempComfortableLow = ConfigDefaults::TEMP_COMFORTABLE_LOW;
    tempComfortableHigh = ConfigDefaults::TEMP_COMFORTABLE_HIGH;
    tempAcceptableLow = ConfigDefaults::TEMP_ACCEPTABLE_LOW;
    tempAcceptableHigh = ConfigDefaults::TEMP_ACCEPTABLE_HIGH;
    humiComfortableLow = ConfigDefaults::HUMI_COMFORTABLE_LOW;
    humiComfortableHigh = ConfigDefaults::HUMI_COMFORTABLE_HIGH;
    humiAcceptableLow = ConfigDefaults::HUMI_ACCEPTABLE_LOW;
    humiAcceptableHigh = ConfigDefaults::HUMI_ACCEPTABLE_HIGH;
    inactivityTimerDelay = ConfigDefaults::INACTIVITY_DELAY;
    co2WarnThreshold = ConfigDefaults::CO2_WARN;
    co2DangerThreshold = ConfigDefaults::CO2_DANGER;
    vocWarnThreshold = ConfigDefaults::VOC_WARN;
    vocDangerThreshold = ConfigDefaults::VOC_DANGER;
    no2WarnThreshold = ConfigDefaults::NO2_WARN;
    no2DangerThreshold = ConfigDefaults::NO2_DANGER;
    o3WarnThreshold = ConfigDefaults::O3_WARN;
    o3DangerThreshold = ConfigDefaults::O3_DANGER;
    noxWarnThreshold = ConfigDefaults::NOX_WARN;
    noxDangerThreshold = ConfigDefaults::NOX_DANGER;
    coWarnThreshold = ConfigDefaults::CO_WARN;
    coDangerThreshold = ConfigDefaults::CO_DANGER;
    pm1WarnThreshold = ConfigDefaults::PM1_WARN;
    pm1DangerThreshold = ConfigDefaults::PM1_DANGER;
    pm25WarnThreshold = ConfigDefaults::PM25_WARN;
    pm25DangerThreshold = ConfigDefaults::PM25_DANGER;
    pm4WarnThreshold = ConfigDefaults::PM4_WARN;
    pm4DangerThreshold = ConfigDefaults::PM4_DANGER;
    pm10WarnThreshold = ConfigDefaults::PM10_WARN;
    pm10DangerThreshold = ConfigDefaults::PM10_DANGER;
    strcpy(wifiSsid, WIFI_SSID);
    strcpy(wifiPassword, WIFI_PASSWORD);
    strcpy(mqttHost, MQTT_HOST);
    mqttPort = MQTT_PORT;
    strcpy(mqttUser, MQTT_USER);
    strcpy(mqttPassword, MQTT_PASSWORD);
    strcpy(mqttDeviceId, MQTT_DEVICE_ID);
    strcpy(mqttDeviceName, MQTT_DEVICE_NAME);
    return;
  }

  logLevel = static_cast<AppLogLevel>(
      preferences.getUChar(KEY_LOG_LEVEL, ConfigDefaults::LOG_LEVEL));

  logger.info("ConfigManager initializing and loading values from NVS...");

  // Load physical thresholds
  highPressureThreshold =
      preferences.getFloat(KEY_HIGH_PRESSURE, ConfigDefaults::HIGH_PRESSURE);
  fanOnCurrentThreshold =
      preferences.getFloat(KEY_FAN_ON, ConfigDefaults::FAN_ON_CURRENT);
  fanOffCurrentThreshold =
      preferences.getFloat(KEY_FAN_OFF, ConfigDefaults::FAN_OFF_CURRENT);
  fanHighCurrentThreshold =
      preferences.getFloat(KEY_FAN_HIGH, ConfigDefaults::FAN_HIGH_CURRENT);
  compressorOnCurrentThreshold = preferences.getFloat(
      KEY_COMPRESSOR_ON, ConfigDefaults::COMPRESSOR_ON_CURRENT);
  compressorOffCurrentThreshold = preferences.getFloat(
      KEY_COMPRESSOR_OFF, ConfigDefaults::COMPRESSOR_OFF_CURRENT);
  compressorHighCurrentThreshold = preferences.getFloat(
      KEY_COMPRESSOR_HIGH, ConfigDefaults::COMPRESSOR_HIGH_CURRENT);
  pumpOnCurrentThreshold =
      preferences.getFloat(KEY_PUMP_ON, ConfigDefaults::PUMP_ON_CURRENT);
  pumpOffCurrentThreshold =
      preferences.getFloat(KEY_PUMP_OFF, ConfigDefaults::PUMP_OFF_CURRENT);
  pumpHighCurrentThreshold =
      preferences.getFloat(KEY_PUMP_HIGH, ConfigDefaults::PUMP_HIGH_CURRENT);

  // Load UI thresholds
  pressureLowThreshold =
      preferences.getFloat(KEY_P_LOW, ConfigDefaults::PRESSURE_LOW);
  pressureMidThreshold =
      preferences.getFloat(KEY_P_MID, ConfigDefaults::PRESSURE_MID);
  geigerAbnormalLowThreshold = preferences.getFloat(
      KEY_GEIGER_ABNORMAL_LOW, ConfigDefaults::GEIGER_ABNORMAL_LOW);
  geigerAbnormalHighThreshold = preferences.getFloat(
      KEY_GEIGER_ABNORMAL_HIGH, ConfigDefaults::GEIGER_ABNORMAL_HIGH);
  geigerDangerHighThreshold = preferences.getFloat(
      KEY_GEIGER_DANGER_HIGH, ConfigDefaults::GEIGER_DANGER_HIGH);
  tempComfortableLow = preferences.getFloat(KEY_TEMP_COMFORTABLE_LOW,
                                            ConfigDefaults::TEMP_COMFORTABLE_LOW);
  tempComfortableHigh = preferences.getFloat(KEY_TEMP_COMFORTABLE_HIGH,
                                             ConfigDefaults::TEMP_COMFORTABLE_HIGH);
  tempAcceptableLow = preferences.getFloat(KEY_TEMP_ACCEPTABLE_LOW,
                                           ConfigDefaults::TEMP_ACCEPTABLE_LOW);
  tempAcceptableHigh = preferences.getFloat(KEY_TEMP_ACCEPTABLE_HIGH,
                                            ConfigDefaults::TEMP_ACCEPTABLE_HIGH);
  humiComfortableLow = preferences.getFloat(KEY_HUMI_COMFORTABLE_LOW,
                                            ConfigDefaults::HUMI_COMFORTABLE_LOW);
  humiComfortableHigh = preferences.getFloat(KEY_HUMI_COMFORTABLE_HIGH,
                                             ConfigDefaults::HUMI_COMFORTABLE_HIGH);
  humiAcceptableLow = preferences.getFloat(KEY_HUMI_ACCEPTABLE_LOW,
                                           ConfigDefaults::HUMI_ACCEPTABLE_LOW);
  humiAcceptableHigh = preferences.getFloat(KEY_HUMI_ACCEPTABLE_HIGH,
                                            ConfigDefaults::HUMI_ACCEPTABLE_HIGH);

  inactivityTimerDelay = preferences.getInt(KEY_INACTIVITY_TIMER_DELAY,
                                            ConfigDefaults::INACTIVITY_DELAY);
  co2WarnThreshold =
      preferences.getInt(KEY_CO2_WARN, ConfigDefaults::CO2_WARN);
  co2DangerThreshold =
      preferences.getInt(KEY_CO2_DANGER, ConfigDefaults::CO2_DANGER);
  vocWarnThreshold =
      preferences.getInt(KEY_VOC_WARN, ConfigDefaults::VOC_WARN);
  vocDangerThreshold =
      preferences.getInt(KEY_VOC_DANGER, ConfigDefaults::VOC_DANGER);
  no2WarnThreshold =
      preferences.getInt(KEY_NO2_WARN, ConfigDefaults::NO2_WARN);
  no2DangerThreshold =
      preferences.getInt(KEY_NO2_DANGER, ConfigDefaults::NO2_DANGER);
  o3WarnThreshold = preferences.getInt(KEY_O3_WARN, ConfigDefaults::O3_WARN);
  o3DangerThreshold =
      preferences.getInt(KEY_O3_DANGER, ConfigDefaults::O3_DANGER);
  noxWarnThreshold =
      preferences.getInt(KEY_NOX_WARN, ConfigDefaults::NOX_WARN);
  noxDangerThreshold =
      preferences.getInt(KEY_NOX_DANGER, ConfigDefaults::NOX_DANGER);
  coWarnThreshold = preferences.getInt(KEY_CO_WARN, ConfigDefaults::CO_WARN);
  coDangerThreshold =
      preferences.getInt(KEY_CO_DANGER, ConfigDefaults::CO_DANGER);
  pm1WarnThreshold =
      preferences.getInt(KEY_PM1_WARN, ConfigDefaults::PM1_WARN);
  pm1DangerThreshold =
      preferences.getInt(KEY_PM1_DANGER, ConfigDefaults::PM1_DANGER);
  pm25WarnThreshold =
      preferences.getInt(KEY_PM25_WARN, ConfigDefaults::PM25_WARN);
  pm25DangerThreshold =
      preferences.getInt(KEY_PM25_DANGER, ConfigDefaults::PM25_DANGER);
  pm4WarnThreshold =
      preferences.getInt(KEY_PM4_WARN, ConfigDefaults::PM4_WARN);
  pm4DangerThreshold =
      preferences.getInt(KEY_PM4_DANGER, ConfigDefaults::PM4_DANGER);
  pm10WarnThreshold =
      preferences.getInt(KEY_PM10_WARN, ConfigDefaults::PM10_WARN);
  pm10DangerThreshold =
      preferences.getInt(KEY_PM10_DANGER, ConfigDefaults::PM10_DANGER);

  // Load WiFi configuration with validation
  size_t rawSsidLen =
      preferences.getString(KEY_WIFI_SSID, wifiSsid, sizeof(wifiSsid));
  size_t ssidLen = (rawSsidLen > 0) ? strlen(wifiSsid) : 0;
  bool validSsid = (ssidLen > 0 && ssidLen <= ConfigLimits::MAX_WIFI_SSID_LEN);
  if (validSsid) {
    bool allSpaces = true;
    for (size_t i = 0; i < ssidLen; i++) {
      if (wifiSsid[i] != ' ' && wifiSsid[i] != '\t') {
        allSpaces = false;
        break;
      }
    }
    if (allSpaces)
      validSsid = false;
  }
  if (!validSsid) {
    if (rawSsidLen > 0) {
      logger.warningf(
          "Invalid WiFi SSID in NVS (length %u); falling back to default.",
          ssidLen);
    }
    snprintf(wifiSsid, sizeof(wifiSsid), "%s", WIFI_SSID);
  }

  size_t rawPassLen = preferences.getString(KEY_WIFI_PASSWORD, wifiPassword,
                                            sizeof(wifiPassword));
  // If password is not set in NVS (rawPassLen == 0), fall back to default
  if (rawPassLen == 0) {
    snprintf(wifiPassword, sizeof(wifiPassword), "%s", WIFI_PASSWORD);
  } else {
    // Password exists in NVS: check actual string length (excluding null
    // terminator)
    size_t passLen = strlen(wifiPassword);
    // Valid password is 0 (open network) or 8-64 characters
    if (passLen > 0 && (passLen < ConfigLimits::MIN_WIFI_PASSWORD_LEN || passLen > ConfigLimits::MAX_WIFI_PASSWORD_LEN)) {
      logger.warningf(
          "WiFi password invalid in NVS (length %u); falling back to default.",
          passLen);
      snprintf(wifiPassword, sizeof(wifiPassword), "%s", WIFI_PASSWORD);
    }
  }

  // Load MQTT configuration
  if (preferences.getString(KEY_MQTT_HOST, mqttHost, sizeof(mqttHost)) == 0) {
    snprintf(mqttHost, sizeof(mqttHost), "%s", MQTT_HOST);
  }
  mqttPort = preferences.getInt(KEY_MQTT_PORT, MQTT_PORT);
  if (preferences.getString(KEY_MQTT_USER, mqttUser, sizeof(mqttUser)) == 0) {
    snprintf(mqttUser, sizeof(mqttUser), "%s", MQTT_USER);
  }
  if (preferences.getString(KEY_MQTT_PASSWORD, mqttPassword,
                            sizeof(mqttPassword)) == 0) {
    snprintf(mqttPassword, sizeof(mqttPassword), "%s", MQTT_PASSWORD);
  }
  if (preferences.getString(KEY_MQTT_DEVICE_ID, mqttDeviceId,
                            sizeof(mqttDeviceId)) == 0) {
    snprintf(mqttDeviceId, sizeof(mqttDeviceId), "%s", MQTT_DEVICE_ID);
  }
  if (preferences.getString(KEY_MQTT_DEVICE_NAME, mqttDeviceName,
                            sizeof(mqttDeviceName)) == 0) {
    snprintf(mqttDeviceName, sizeof(mqttDeviceName), "%s",
             MQTT_DEVICE_NAME);
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
float ConfigManager::getHighPressureThreshold() {
  return highPressureThreshold;
}
float ConfigManager::getFanOnCurrentThreshold() {
  return fanOnCurrentThreshold;
}
float ConfigManager::getFanOffCurrentThreshold() {
  return fanOffCurrentThreshold;
}
float ConfigManager::getFanHighCurrentThreshold() {
  return fanHighCurrentThreshold;
}
float ConfigManager::getCompressorOnCurrentThreshold() {
  return compressorOnCurrentThreshold;
}
float ConfigManager::getCompressorOffCurrentThreshold() {
  return compressorOffCurrentThreshold;
}
float ConfigManager::getCompressorHighCurrentThreshold() {
  return compressorHighCurrentThreshold;
}
float ConfigManager::getPumpOnCurrentThreshold() {
  return pumpOnCurrentThreshold;
}
float ConfigManager::getPumpOffCurrentThreshold() {
  return pumpOffCurrentThreshold;
}
float ConfigManager::getPumpHighCurrentThreshold() {
  return pumpHighCurrentThreshold;
}

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
float ConfigManager::getGeigerAbnormalLowThreshold() {
  return geigerAbnormalLowThreshold;
}
float ConfigManager::getGeigerAbnormalHighThreshold() {
  return geigerAbnormalHighThreshold;
}
float ConfigManager::getGeigerDangerHighThreshold() {
  return geigerDangerHighThreshold;
}
float ConfigManager::getTempComfortableLow() { return tempComfortableLow; }
float ConfigManager::getTempComfortableHigh() { return tempComfortableHigh; }
float ConfigManager::getTempAcceptableLow() { return tempAcceptableLow; }
float ConfigManager::getTempAcceptableHigh() { return tempAcceptableHigh; }
float ConfigManager::getHumiComfortableLow() { return humiComfortableLow; }
float ConfigManager::getHumiComfortableHigh() { return humiComfortableHigh; }
float ConfigManager::getHumiAcceptableLow() { return humiAcceptableLow; }
float ConfigManager::getHumiAcceptableHigh() { return humiAcceptableHigh; }
uint32_t ConfigManager::getInactivityTimerDelay() {
  return inactivityTimerDelay;
}
const char *ConfigManager::getWifiSsid() { return wifiSsid; }
const char *ConfigManager::getWifiPassword() { return wifiPassword; }
const char *ConfigManager::getMqttHost() { return mqttHost; }
int ConfigManager::getMqttPort() { return mqttPort; }
const char *ConfigManager::getMqttUser() { return mqttUser; }
const char *ConfigManager::getMqttPassword() { return mqttPassword; }
const char *ConfigManager::getMqttDeviceId() { return mqttDeviceId; }
const char *ConfigManager::getMqttDeviceName() { return mqttDeviceName; }
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
      logger.errorf("Failed to save key '%s' to NVS.",
                    KEY_GEIGER_ABNORMAL_HIGH);
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
      logger.infof("Saved new temperature comfortable LOW threshold: %.1f",
                   value);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.",
                    KEY_TEMP_COMFORTABLE_LOW);
    }
  }
}
void ConfigManager::setTempComfortableHigh(float value) {
  if (tempComfortableHigh != value) {
    tempComfortableHigh = value;
    if (preferences.putFloat(KEY_TEMP_COMFORTABLE_HIGH, value) > 0) {
      logger.infof("Saved new temperature comfortable HIGH threshold: %.1f",
                   value);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.",
                    KEY_TEMP_COMFORTABLE_HIGH);
    }
  }
}
void ConfigManager::setTempAcceptableLow(float value) {
  if (tempAcceptableLow != value) {
    tempAcceptableLow = value;
    if (preferences.putFloat(KEY_TEMP_ACCEPTABLE_LOW, value) > 0) {
      logger.infof("Saved new temperature acceptable LOW threshold: %.1f",
                   value);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.", KEY_TEMP_ACCEPTABLE_LOW);
    }
  }
}
void ConfigManager::setTempAcceptableHigh(float value) {
  if (tempAcceptableHigh != value) {
    tempAcceptableHigh = value;
    if (preferences.putFloat(KEY_TEMP_ACCEPTABLE_HIGH, value) > 0) {
      logger.infof("Saved new temperature acceptable HIGH threshold: %.1f",
                   value);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.",
                    KEY_TEMP_ACCEPTABLE_HIGH);
    }
  }
}
void ConfigManager::setHumiComfortableLow(float value) {
  if (humiComfortableLow != value) {
    humiComfortableLow = value;
    if (preferences.putFloat(KEY_HUMI_COMFORTABLE_LOW, value) > 0) {
      logger.infof("Saved new humidity comfortable LOW threshold: %.1f", value);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.",
                    KEY_HUMI_COMFORTABLE_LOW);
    }
  }
}
void ConfigManager::setHumiComfortableHigh(float value) {
  if (humiComfortableHigh != value) {
    humiComfortableHigh = value;
    if (preferences.putFloat(KEY_HUMI_COMFORTABLE_HIGH, value) > 0) {
      logger.infof("Saved new humidity comfortable HIGH threshold: %.1f",
                   value);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.",
                    KEY_HUMI_COMFORTABLE_HIGH);
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
      logger.errorf("Failed to save key '%s' to NVS.",
                    KEY_HUMI_ACCEPTABLE_HIGH);
    }
  }
}
void ConfigManager::setInactivityTimerDelay(int value) {
  if (inactivityTimerDelay != value) {
    inactivityTimerDelay = value;
    if (preferences.putInt(KEY_INACTIVITY_TIMER_DELAY, value) > 0) {
      logger.infof("Saved new inactivity timer delay: %d seconds", value);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.",
                    KEY_INACTIVITY_TIMER_DELAY);
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

bool ConfigManager::setWifiSsid(const char *value) {
  if (!value)
    return false;
  size_t len = strlen(value);
  if (len == 0 || len > ConfigLimits::MAX_WIFI_SSID_LEN) {
    logger.errorf("Rejected invalid WiFi SSID (length %u, max %u)", len,
                  (unsigned)ConfigLimits::MAX_WIFI_SSID_LEN);
    return false;
  }
  if (strcmp(wifiSsid, value) != 0) {
    snprintf(wifiSsid, sizeof(wifiSsid), "%s", value);
    if (preferences.putString(KEY_WIFI_SSID, wifiSsid) > 0) {
      logger.infof("Saved new WiFi SSID: %s", wifiSsid);
      return true;
    } else {
      logger.errorf("Failed to save key '%s' to NVS.", KEY_WIFI_SSID);
      return false;
    }
  }
  return true;
}

bool ConfigManager::setWifiPassword(const char *value) {
  if (!value)
    return false;
  size_t len = strlen(value);
  if (len > 0 && (len < ConfigLimits::MIN_WIFI_PASSWORD_LEN || len > ConfigLimits::MAX_WIFI_PASSWORD_LEN)) {
    logger.errorf(
        "Rejected invalid WiFi password (length %u, must be 0 or %u-%u)", len,
        (unsigned)ConfigLimits::MIN_WIFI_PASSWORD_LEN, (unsigned)ConfigLimits::MAX_WIFI_PASSWORD_LEN);
    return false;
  }
  if (strcmp(wifiPassword, value) != 0) {
    snprintf(wifiPassword, sizeof(wifiPassword), "%s", value);
    if (preferences.putString(KEY_WIFI_PASSWORD, wifiPassword) > 0) {
      logger.info("Saved new WiFi password to NVS.");
      return true;
    } else {
      logger.errorf("Failed to save key '%s' to NVS.", KEY_WIFI_PASSWORD);
      return false;
    }
  }
  return true;
}

bool ConfigManager::setWifiCredentials(const char *ssid, const char *password,
                                       bool updatePassword) {
  if (!ssid)
    return false;
  size_t ssidLen = strlen(ssid);
  if (ssidLen == 0 || ssidLen > ConfigLimits::MAX_WIFI_SSID_LEN) {
    logger.errorf("Rejected invalid WiFi SSID (length %u, max %u)", ssidLen,
                  (unsigned)ConfigLimits::MAX_WIFI_SSID_LEN);
    return false;
  }
  if (updatePassword) {
    if (!password)
      return false;
    size_t passLen = strlen(password);
    if (passLen > 0 && (passLen < ConfigLimits::MIN_WIFI_PASSWORD_LEN || passLen > ConfigLimits::MAX_WIFI_PASSWORD_LEN)) {
      logger.errorf(
          "Rejected invalid WiFi password (length %u, must be 0 or %u-%u)",
          passLen, (unsigned)ConfigLimits::MIN_WIFI_PASSWORD_LEN,
          (unsigned)ConfigLimits::MAX_WIFI_PASSWORD_LEN);
      return false;
    }
  }

  if (updatePassword) {
    if (!setWifiPassword(password)) {
      logger.error("Failed to save WiFi password; aborting SSID update.");
      return false;
    }
  }
  if (!setWifiSsid(ssid)) {
    logger.error("Failed to save WiFi SSID.");
    return false;
  }
  return true;
}

void ConfigManager::setMqttHost(const char *value) {
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

void ConfigManager::setMqttUser(const char *value) {
  if (strcmp(mqttUser, value) != 0) {
    snprintf(mqttUser, sizeof(mqttUser), "%s", value);
    if (preferences.putString(KEY_MQTT_USER, mqttUser) > 0) {
      logger.infof("Saved new MQTT user: %s", mqttUser);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_USER);
    }
  }
}

void ConfigManager::setMqttPassword(const char *value) {
  if (strcmp(mqttPassword, value) != 0) {
    snprintf(mqttPassword, sizeof(mqttPassword), "%s", value);
    if (preferences.putString(KEY_MQTT_PASSWORD, mqttPassword) > 0) {
      logger.info("Saved new MQTT password to NVS.");
    } else {
      logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_PASSWORD);
    }
  }
}

void ConfigManager::setMqttDeviceId(const char *value) {
  if (strcmp(mqttDeviceId, value) != 0) {
    snprintf(mqttDeviceId, sizeof(mqttDeviceId), "%s", value);
    if (preferences.putString(KEY_MQTT_DEVICE_ID, mqttDeviceId) > 0) {
      logger.infof("Saved new MQTT device ID: %s", mqttDeviceId);
    } else {
      logger.errorf("Failed to save key '%s' to NVS.", KEY_MQTT_DEVICE_ID);
    }
  }
}

void ConfigManager::setMqttDeviceName(const char *value) {
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

ConfigManagerAccessor::~ConfigManagerAccessor() { unlockMutex(); }

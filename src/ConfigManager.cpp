#include "ConfigManager.h"
#include "Logger.h"

// --- Define Keys ---
const char* ConfigManager::KEY_HIGH_PRESSURE = "p_high_thresh";
const char* ConfigManager::KEY_FAN_ON = "fan_on_thresh";
const char* ConfigManager::KEY_FAN_OFF = "fan_off_thresh";
const char* ConfigManager::KEY_FAN_HIGH = "fan_high_thresh";
const char* ConfigManager::KEY_P_LOW = "p_low_thresh";
const char* ConfigManager::KEY_P_MID = "p_mid_thresh";
const char* ConfigManager::KEY_CPM_WARN = "cpm_warn_ui";
const char* ConfigManager::KEY_CPM_DANGER = "cpm_danger_ui";
const char* ConfigManager::KEY_CO2_WARN = "co2_warn_ui";
const char* ConfigManager::KEY_CO2_DANGER = "co2_danger_ui";
const char* ConfigManager::KEY_VOC_WARN = "voc_warn_ui";
const char* ConfigManager::KEY_VOC_DANGER = "voc_danger_ui";
const char* ConfigManager::KEY_PM1_WARN = "pm1_warn_ui";
const char* ConfigManager::KEY_PM1_DANGER = "pm1_danger_ui";
const char* ConfigManager::KEY_PM25_WARN = "pm25_warn_ui";
const char* ConfigManager::KEY_PM25_DANGER = "pm25_danger_ui";
const char* ConfigManager::KEY_PM4_WARN = "pm4_warn_ui";
const char* ConfigManager::KEY_PM4_DANGER = "pm4_danger_ui";
const char* ConfigManager::KEY_PM10_WARN = "pm10_warn_ui";
const char* ConfigManager::KEY_PM10_DANGER = "pm10_danger_ui";


// --- Define Default Values ---
const float ConfigManager::DEFAULT_HIGH_PRESSURE_THRESHOLD = 150.0f;
const float ConfigManager::DEFAULT_FAN_ON_CURRENT_THRESHOLD = 1.0f;
const float ConfigManager::DEFAULT_FAN_OFF_CURRENT_THRESHOLD = 0.1f;
const float ConfigManager::DEFAULT_FAN_HIGH_CURRENT_THRESHOLD = 2.5f;
const float ConfigManager::DEFAULT_PRESSURE_LOW_THRESHOLD = 70.0f;
const float ConfigManager::DEFAULT_PRESSURE_MID_THRESHOLD = 130.0f;
const int ConfigManager::DEFAULT_CPM_WARN_THRESHOLD = 100;
const int ConfigManager::DEFAULT_CPM_DANGER_THRESHOLD = 300;
const int ConfigManager::DEFAULT_CO2_WARN_THRESHOLD = 1000;
const int ConfigManager::DEFAULT_CO2_DANGER_THRESHOLD = 2000;
const int ConfigManager::DEFAULT_VOC_WARN_THRESHOLD = 200;
const int ConfigManager::DEFAULT_VOC_DANGER_THRESHOLD = 300;
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
        highPressureThreshold = DEFAULT_HIGH_PRESSURE_THRESHOLD;
        fanOnCurrentThreshold = DEFAULT_FAN_ON_CURRENT_THRESHOLD;
        fanOffCurrentThreshold = DEFAULT_FAN_OFF_CURRENT_THRESHOLD;
        fanHighCurrentThreshold = DEFAULT_FAN_HIGH_CURRENT_THRESHOLD;
        pressureLowThreshold = DEFAULT_PRESSURE_LOW_THRESHOLD;
        pressureMidThreshold = DEFAULT_PRESSURE_MID_THRESHOLD;
        cpmWarnThreshold = DEFAULT_CPM_WARN_THRESHOLD;
        cpmDangerThreshold = DEFAULT_CPM_DANGER_THRESHOLD;
        co2WarnThreshold = DEFAULT_CO2_WARN_THRESHOLD;
        co2DangerThreshold = DEFAULT_CO2_DANGER_THRESHOLD;
        vocWarnThreshold = DEFAULT_VOC_WARN_THRESHOLD;
        vocDangerThreshold = DEFAULT_VOC_DANGER_THRESHOLD;
        pm1WarnThreshold = DEFAULT_PM1_WARN_THRESHOLD;
        pm1DangerThreshold = DEFAULT_PM1_DANGER_THRESHOLD;
        pm25WarnThreshold = DEFAULT_PM25_WARN_THRESHOLD;
        pm25DangerThreshold = DEFAULT_PM25_DANGER_THRESHOLD;
        pm4WarnThreshold = DEFAULT_PM4_WARN_THRESHOLD;
        pm4DangerThreshold = DEFAULT_PM4_DANGER_THRESHOLD;
        pm10WarnThreshold = DEFAULT_PM10_WARN_THRESHOLD;
        pm10DangerThreshold = DEFAULT_PM10_DANGER_THRESHOLD;
        return;
    }

    logger.info("ConfigManager initializing and loading values from NVS...");

    // Load physical thresholds
    highPressureThreshold = preferences.getFloat(KEY_HIGH_PRESSURE, DEFAULT_HIGH_PRESSURE_THRESHOLD);
    fanOnCurrentThreshold = preferences.getFloat(KEY_FAN_ON, DEFAULT_FAN_ON_CURRENT_THRESHOLD);
    fanOffCurrentThreshold = preferences.getFloat(KEY_FAN_OFF, DEFAULT_FAN_OFF_CURRENT_THRESHOLD);
    fanHighCurrentThreshold = preferences.getFloat(KEY_FAN_HIGH, DEFAULT_FAN_HIGH_CURRENT_THRESHOLD);
    
    // Load UI thresholds
    pressureLowThreshold = preferences.getFloat(KEY_P_LOW, DEFAULT_PRESSURE_LOW_THRESHOLD);
    pressureMidThreshold = preferences.getFloat(KEY_P_MID, DEFAULT_PRESSURE_MID_THRESHOLD);
    cpmWarnThreshold = preferences.getInt(KEY_CPM_WARN, DEFAULT_CPM_WARN_THRESHOLD);
    cpmDangerThreshold = preferences.getInt(KEY_CPM_DANGER, DEFAULT_CPM_DANGER_THRESHOLD);
    co2WarnThreshold = preferences.getInt(KEY_CO2_WARN, DEFAULT_CO2_WARN_THRESHOLD);
    co2DangerThreshold = preferences.getInt(KEY_CO2_DANGER, DEFAULT_CO2_DANGER_THRESHOLD);
    vocWarnThreshold = preferences.getInt(KEY_VOC_WARN, DEFAULT_VOC_WARN_THRESHOLD);
    vocDangerThreshold = preferences.getInt(KEY_VOC_DANGER, DEFAULT_VOC_DANGER_THRESHOLD);
    pm1WarnThreshold = preferences.getInt(KEY_PM1_WARN, DEFAULT_PM1_WARN_THRESHOLD);
    pm1DangerThreshold = preferences.getInt(KEY_PM1_DANGER, DEFAULT_PM1_DANGER_THRESHOLD);
    pm25WarnThreshold = preferences.getInt(KEY_PM25_WARN, DEFAULT_PM25_WARN_THRESHOLD);
    pm25DangerThreshold = preferences.getInt(KEY_PM25_DANGER, DEFAULT_PM25_DANGER_THRESHOLD);
    pm4WarnThreshold = preferences.getInt(KEY_PM4_WARN, DEFAULT_PM4_WARN_THRESHOLD);
    pm4DangerThreshold = preferences.getInt(KEY_PM4_DANGER, DEFAULT_PM4_DANGER_THRESHOLD);
    pm10WarnThreshold = preferences.getInt(KEY_PM10_WARN, DEFAULT_PM10_WARN_THRESHOLD);
    pm10DangerThreshold = preferences.getInt(KEY_PM10_DANGER, DEFAULT_PM10_DANGER_THRESHOLD);

    // Log the loaded values at a debug level for detailed startup info
    logger.debugf("Loaded Physical Config: P_High=%.1f, Fan_On=%.2f, Fan_Off=%.2f, Fan_High=%.2f", 
                highPressureThreshold, fanOnCurrentThreshold, fanOffCurrentThreshold, fanHighCurrentThreshold);
    logger.debugf("Loaded UI Config: P_Low=%.1f, P_Mid=%.1f, CPM_Warn=%d, CPM_Danger=%d, CO2_Warn=%d, CO2_Danger=%d, VOC_Warn=%d, VOC_Danger=%d",
                pressureLowThreshold, pressureMidThreshold, cpmWarnThreshold, cpmDangerThreshold, co2WarnThreshold, co2DangerThreshold, vocWarnThreshold, vocDangerThreshold);
    logger.debugf("Loaded PM Config: PM1(W%d,D%d), PM2.5(W%d,D%d), PM4(W%d,D%d), PM10(W%d,D%d)",
                pm1WarnThreshold, pm1DangerThreshold, pm25WarnThreshold, pm25DangerThreshold, pm4WarnThreshold, pm4DangerThreshold, pm10WarnThreshold, pm10DangerThreshold);
}

// --- Physical Sensor Getters ---
float ConfigManager::getHighPressureThreshold() { return highPressureThreshold; }
float ConfigManager::getFanOnCurrentThreshold() { return fanOnCurrentThreshold; }
float ConfigManager::getFanOffCurrentThreshold() { return fanOffCurrentThreshold; }
float ConfigManager::getFanHighCurrentThreshold() { return fanHighCurrentThreshold; }

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


// --- UI Threshold Getters ---
float ConfigManager::getPressureLowThreshold() { return pressureLowThreshold; }
float ConfigManager::getPressureMidThreshold() { return pressureMidThreshold; }
int ConfigManager::getCpmWarnThreshold() { return cpmWarnThreshold; }
int ConfigManager::getCpmDangerThreshold() { return cpmDangerThreshold; }
int ConfigManager::getCo2WarnThreshold() { return co2WarnThreshold; }
int ConfigManager::getCo2DangerThreshold() { return co2DangerThreshold; }
int ConfigManager::getVocWarnThreshold() { return vocWarnThreshold; }
int ConfigManager::getVocDangerThreshold() { return vocDangerThreshold; }
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
void ConfigManager::setCpmWarnThreshold(int value) {
    if (cpmWarnThreshold != value) {
        cpmWarnThreshold = value;
        if (preferences.putInt(KEY_CPM_WARN, value) > 0) {
            logger.infof("Saved new CPM WARN threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_CPM_WARN);
        }
    }
}
void ConfigManager::setCpmDangerThreshold(int value) {
    if (cpmDangerThreshold != value) {
        cpmDangerThreshold = value;
        if (preferences.putInt(KEY_CPM_DANGER, value) > 0) {
            logger.infof("Saved new CPM DANGER threshold: %d", value);
        } else {
            logger.errorf("Failed to save key '%s' to NVS.", KEY_CPM_DANGER);
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

#include "ZMOD4510Manager.h"
#include "Logger.h"
#include "SerialMutex.h"

ZMOD4510Manager::ZMOD4510Manager()
    : sensor(),
      latestValues(),
      initialized_(false),
      healthy_(false),
      new_data_available_(false),
      nano_connected_(false),
      nano_reboot_detected_(false),
      previous_nano_connected_(false) {
}

void ZMOD4510Manager::init() {
    logger.info("ZMOD4510Manager: Initializing");
    
    // Set initial state
    initialized_ = false;
    healthy_ = false;
    nano_connected_ = false;
    nano_reboot_detected_ = false;
    previous_nano_connected_ = false;

    sensor.initHAL();
    
    logger.info("ZMOD4510Manager: Ready for initialization");
}

void ZMOD4510Manager::process() {
    // Handle nano reboot detection
    if (nano_reboot_detected_) {
        logger.warning("ZMOD4510Manager: Nano reboot detected, resetting sensor");
        initialized_ = false;
        healthy_ = false;
        nano_reboot_detected_ = false;
    }
    
    // Only process if initialized
    if (!initialized_) {
        // Attempt initialization if nano is connected
        if (nano_connected_) {
            attemptInitialization();
        }
        return;
    }
    else {
        // Process the sensor
        sensor.process();
        
        // Check for new data
        if (sensor.hasNewData()) {
            ZMOD4510Sensor::Results results = sensor.getResults();
            
            if (results.valid) {
                latestValues.o3_conc_ppb = results.o3_conc_ppb;
                latestValues.no2_conc_ppb = results.no2_conc_ppb;
                latestValues.fast_aqi = results.fast_aqi;
                latestValues.epa_aqi = results.epa_aqi;
                latestValues.valid = true;
                new_data_available_ = true;
                
            } else {
                logger.warning("ZMOD4510: Invalid measurement results");
            }
        }
    }
}

bool ZMOD4510Manager::hasNewData() const {
    return new_data_available_;
}

ZMOD4510Manager::Values ZMOD4510Manager::getData() {
    new_data_available_ = false;
    return latestValues;
}

void ZMOD4510Manager::setEnvironmentalData(float temperature_degc, float humidity_pct) {
    sensor.setEnvironmentalData(temperature_degc, humidity_pct);
}

void ZMOD4510Manager::attemptInitialization() {
    static unsigned long last_attempt_time = 0;
    const unsigned long INIT_RETRY_INTERVAL_MS = 5000; // 5 seconds
    unsigned long current_time = millis();
    
    // Only attempt initialization if enough time has passed since last attempt
    if (current_time - last_attempt_time >= INIT_RETRY_INTERVAL_MS) {
        last_attempt_time = current_time;
        
        // Initialize the sensor
        if (!sensor.init()) {
            logger.error("Failed to initialize ZMOD4510 sensor");
            return;
        }
        
        initialized_ = true;
        healthy_ = true;
        logger.info("ZMOD4510Manager: Initialization successful");
    }
}

void ZMOD4510Manager::onConnectionStatusChanged(bool connected) {
    // Only log if the connection status actually changed
    if (connected != previous_nano_connected_) {
        if (!connected) {
            logger.warning("ZMOD4510Manager: Nano disconnected");
        } else {
            logger.info("ZMOD4510Manager: Nano connected");
        }
        previous_nano_connected_ = connected;
    }
    
    nano_connected_ = connected;
    
    if (!connected && initialized_) {
        initialized_ = false;
        healthy_ = false;
    }
}

void ZMOD4510Manager::onNanoReboot() {
    nano_reboot_detected_ = true;
}
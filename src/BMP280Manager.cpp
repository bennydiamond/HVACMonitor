#include "BMP280Manager.h"

BMP280Manager::BMP280Manager() 
    : initialized_(false), 
      healthy_(false), 
      new_data_available_(false),
      last_measurement_time_(0),
      nano_connected_(false),
      nano_reboot_detected_(false),
      previous_nano_connected_(false),
      state_(STATE_INIT) {
}

void BMP280Manager::begin() {
    logger.info("BMP280Manager: Initializing");
    
    // Set initial state
    initialized_ = false;
    healthy_ = false;
    nano_connected_ = false;
    nano_reboot_detected_ = false;
    previous_nano_connected_ = false;
    state_ = STATE_INIT;
}

void BMP280Manager::process() {
    // Handle nano reboot detection
    if (nano_reboot_detected_) {
        logger.warning("BMP280Manager: Nano reboot detected, resetting sensor");
        initialized_ = false;
        healthy_ = false;
        state_ = STATE_INIT;
        nano_reboot_detected_ = false;
    }
    
    switch (state_) {
        case STATE_INIT:
            // Only attempt initialization if nano is connected
            if (nano_connected_) {
                attemptInitialization();
            }
            break;
            
        case STATE_IDLE:
            // Check if it's time for a new measurement
            if (millis() - last_measurement_time_ >= MEASUREMENT_INTERVAL_MS) 
            {
                    state_ = STATE_MEASURING;
            }
            break;
            
        case STATE_MEASURING:
            // Check if measurement is complete
            if (sensor_.hasValue()) {
                new_data_available_ = true;
                last_measurement_time_ = millis();
                state_ = STATE_DATA_READY;
                logger.debug("BMP280Manager: Measurement complete");
            }
            break;
            
        case STATE_DATA_READY:
            // Wait for data to be consumed
            break;
    }
}

bool BMP280Manager::hasNewData() const {
    return new_data_available_;
}

BMP280Sensor::SensorData BMP280Manager::getData() {
    new_data_available_ = false;
    state_ = STATE_IDLE;
    return sensor_.getData();
}

float BMP280Manager::getPressure() {
    return sensor_.getPressure();
}

float BMP280Manager::getTemperature() {
    return sensor_.getTemperature();
}


void BMP280Manager::checkHealth() {
    // Simple health check - try to read sensor ID
    uint8_t id = sensor_.readID();
    if (id == BMx280MI::BMP280_ID || id == BMx280MI::BME280_ID) {
        healthy_ = true;
        logger.debugf("BMP280Manager: Health check passed, sensor ID: 0x%02X", id);
    } else {
        healthy_ = false;
        logger.warningf("BMP280Manager: Health check failed, sensor ID: 0x%02X", id);
    }
}

void BMP280Manager::attemptInitialization() {
    static unsigned long last_attempt_time = 0;
    unsigned long current_time = millis();
    
    // Only attempt initialization if enough time has passed since last attempt
    if (current_time - last_attempt_time >= INIT_RETRY_INTERVAL_MS) {
        last_attempt_time = current_time;
        
        if (sensor_.begin()) {
            initialized_ = true;
            healthy_ = true;
            state_ = STATE_IDLE;
            logger.info("BMP280Manager: Sensor initialization successful");
        } else {
            logger.error("BMP280Manager: Sensor initialization failed");
            initialized_ = false;
            healthy_ = false;
            // Stay in INIT state to retry
        }
    }
}

void BMP280Manager::onConnectionStatusChanged(bool connected) {
    // Only log if the connection status actually changed
    if (connected != previous_nano_connected_) {
        if (!connected) {
            logger.warning("BMP280Manager: Nano disconnected");
        } else {
            logger.info("BMP280Manager: Nano connected");
        }
        previous_nano_connected_ = connected;
    }
    
    nano_connected_ = connected;
    
    if (!connected && initialized_) {
        initialized_ = false;
        healthy_ = false;
        state_ = STATE_INIT;
    }
}

void BMP280Manager::onNanoReboot() {
    nano_reboot_detected_ = true;
} 
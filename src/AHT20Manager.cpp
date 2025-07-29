#ifdef AHT20_ENABLED
#include "AHT20Manager.h"

AHT20Manager& AHT20Manager::getInstance() {
    static AHT20Manager* instance = nullptr;
    if (!instance) {
        instance = new AHT20Manager();
    }
    return *instance;
}

AHT20Manager::AHT20Manager() 
    : currentState(STATE_INIT),
      stateStartTime(0),
      lastMeasurementTime(0),
      nano_connected_(false),
      initialized(false),
      healthy(false),
      new_data_available(false),
      nano_reboot_detected_(false),
      previous_nano_connected_(false),
      sensor(AHT20_DEFAULT_ADDRESS) {
}

void AHT20Manager::init() {
    logger.info("AHT20Manager: Initializing");
    lastMeasurementTime = 0;
}

void AHT20Manager::process() {
    unsigned long currentTime = millis();
    
    // Handle Nano reboot detection
    if (nano_reboot_detected_) {
        logger.warning("AHT20Manager: Nano reboot detected, resetting sensor");
        currentState = STATE_INIT;
        initialized = false;
        healthy = false;
        nano_reboot_detected_ = false;
        stateStartTime = currentTime;
    }
    
    // State machine
    switch (currentState) {
        case STATE_INIT:
            processInitState(currentTime);
            break;
            
        case STATE_START_MEASURE:
            processStartMeasureState(currentTime);
            break;
            
        case STATE_WAIT:
            processWaitState(currentTime);
            break;
            
        case STATE_ACQUIRE:
            processAcquireState(currentTime);
            break;
    }
}

bool AHT20Manager::hasNewData() const {
    return new_data_available;
}

AHT20Manager::Values AHT20Manager::getData() {
    new_data_available = false;
    return latestValues;
}

void AHT20Manager::onConnectionStatusChanged(bool connected) {
    // Only log if the connection status actually changed
    if (connected != previous_nano_connected_) {
        if (!connected) {
            logger.warning("AHT20Manager: Nano disconnected");
        } else {
            logger.info("AHT20Manager: Nano connected");
        }
        previous_nano_connected_ = connected;
    }
    
    nano_connected_ = connected;
    
    if (!connected) {
        // Reset state when Nano disconnects
        initialized = false;
        healthy = false;
        currentState = STATE_INIT;
        stateStartTime = millis();
    }
}

void AHT20Manager::onNanoReboot() {
    nano_reboot_detected_ = true;
}

void AHT20Manager::processInitState(unsigned long currentTime) {
    // Only attempt initialization if Nano is connected and enough time has passed
    if (!nano_connected_) {
        return; // Wait for Nano connection
    }
    
    if (currentTime - stateStartTime >= INIT_RETRY_INTERVAL_MS) {
        stateStartTime = currentTime;
        
        // Initialize the sensor
        if (!sensor.init()) {
            logger.error("AHT20Manager: Failed to initialize AHT20 sensor");
            healthy = false;
            return;
        }
        
        initialized = true;
        healthy = true;
        logger.info("AHT20Manager: Initialization successful");
        
        // Transition to start measure state
        currentState = STATE_START_MEASURE;
        stateStartTime = currentTime;
    }
}

void AHT20Manager::processStartMeasureState(unsigned long currentTime) {
    // Check if it's time for a new measurement
    if (currentTime - lastMeasurementTime >= MEASUREMENT_INTERVAL_MS) {
        // Trigger a new measurement
        if (sensor.triggerMeasurement()) {
            lastMeasurementTime = currentTime;
            currentState = STATE_WAIT;
            stateStartTime = currentTime;
            logger.debug("AHT20Manager: Measurement triggered");
        } else {
            logger.warning("AHT20Manager: Failed to trigger measurement");
            healthy = false;
            // Stay in this state and try again
        }
    }
}

void AHT20Manager::processWaitState(unsigned long currentTime) {
    // Wait for measurement to complete
    if (currentTime - stateStartTime >= WAIT_DURATION_MS) {
        currentState = STATE_ACQUIRE;
        stateStartTime = currentTime;
    }
}

void AHT20Manager::processAcquireState(unsigned long currentTime) {
    // Check if sensor is busy
    if (sensor.isBusy()) {
        // Still busy, go back to wait state
        currentState = STATE_WAIT;
        stateStartTime = currentTime;
        logger.debug("AHT20Manager: Sensor still busy, waiting more");
        return;
    }
    
    // Sensor not busy, try to read data
    if (sensor.newData()) {
        float temperature, humidity;
        bool tempValid = sensor.getTemperature(temperature);
        bool humValid = sensor.getHumidity(humidity);
        
        if (tempValid && humValid) {
            latestValues.temperature_degc = temperature;
            latestValues.humidity_pct = humidity;
            latestValues.valid = true;
            new_data_available = true;
            healthy = true;
            logger.debugf("AHT20Manager: New data - T=%.1f°C, H=%.1f%%", temperature, humidity);
        } else {
            logger.warning("AHT20Manager: Invalid measurement results");
            healthy = false;
        }
    } else {
        logger.debug("AHT20Manager: No new data available");
        healthy = false;
    }
    
    // Go back to start measure state for next measurement cycle
    currentState = STATE_START_MEASURE;
} 

#endif // AHT20_ENABLED
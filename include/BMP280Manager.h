#pragma once

#include <Arduino.h>
#include "BMP280Sensor.h"
#include "Logger.h"

class BMP280Manager {
public:
    // Singleton instance
    static BMP280Manager& getInstance() {
        static BMP280Manager instance;
        return instance;
    }
    
    // Initialize the manager (simple initialization that cannot fail)
    void begin();
    
    // Process the sensor (call in loop) - handles complex initialization and error recovery
    void process();
    
    // Check if new data is available
    bool hasNewData() const;
    
    // Get the latest sensor data
    BMP280Sensor::SensorData getData();
    
    // Get pressure value
    float getPressure();
    
    // Get temperature value
    float getTemperature();
    
    // Get sensor status
    bool isInitialized() const { return initialized_; }
    bool isHealthy() const { return healthy_; }
    
    // Handle connection status changes from SensorTask
    void onConnectionStatusChanged(bool connected);
    
    // Handle nano reboot detection from SensorTask
    void onNanoReboot();
    


private:
    BMP280Manager(); // Private constructor for singleton
    
    BMP280Sensor sensor_;
    bool initialized_;
    bool healthy_;
    bool new_data_available_;
    unsigned long last_measurement_time_;
    bool nano_connected_;
    bool nano_reboot_detected_;
    bool previous_nano_connected_;
    
    // State management
    enum State {
        STATE_INIT,
        STATE_IDLE,
        STATE_MEASURING,
        STATE_DATA_READY
    };
    State state_;
    
private:
    static const unsigned long MEASUREMENT_INTERVAL_MS = 4050; // Just a bit over 4000ms, standby time of BMP280
    static const unsigned long INIT_RETRY_INTERVAL_MS = 5000;
    
    // Helper methods
    void checkHealth();
    void attemptInitialization();
}; 
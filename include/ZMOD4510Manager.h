#pragma once

#include "ZMOD4510Sensor.h"

class ZMOD4510Manager {
public:
    // Singleton instance
    static ZMOD4510Manager& getInstance() {
        static ZMOD4510Manager instance;
        return instance;
    }

    typedef struct Values {
        Values() : o3_conc_ppb(0), no2_conc_ppb(0), fast_aqi(0), epa_aqi(0), valid(false) {}
        uint16_t o3_conc_ppb;    // O3 concentration in ppb
        uint16_t no2_conc_ppb;   // NO2 concentration in ppb
        uint16_t fast_aqi;    // Fast AQI value
        uint16_t epa_aqi;     // EPA AQI value
        bool valid;           // Whether the results are valid
    } Values;
    
    // Initialize the manager (simple initialization that cannot fail)
    void init();
    
    // Process the sensor (call in loop) - handles complex initialization and error recovery
    void process();
    
    // Check if new data is available
    bool hasNewData() const;
    
    // Get the latest sensor data
    Values getData();
    
    // Update environmental data (temperature and humidity) for algo calculations
    void setEnvironmentalData(float temperature_degc, float humidity_pct);
    
    // Get sensor status
    bool isInitialized() const { return initialized_; }
    bool isHealthy() const { return healthy_; }
    
    // Handle connection status changes from SensorTask
    void onConnectionStatusChanged(bool connected);
    
    // Handle nano reboot detection from SensorTask
    void onNanoReboot();
    
    // Attempt sensor initialization (called internally)
    void attemptInitialization();
    
private:
    ZMOD4510Manager(); // Private constructor for singleton
    
    ZMOD4510Sensor sensor;
    bool initialized_;
    bool healthy_;
    bool new_data_available_;
    bool nano_connected_;
    bool nano_reboot_detected_;
    bool previous_nano_connected_; // Track previous connection state
    Values latestValues;
};

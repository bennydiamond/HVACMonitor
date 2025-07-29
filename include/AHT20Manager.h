#pragma once

#ifdef AHT20_ENABLED

#include "AHT20.h"
#include "Logger.h"

class AHT20Manager {
public:
    // State machine states
    enum State {
        STATE_INIT,
        STATE_START_MEASURE,
        STATE_WAIT,
        STATE_ACQUIRE
    };
    
    // Measurement values structure
    typedef struct Values {
        Values() : temperature_degc(0), humidity_pct(0), valid(false) {}
        float temperature_degc;
        float humidity_pct;
        bool valid;
    } Values;
    
    static AHT20Manager& getInstance();
    
    // Initialize the manager
    void init();
    
    // Process the sensor (call this in the task loop)
    void process();
    
    // Check if new data is available
    bool hasNewData() const;
    
    // Get the latest sensor data
    Values getData();
    
    // Handle connection status changes
    void onConnectionStatusChanged(bool connected);
    
    // Handle Nano reboot detection
    void onNanoReboot();
    
private:
    // Private constructor for singleton
    AHT20Manager();
    
    // State machine
    State currentState;
    unsigned long stateStartTime;
    unsigned long lastMeasurementTime;
    
    // Timing constants
    static const unsigned long MEASUREMENT_INTERVAL_MS = 5000;  // 5 seconds between measurements
    static const unsigned long WAIT_DURATION_MS = 90;           // 90ms wait for measurement completion
    static const unsigned long INIT_RETRY_INTERVAL_MS = 1000;   // 1 second between init retries
    
    // Connection and health tracking
    bool nano_connected_;
    bool initialized;
    bool healthy;
    bool new_data_available;
    bool nano_reboot_detected_;
    bool previous_nano_connected_;
    
    // Latest sensor values
    Values latestValues;
    
    AHT20 sensor;

    // State machine processing methods
    void processInitState(unsigned long currentTime);
    void processStartMeasureState(unsigned long currentTime);
    void processWaitState(unsigned long currentTime);
    void processAcquireState(unsigned long currentTime);
}; 

#endif // AHT20_ENABLED 
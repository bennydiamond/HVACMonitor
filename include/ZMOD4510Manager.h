#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "ZMOD4510Sensor.h"

class ZMOD4510Manager {
public:

    typedef struct Values {
        Values() : o3_conc_ppb(0), no2_conc_ppb(0), fast_aqi(0), epa_aqi(0), valid(false) {}
        uint16_t o3_conc_ppb;    // O3 concentration in ppb
        uint16_t no2_conc_ppb;   // NO2 concentration in ppb
        uint16_t fast_aqi;    // Fast AQI value
        uint16_t epa_aqi;     // EPA AQI value
        bool valid;           // Whether the results are valid
    } Values;
    
    ZMOD4510Manager();
    ~ZMOD4510Manager();
    
    // Initialize the manager and start the task
    void init();
    
    // Process the manager (call in loop)
    // Thread-safe
    bool process(bool sensorStackConnected, bool firstTimeFlag, Values& outValues);

    // Update environmental data (temperature and humidity) for algo calculations
    // Thread-safe
    void setEnvironmentalData(float temperature_degc, float humidity_pct);
    
private:
    enum State {
        STATE_WAITING_FOR_NANO,  // Waiting for Nano to connect
        STATE_INITIALIZING,      // Initializing the sensor
        STATE_READY              // Sensor is ready for use
    };
    
    static void taskFunction(void* parameter);
    void taskLoop();
    
    ZMOD4510Sensor sensor;
    State state;
    Values latestValues;
    bool previousFirstTimeFlag;
    
    TaskHandle_t taskHandle;
    
    // Shared state variables
    bool sensorStackConnected;
    bool firstTimeFlag;
    SemaphoreHandle_t stateMutex;
    StaticSemaphore_t stateMutexBuffer;
};

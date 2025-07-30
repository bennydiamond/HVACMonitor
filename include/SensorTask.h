#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "ZMOD4510Manager.h"
#include "BMP280Manager.h"
#ifdef AHT20_ENABLED
#include "AHT20Manager.h"
#endif

class SensorTask {
public:
    // ZMOD4510 sensor data structure
    typedef struct ZMOD4510Values {
        ZMOD4510Values() : o3_conc_ppb(0), no2_conc_ppb(0), fast_aqi(0), epa_aqi(0), valid(false) {}
        uint16_t o3_conc_ppb;
        uint16_t no2_conc_ppb;
        uint16_t fast_aqi;
        uint16_t epa_aqi;
        bool valid;
    } ZMOD4510Values;
    
    // BMP280 sensor data structure
    typedef struct BMP280Values {
        BMP280Values() : pressure_pa(0), temperature_degc(0), valid(false) {}
        float pressure_pa;
        float temperature_degc;
        bool valid;
    } BMP280Values;
#ifdef AHT20_ENABLED
    // AHT20 sensor data structure
    typedef struct AHT20Values {
        AHT20Values() : temperature_degc(0), humidity_pct(0), valid(false) {}
        float temperature_degc;
        float humidity_pct;
        bool valid;
    } AHT20Values;
#endif
    SensorTask();
    ~SensorTask();
    
    // Initialize the task and start it
    void init();
    
    // Get ZMOD4510 sensor data (thread-safe)
    bool getZMOD4510Data(ZMOD4510Values& outValues);
    
    // Get BMP280 sensor data (thread-safe)
    bool getBMP280Data(BMP280Values& outValues);
#ifdef AHT20_ENABLED
    // Get AHT20 sensor data (thread-safe)
    bool getAHT20Data(AHT20Values& outValues);
#endif
    // Update environmental data for ZMOD4510 algo calculations (thread-safe)
    void setEnvironmentalData(float temperature_degc, float humidity_pct);
    
    // Update connection status and first time flag (thread-safe)
    void updateConnectionStatus(bool sensorStackConnected, bool firstTimeFlag);
    
private:
    static void taskFunction(void* parameter);
    void taskLoop();
    
    ZMOD4510Values latestZMOD4510Values;
    BMP280Values latestBMP280Values;
#ifdef AHT20_ENABLED
    AHT20Values latestAHT20Values;
#endif
    // State variables
    bool previousFirstTimeFlag;
    bool firstTimeFlag;
    bool firstHealthCheckReceived;
    bool rebootDetected;
    
    TaskHandle_t taskHandle;
    
    // Shared state variables
    bool sensorStackConnected;
    SemaphoreHandle_t stateMutex;
    StaticSemaphore_t stateMutexBuffer;
}; 
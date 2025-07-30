#pragma once

#include <Arduino.h>
#include "zmod4510/no2_o3-arduino.h"
#include "I2CBridge.h"
#include "Logger.h"

class ZMOD4510Sensor {
public:
    // Measurement results structure
    struct Results {
        float rmox[4];        // Sensor resistance in kOhm
        uint16_t o3_conc_ppb;    // O3 concentration in ppb
        uint16_t no2_conc_ppb;   // NO2 concentration in ppb
        uint16_t fast_aqi;         // Fast AQI value
        uint16_t epa_aqi;          // EPA AQI value
        bool valid;           // Whether the results are valid
    };

    ZMOD4510Sensor();
    ~ZMOD4510Sensor();

    // Initialize HAL and data structures (call once at startup)
    bool initHAL();
    
    // Initialize the sensor (call when needed to configure the sensor)
    bool init();

    // Start a measurement cycle (non-blocking)
    void startMeasurement();

    // Process the sensor (call in loop)
    void process();

    // Check if new data is available
    bool hasNewData() const;

    // Get the latest measurement results
    Results getResults();

    // Set temperature and humidity values from external sensor
    void setEnvironmentalData(float temperature_degc, float humidity_pct);

private:
    enum State {
        STATE_IDLE,
        STATE_MEASURING,
        STATE_WAITING,
        STATE_READING_RESULTS,
        STATE_DATA_READY
    };

    // Hardware interface
    Interface_t hal;
    
    // Sensor data structures
    zmod4xxx_dev_t dev;
    uint8_t zmod4xxx_status;
    uint8_t adc_result[ZMOD4510_ADC_DATA_LEN];
    uint8_t prod_data[ZMOD4510_PROD_DATA_LEN];
    
    // Algorithm data structures
    no2_o3_handle_t algo_handle;
    no2_o3_results_t algo_results;
    no2_o3_inputs_t algo_input;
    
    // Environmental data
    float temperature_degc;
    float humidity_pct;
    
    // State management
    State state;
    Results latest_results;
    bool new_data_available;
    unsigned long measurement_start_time;
    
    // Helper methods
    int detect_and_configure();
    void read_and_verify();
    void processResults();
};
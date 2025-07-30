#pragma once

#include <Arduino.h>
#include <BMx280MI.h>
#include "I2CBridge.h"
#include "Logger.h"

// BMP280 sensor class that uses I2CBridge for communication
class BMP280Sensor : public BMx280MI {
public:
    // Constructor
    BMP280Sensor(uint8_t i2c_address = 0x77); // Could also be 0x76 depending on SDO pin state
    ~BMP280Sensor();

    // Sensor data structure
    struct SensorData {
        float pressure_pa;
        float temperature_degc;
        bool valid;
        unsigned long timestamp;
    };

    // Initialize the sensor
    bool begin();
    
    // Start a measurement (forced mode only)
    bool startMeasurement();
    
    // Check if measurement is complete
    bool hasValue();
    
    // Get the latest sensor data
    SensorData getData();
    
    // Get pressure value
    float getPressure();
    
    // Get temperature value (for compensation)
    float getTemperature();

private:
    uint8_t address_;
    SensorData latest_data_;
    bool data_valid_;
    
    // Override virtual functions from BMx280MI
    bool beginInterface() override;
    uint8_t readRegister(uint8_t reg) override;
    uint32_t readRegisterBurst(uint8_t reg, uint8_t length) override;
    void writeRegister(uint8_t reg, uint8_t data) override;
}; 
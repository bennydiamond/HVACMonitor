#ifdef AHT20_ENABLED

/****************************************************************
 * AHT20.h
 * AHT20 Temperature and Humidity Sensor Library Header File
 * Adapted to use I2CBridge for ESP32-Nano communication
 * 
 * This file defines the AHT20 class for interfacing with the
 * AHT20 temperature and humidity sensor via I2CBridge.
 * 
 * Distributed as-is; no warranty is given.
 ***************************************************************/

#ifndef AHT20_H
#define AHT20_H

#include <Arduino.h>
#include "I2CBridge.h"

// AHT20 I2C Address
#define AHT20_DEFAULT_ADDRESS 0x38

// AHT20 Register Commands
#define AHT20_REG_CALIBRATE 0xBE
#define AHT20_REG_MEASURE   0xAC
#define AHT20_REG_RESET     0xBA

// AHT20 Status Bits
#define AHT20_STATUS_BUSY     0x80
#define AHT20_STATUS_CAL      0x08

// AHT20 Error Status
#define AHT20_NO_ERROR        0x00
#define AHT20_BUSY_ERROR      0x01
#define AHT20_ACK_ERROR       0x02
#define AHT20_DATA_ERROR      0x03
#define AHT20_CRC8_ERROR      0x04

// AHT20 CRC-8 constants
#define AHT20_CRC8_POLYNOMIAL 0x31  // CRC-8-Maxim polynomial
#define AHT20_CRC8_INIT       0xFF  // Initial value

class AHT20 {
public:
    // Constructor
    AHT20(uint8_t address = AHT20_DEFAULT_ADDRESS);
    
    // Destructor
    ~AHT20();
    
    // Initialize the sensor
    bool init();
    
    // Check if new data is available (sensor calibrated and not busy)
    bool newData();
    
    // Get temperature validity flag, value passed as parameter
    bool getTemperature(float& temperature);
    
    // Get humidity validity flag, value passed as parameter
    bool getHumidity(float& humidity);
    
    // Soft reset the sensor (useful for recovery)
    bool softReset();
    
    // Get error status
    uint8_t getStatus() const { return _status; }
    
    // Check if sensor is healthy
    bool isHealthy() const { return _status == AHT20_NO_ERROR; }
    
    // Check if sensor is ready for new measurement
    bool isReadyForMeasurement() const;
    
    // Check if sensor is busy (for state machine)
    bool isBusy() const;
    
    // Trigger a new measurement (for state machine)
    bool triggerMeasurement();
    
    // Get detailed sensor status for debugging
    void getDetailedStatus(uint8_t& status, bool& busy, bool& calibrated) const;

private:
    // I2C address
    uint8_t _deviceAddress;
    
    // Sensor data structure
    struct {
        uint32_t temperature;
        uint32_t humidity;
    } sensorData;
    
    // Sensor state
    bool _initialized;
    bool _measurementStarted;
    bool _dataValid;
    uint8_t _status; // Error status tracking
    
    // Private methods
    bool SensorInit();
    bool isCalibrated();
    uint8_t getStatus();
    bool calibrate();
    bool readData();
    uint8_t calculateCRC8(const uint8_t* data, uint8_t length);
};

#endif // AHT20_H

#endif // AHT20_ENABLED 
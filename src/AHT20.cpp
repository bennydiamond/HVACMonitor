/****************************************************************
 * AHT20.cpp
 * AHT20 Temperature and Humidity Sensor Library Implementation
 * Adapted to use I2CBridge for ESP32-Nano communication
 * 
 * This file implements all the functions of the AHT20 class.
 * Functions in this library can return the humidity and temperature 
 * measured by the sensor. All data is communicated over I2C bus
 * via the I2CBridge.
 * 
 * Distributed as-is; no warranty is given.
 ***************************************************************/

#ifdef AHT20_ENABLED

#include "AHT20.h"
#include "Logger.h"

AHT20::AHT20(uint8_t address) 
    : _deviceAddress(address), 
      _initialized(false),
      _measurementStarted(false),
      _dataValid(false),
      _status(AHT20_NO_ERROR) {
    sensorData.temperature = 0;
    sensorData.humidity = 0;
}

AHT20::~AHT20() {
    // Nothing to clean up
}

bool AHT20::init() {
    logger.info("AHT20: Initializing sensor");
    
    _status = AHT20_NO_ERROR; // Reset status
    
    _initialized = SensorInit();
    
    // Wait 40 ms after power-on before reading temp or humidity. Datasheet pg 8
    delay(40);
    
    if (_initialized) {
        logger.info("AHT20: Sensor initialized successfully");
    } else {
        logger.error("AHT20: Sensor initialization failed");
        _status = AHT20_ACK_ERROR;
    }
    
    return _initialized;
}

bool AHT20::newData() {
    if (!_initialized || !_measurementStarted) {
        _status = AHT20_ACK_ERROR;
        return false;
    }
    
    // Check if sensor is busy (measurement in progress)
    if (isBusy()) {
        _status = AHT20_BUSY_ERROR;
        return false; // Still busy
    }
    
    // Measurement complete, read data
    if (!readData()) {
        _status = AHT20_DATA_ERROR;
        return false;
    }
    
    _dataValid = true;
    _status = AHT20_NO_ERROR;
    
    // Trigger next measurement for continuous sampling
    if (!triggerMeasurement()) {
        logger.error("AHT20: Failed to trigger next measurement");
        _status = AHT20_ACK_ERROR;
        return false;
    }
    
    return true;
}

bool AHT20::getTemperature(float& temperature) {
    if (!_dataValid || _status != AHT20_NO_ERROR) {
        temperature = -999.0f;
        return false;
    }
    
    // From datasheet pg 8
    temperature = ((float)sensorData.temperature / 1048576) * 200 - 50;
    return true;
}

bool AHT20::getHumidity(float& humidity) {
    if (!_dataValid || _status != AHT20_NO_ERROR) {
        humidity = -999.0f;
        return false;
    }
    
    // From datasheet pg 8
    humidity = ((float)sensorData.humidity / 1048576) * 100;
    return true;
}

// Reset sensor to initial state
// Init the sensor and wait for calibration
bool AHT20::SensorInit() {
    if (!softReset()) {
        logger.error("AHT20: Soft reset failed");
        _status = AHT20_ACK_ERROR;
        return false;
    }

    // Soft reset takes 20ms
    delay(20);
    
    uint8_t queryRetry = 100;
    while(queryRetry--) {
        if(isBusy() == false) {
            bool calResult = calibrate();
            if (calResult) {
                _status = AHT20_NO_ERROR;
                return true;
            } else {
                logger.error("AHT20: Calibration failed");
                _status = AHT20_ACK_ERROR;
                return false;
            }
        }
        delay(1);
    }
    
    logger.error("AHT20: Sensor busy timeout after reset");
    _status = AHT20_BUSY_ERROR;
    return false;
}

uint8_t AHT20::getStatus() {
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeReadBytes(_deviceAddress, nullptr, 0, 1);
    
    if (result.success && result.data_len > 0) {
        return result.data[0];
    }
    
    _status = AHT20_ACK_ERROR;
    return 0;
}

// Returns the state of the cal bit in the status byte
bool AHT20::isCalibrated() {
    return (getStatus() & AHT20_STATUS_CAL);
}

bool AHT20::isBusy() const {
    // Check if sensor is busy by reading status
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeReadBytes(_deviceAddress, nullptr, 0, 1);
    
    if (result.success && result.data_len > 0) {
        uint8_t status = result.data[0];
        return (status & AHT20_STATUS_BUSY) != 0; // Return true if busy
    }
    
    return true; // Assume busy on communication error
}

bool AHT20::calibrate() {
    uint8_t cmd[3] = {AHT20_REG_CALIBRATE, 0x08, 0x00};
    
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeBytes(_deviceAddress, cmd, 3);

    if(result.success) {
        uint8_t queryRetry = 100;
        while(queryRetry--) {
            if(isBusy() == false) {
                if (isCalibrated()) {
                    logger.info("AHT20: Calibration successful");
                    return true;
                } else {
                    logger.error("AHT20: Calibration bit not set");
                    return false;
                }
            }
            delay(1);
        }
        logger.error("AHT20: Calibration busy timeout");
        return false;
    } else {
        logger.error("AHT20: Failed to send calibration command");
        return false;
    }
}

bool AHT20::triggerMeasurement() {
    uint8_t cmd[3] = {AHT20_REG_MEASURE, 0x33, 0x00};
    
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeBytes(_deviceAddress, cmd, 3);
    
    if (!result.success) {
        logger.error("AHT20: Failed to trigger measurement");
        _status = AHT20_ACK_ERROR;
        return false;
    }
    else
    {
        _measurementStarted = true;
    }
    
    return true;
}

bool AHT20::isReadyForMeasurement() const {
    // Check if sensor is initialized and not busy
    if (!_initialized) {
        return false;
    }
    
    // Check if sensor is busy by reading status
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeReadBytes(_deviceAddress, nullptr, 0, 1);
    
    if (result.success && result.data_len > 0) {
        uint8_t status = result.data[0];
        return !(status & AHT20_STATUS_BUSY); // Return true if not busy
    }
    
    return false; // Communication error
}

void AHT20::getDetailedStatus(uint8_t& status, bool& busy, bool& calibrated) const {
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeReadBytes(_deviceAddress, nullptr, 0, 1);
    
    if (result.success && result.data_len > 0) {
        status = result.data[0];
        busy = (status & AHT20_STATUS_BUSY) != 0;
        calibrated = (status & AHT20_STATUS_CAL) != 0;
    } else {
        status = 0;
        busy = true;
        calibrated = false;
    }
}

// Loads the sensor data
bool AHT20::readData() {
    // Clear previous data
    sensorData.temperature = 0;
    sensorData.humidity = 0;
    
    I2CBridge& i2c = I2CBridge::getInstance();
    // Read 7 bytes: {status, RH, RH, RH+T, T, T, CRC}
    I2CBridge::Result result = i2c.writeReadBytes(_deviceAddress, nullptr, 0, 7);
    
    if (!result.success || result.data_len < 7) {
        logger.error("AHT20: Failed to read sensor data");
        _status = AHT20_DATA_ERROR;
        return false;
    }
    
    // Read and discard state
    uint8_t state = result.data[0];
    
    // Check if sensor is still busy
    if (state & AHT20_STATUS_BUSY) {
        logger.debug("AHT20: Sensor still busy during data read");
        _status = AHT20_BUSY_ERROR;
        return false;
    }
    
    // Check CRC-8 for data integrity
    uint8_t expectedCRC = calculateCRC8(result.data, 6);
    uint8_t receivedCRC = result.data[6];
    
    if (expectedCRC != receivedCRC) {
        logger.error("AHT20: CRC-8 validation failed");
        _status = AHT20_CRC8_ERROR;
        return false;
    }
    
    uint32_t incoming = 0;
    incoming |= (uint32_t)result.data[1] << (8 * 2);
    incoming |= (uint32_t)result.data[2] << (8 * 1);
    uint8_t midByte = result.data[3];
    
    incoming |= midByte;
    sensorData.humidity = incoming >> 4;
    
    sensorData.temperature = (uint32_t)midByte << (8 * 2);
    sensorData.temperature |= (uint32_t)result.data[4] << (8 * 1);
    sensorData.temperature |= (uint32_t)result.data[5] << (8 * 0);
    
    // Need to get rid of data in bits > 20
    sensorData.temperature = sensorData.temperature & ~(0xFFF00000);
    
    _status = AHT20_NO_ERROR;
    return true;
}

bool AHT20::softReset() {
    uint8_t cmd[1] = {AHT20_REG_RESET};
    
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeBytes(_deviceAddress, cmd, 1);
    
    if (result.success) {
        // Reset internal state
        _initialized = false;
        _measurementStarted = false;
        _dataValid = false;
        _status = AHT20_NO_ERROR;
        logger.info("AHT20: Soft reset completed");
        return true;
    } else {
        logger.error("AHT20: Soft reset failed");
        _status = AHT20_ACK_ERROR;
        return false;
    }
} 

uint8_t AHT20::calculateCRC8(const uint8_t* data, uint8_t length) {
    uint8_t crc = AHT20_CRC8_INIT;  // Initial value
    
    for (uint8_t byteIndex = 0; byteIndex < length; byteIndex++) {
        crc ^= data[byteIndex];
        
        for (uint8_t bitIndex = 8; bitIndex > 0; --bitIndex) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ AHT20_CRC8_POLYNOMIAL;
            } else {
                crc = (crc << 1);
            }
        }
    }
    
    return crc;
} 

#endif // AHT20_ENABLED
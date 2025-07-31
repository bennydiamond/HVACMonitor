#include "BMP280Sensor.h"

BMP280Sensor::BMP280Sensor(uint8_t i2c_address) 
    : address_(i2c_address), data_valid_(false) {
    memset(&latest_data_, 0, sizeof(SensorData));
    latest_data_.valid = false;
}

BMP280Sensor::~BMP280Sensor() {
    // Nothing to clean up
}

bool BMP280Sensor::begin() {
    logger.info("BMP280: Initializing sensor");
    
    // Call the parent class begin() which will use our overridden methods
    if (!BMx280MI::begin()) {
        logger.error("BMP280: Sensor initialization failed");
        return false;
    }
    
    // Reset sensor to default parameters
    resetToDefaults();
    
    // Configure oversampling settings for pressure only
    writeOversamplingPressure(BMx280MI::OSRS_P_x16);
    writeOversamplingTemperature(BMx280MI::OSRS_T_x16); // Still needed for pressure calculation
    
    // Enable filtering to reduce noise (x16 filter for maximum stability)
    writeFilterSetting(BMx280MI::FILTER_x16);
    
    // Set to normal mode for continuous measurements
    writeStandbyTime(BMx280MI::T_SB_6); // 2000ms standby time
    writePowerMode(BMx280MI::BMx280_MODE_NORMAL);
    
    return true;
}

bool BMP280Sensor::startMeasurement() {
    return measure();
}

bool BMP280Sensor::hasValue() {
    if (BMx280MI::hasValue()) {
        // Update our data structure with pressure and temperature
        latest_data_.pressure_pa = BMx280MI::getPressure();
        latest_data_.temperature_degc = BMx280MI::getTemperature();
        latest_data_.valid = true;
        latest_data_.timestamp = millis();
        data_valid_ = true;
        
        return true;
    }
    return false;
}

BMP280Sensor::SensorData BMP280Sensor::getData() {
    return latest_data_;
}

float BMP280Sensor::getPressure() {
    return latest_data_.pressure_pa;
}

float BMP280Sensor::getTemperature() {
    return BMx280MI::getTemperature();
}

// Override virtual functions from BMx280MI

bool BMP280Sensor::beginInterface() {
    // Nothing to do here as I2CBridge is already initialized
    return true;
}

uint8_t BMP280Sensor::readRegister(uint8_t reg) {
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeReadBytes(address_, &reg, 1, 1);
    
    if (!result.success || result.data_len == 0) {
        logger.errorf("BMP280: Failed to read register 0x%02X", reg);
        return 0;
    }
    
    return result.data[0];
}

uint32_t BMP280Sensor::readRegisterBurst(uint8_t reg, uint8_t length) {
    if (length > 4) {
        logger.errorf("BMP280: Burst read length %d exceeds maximum of 4", length);
        return 0;
    }
    
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeReadBytes(address_, &reg, 1, length);
    
    if (!result.success || result.data_len != length) {
        logger.errorf("BMP280: Failed to read %d bytes from register 0x%02X", length, reg);
        return 0;
    }
    
    // Combine bytes into 32-bit value (LSB = last byte read)
    uint32_t data = 0;
    for (uint8_t i = 0; i < length; i++) {
        data <<= 8;
        data |= result.data[i];
    }
    
    return data;
}

void BMP280Sensor::writeRegister(uint8_t reg, uint8_t data) {
    uint8_t write_data[2] = {reg, data};
    
    I2CBridge& i2c = I2CBridge::getInstance();
    I2CBridge::Result result = i2c.writeBytes(address_, write_data, 2);
    
    if (!result.success) {
        logger.errorf("BMP280: Failed to write 0x%02X to register 0x%02X", data, reg);
    }
} 
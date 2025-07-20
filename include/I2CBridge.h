#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "NanoCommands.h"

// I2C Bridge class for communicating with sensors via the Arduino Nano
class I2CBridge {
public:
    
    // Result structure for I2C operations
    struct Result {
        bool success;
        uint8_t error_code;
        uint8_t data[32]; // Max 32 bytes of data
        uint8_t data_len;
    };
    
    // Singleton instance
    static I2CBridge& getInstance() {
        static I2CBridge instance;
        return instance;
    }
    
    static bool begin();
    static void setTimeout(unsigned long timeout_ms) { _timeout_ms = timeout_ms; }
    
    // Basic I2C operations
    static Result readBytes(uint8_t addr, uint8_t len);
    static Result writeBytes(uint8_t addr, uint8_t* data, uint8_t len);
    static Result writeReadBytes(uint8_t addr, uint8_t* writeData, uint8_t writeLen, uint8_t readLen);
    
    // Response handling
    static void processReadResponse(uint8_t status, uint8_t* data, uint8_t len);
    static void processWriteResponse(uint8_t status);
    
    // Queue for I2C responses
    static QueueHandle_t getResponseQueue() {
        if (_response_queue == nullptr) {
            _response_queue = xQueueCreate(1, sizeof(Result));
        }
        return _response_queue;
    }
    
    // I2C communication functions
    static void send_i2c_read_request(uint8_t address, uint8_t num_bytes);
    static void send_i2c_write_request(uint8_t address, uint8_t* data, uint8_t data_len);
    static void send_i2c_write_read_request(uint8_t address, uint8_t* write_data, uint8_t write_len, uint8_t read_len);
    
private:
    I2CBridge() {} // Private constructor for singleton
    
    // Helper function for checksum calculation
    static uint8_t calculate_checksum(const char* data_str);
    
    static unsigned long _timeout_ms;
    static QueueHandle_t _response_queue;
};

// Function declarations for HAL implementation
extern "C" {
    int i2c_write(uint8_t addr, uint8_t* data, uint8_t len);
    int i2c_read(uint8_t addr, uint8_t* data, uint8_t len);
}
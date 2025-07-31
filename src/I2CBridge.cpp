#include "I2CBridge.h"
#include "NanoCommands.h"
#include "Logger.h"
#include "SerialMutex.h"

#define I2C_BRIDGE_DEBUG // Uncomment for debug output

// Private member variables for the singleton
unsigned long I2CBridge::_timeout_ms = 1000;
QueueHandle_t I2CBridge::_response_queue = nullptr;

// Helper function for checksum calculation
uint8_t I2CBridge::calculate_checksum(const char* data_str) {
    uint8_t crc = 0x00;
    uint8_t polynomial = 0x07;
    size_t length = strlen(data_str);
    for (size_t i = 0; i < length; i++) {
        crc ^= data_str[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (crc << 1) ^ polynomial : (crc << 1);
        }
    }
    return crc;
}

// I2C communication functions
void I2CBridge::send_i2c_read_request(uint8_t address, uint8_t num_bytes) {
    if (num_bytes > 32) {
        logger.warning("I2C read request exceeds maximum buffer size (32 bytes)");
        num_bytes = 32; // Limit to maximum allowed
    }
    
    char data_part[16]; // Buffer for command data
    snprintf(data_part, 16, "%c%02X,%02X", CMD_I2C_READ, address, num_bytes);
    
    uint8_t checksum = calculate_checksum(data_part);
#ifdef I2C_BRIDGE_DEBUG
    logger.debugf("Sending I2C read request: <%s,%d>", data_part, checksum);
#endif
    // Use SerialMutex for atomic packet sending
    SerialMutex& serialMutex = SerialMutex::getInstance();
    if (serialMutex.lock()) {
        // We still need to use direct Serial here for communication with the Nano
        Serial.print('<');
        Serial.print(data_part);
        Serial.print(',');
        Serial.print(checksum);
        Serial.print('>');
        serialMutex.unlock();
    }
}

void I2CBridge::send_i2c_write_request(uint8_t address, uint8_t* data, uint8_t data_len) {
    // 1. Initial check remains the same
    if (data_len > 96) {
        logger.warning("I2C write request exceeds maximum buffer size (96 bytes)");
        data_len = 96;
    }
    
    // 2. Safely construct the data_part string
    char data_part[128];
    size_t buffer_size = sizeof(data_part);
    int written_len = 0;
    int result;

    // Write the initial part of the command (e.g., "W33,0A")
    result = snprintf(data_part, buffer_size, "%c%02X,%02X", CMD_I2C_WRITE, address, data_len);

    // Check if the initial write failed or was truncated
    if (result < 0 || (size_t)result >= buffer_size) {
        logger.errorf("Buffer error during initial formatting of I2C write request.");
        return; // Abort sending the malformed packet
    }
    written_len = result;

    // Append each data byte as hex, safely checking remaining space each time
    for (uint8_t i = 0; i < data_len; i++) {
        size_t remaining_size = buffer_size - written_len;
        result = snprintf(data_part + written_len, remaining_size, ",%02X", data[i]);

        // Check if the append operation failed or was truncated
        if (result < 0 || (size_t)result >= remaining_size) {
            logger.errorf("Buffer overflow while formatting I2C write data. Command truncated.");
            break; // Stop appending data to prevent overflow
        }
        written_len += result;
    }
    
    // 3. The rest of the function proceeds with the safely-built string
    uint8_t checksum = calculate_checksum(data_part);
#ifdef I2C_BRIDGE_DEBUG
    logger.debugf("Sending I2C write request: <%s,%d>", data_part, checksum);
#endif
    
    SerialMutex& serialMutex = SerialMutex::getInstance();
    if (serialMutex.lock()) {
        Serial.print('<');
        Serial.print(data_part);
        Serial.print(',');
        Serial.print(checksum);
        Serial.print('>');
        serialMutex.unlock();
    }
}

void I2CBridge::send_i2c_write_read_request(uint8_t address, uint8_t* write_data, uint8_t write_len, uint8_t read_len) {
    // 1. Initial checks remain the same
    if (write_len > 16) {
        logger.warning("I2C write-read request exceeds maximum write buffer size (16 bytes)");
        write_len = 16;
    }
    if (read_len > 32) {
        logger.warning("I2C write-read request exceeds maximum read buffer size (32 bytes)");
        read_len = 32;
    }
    
    // 2. Safely construct the data_part string
    char data_part[128];
    size_t buffer_size = sizeof(data_part);
    int written_len = 0;
    int result;

    // Write the initial part of the command (e.g., "I33,01,02")
    result = snprintf(data_part, buffer_size, "%c%02X,%02X,%02X", CMD_I2C_READ, address, write_len, read_len);

    // Check if the initial write failed or was truncated
    if (result < 0 || (size_t)result >= buffer_size) {
        logger.errorf("Buffer error during initial formatting of I2C request.");
        return; // Abort sending the malformed packet
    }
    written_len = result;

    // Append each write byte as hex, safely checking remaining space each time
    for (uint8_t i = 0; i < write_len; i++) {
        size_t remaining_size = buffer_size - written_len;
        result = snprintf(data_part + written_len, remaining_size, ",%02X", write_data[i]);

        // Check if the append operation failed or was truncated
        if (result < 0 || (size_t)result >= remaining_size) {
            logger.errorf("Buffer overflow while formatting I2C write data. Command truncated.");
            break; // Stop appending data to prevent overflow
        }
        written_len += result;
    }
    
    // 3. The rest of the function proceeds with the safely-built string
    uint8_t checksum = calculate_checksum(data_part);
#ifdef I2C_BRIDGE_DEBUG
    logger.debugf("Sending I2C write-read request: <%s,%d>", data_part, checksum);
#endif
    
    SerialMutex& serialMutex = SerialMutex::getInstance();
    if (serialMutex.lock()) {
        Serial.print('<');
        Serial.print(data_part);
        Serial.print(',');
        Serial.print(checksum);
        Serial.print('>');
        serialMutex.unlock();
    }
}

bool I2CBridge::begin() {
    // Initialize the response queue
    if (_response_queue == nullptr) {
        _response_queue = xQueueCreate(1, sizeof(Result));
        if (_response_queue == nullptr) {
            logger.error("Failed to create I2C response queue");
            return false;
        }
    }
    return true;
}

I2CBridge::Result I2CBridge::readBytes(uint8_t addr, uint8_t len) {
    Result result = {false, I2C_ERROR_PENDING, {0}, 0};
    
    // Make sure the queue is empty before sending a new request
    xQueueReset(_response_queue);
    
    // Send the read request to the Nano
    send_i2c_read_request(addr, len);
#ifdef I2C_BRIDGE_DEBUG
    logger.debugf("I2CBridge: Waiting for read response (addr=0x%02X, len=%d)", addr, len);
#endif
    
    // Wait for response with timeout
    if (xQueueReceive(_response_queue, &result, pdMS_TO_TICKS(300)) == pdTRUE) {
        // Log the received data
        if (result.data_len > 0) {
            String hexData;
            for (int i = 0; i < result.data_len; i++) {
                if (i > 0) hexData += " ";
                char hex[4];
                sprintf(hex, "%02X", result.data[i]);
                hexData += hex;
            }
#ifdef I2C_BRIDGE_DEBUG
            logger.debugf("I2CBridge: Read response received - addr=0x%02X, status=%d, data=%s", 
                         addr, result.error_code, hexData.c_str());
#endif
        } 
#ifdef I2C_BRIDGE_DEBUG
        else {
            logger.debugf("I2CBridge: Read response received - addr=0x%02X, status=%d, no data", 
                         addr, result.error_code);
        }
#endif
        return result;
    }
    
    // Timeout occurred
    logger.warningf("I2CBridge: Read response timeout for addr=0x%02X", addr);
    result.error_code = I2C_ERROR_TIMEOUT;
    return result;
}

I2CBridge::Result I2CBridge::writeBytes(uint8_t addr, uint8_t* data, uint8_t len) {
    Result result = {false, I2C_ERROR_PENDING, {0}, 0};
    
    // Make sure the queue is empty before sending a new request
    xQueueReset(_response_queue);
    
    // Send the write request to the Nano
    send_i2c_write_request(addr, data, len);
    
    // Log the data being sent
    String hexData;
    for (int i = 0; i < len; i++) {
        if (i > 0) hexData += " ";
        char hex[4];
        sprintf(hex, "%02X", data[i]);
        hexData += hex;
    }
#ifdef I2C_BRIDGE_DEBUG
    logger.debugf("I2CBridge: Waiting for write response (addr=0x%02X, data=%s)", addr, hexData.c_str());
#endif
    // Wait for response with timeout
    if (xQueueReceive(_response_queue, &result, pdMS_TO_TICKS(300)) == pdTRUE) {
#ifdef I2C_BRIDGE_DEBUG
        logger.debugf("I2CBridge: Write response received - addr=0x%02X, status=%d", addr, result.error_code);
#endif
        return result;
    }
    
    // Timeout occurred
    logger.warningf("I2CBridge: Write response timeout for addr=0x%02X", addr);
    result.error_code = I2C_ERROR_TIMEOUT;
    return result;
}

I2CBridge::Result I2CBridge::writeReadBytes(uint8_t addr, uint8_t* writeData, uint8_t writeLen, uint8_t readLen) {
    Result result = {false, I2C_ERROR_PENDING, {0}, 0};
    
    // Make sure the queue is empty before sending a new request
    xQueueReset(_response_queue);
    
    // Log the write data
    String writeHexData;
    for (int i = 0; i < writeLen; i++) {
        if (i > 0) writeHexData += " ";
        char hex[4];
        sprintf(hex, "%02X", writeData[i]);
        writeHexData += hex;
    }
#ifdef I2C_BRIDGE_DEBUG
    logger.debugf("I2CBridge: Sending write-read request (addr=0x%02X, writeData=%s, readLen=%d)", 
                 addr, writeHexData.c_str(), readLen);
#endif
    // Send the write-read request to the Nano
    send_i2c_write_read_request(addr, writeData, writeLen, readLen);
    
    // Wait for response with timeout
    if (xQueueReceive(_response_queue, &result, pdMS_TO_TICKS(300)) == pdTRUE) {
        // Log the received data
        if (result.data_len > 0) {
            String readHexData;
            for (int i = 0; i < result.data_len; i++) {
                if (i > 0) readHexData += " ";
                char hex[4];
                sprintf(hex, "%02X", result.data[i]);
                readHexData += hex;
            }
#ifdef I2C_BRIDGE_DEBUG
            logger.debugf("I2CBridge: Write-read response received - addr=0x%02X, status=%d, data=%s", 
                         addr, result.error_code, readHexData.c_str());
#endif
        } 
#ifdef I2C_BRIDGE_DEBUG
        else if (result.error_code == I2C_ERROR_NONE) {
            logger.debugf("I2CBridge: Write-read response received - addr=0x%02X, status=%d, no data", 
                         addr, result.error_code);
        } else {
            logger.debugf("I2CBridge: Write-read response received - addr=0x%02X, status=%d, no data", 
                         addr, result.error_code);
        }
#endif
        return result;
    }
    
    // Timeout occurred
    logger.warningf("I2CBridge: Write-read response timeout for addr=0x%02X, reg=0x%02X", 
                  addr, writeLen > 0 ? writeData[0] : 0);
    result.error_code = I2C_ERROR_TIMEOUT;
    return result;
}

void I2CBridge::processReadResponse(uint8_t status, uint8_t* data, uint8_t len) {
    Result result;
    result.success = (status == I2C_ERROR_NONE);
    result.error_code = status;
    result.data_len = len;
    
    if (len > 0 && len <= sizeof(result.data)) {
        memcpy(result.data, data, len);
    } else if (len > sizeof(result.data)) {
        // Truncate if data is too large
        memcpy(result.data, data, sizeof(result.data));
        result.data_len = sizeof(result.data);
        logger.warningf("I2CBridge: Truncated read response data from %d to %d bytes", len, result.data_len);
    }
    
    // Log the data being sent to the queue
    if (result.data_len > 0) {
        String hexData;
        for (int i = 0; i < result.data_len; i++) {
            if (i > 0) hexData += " ";
            char hex[4];
            sprintf(hex, "%02X", result.data[i]);
            hexData += hex;
        }
#ifdef I2C_BRIDGE_DEBUG
        logger.debugf("I2CBridge: Sending to queue - status: %d, data: %s", status, hexData.c_str());
#endif
    }
#ifdef I2C_BRIDGE_DEBUG
    else {
        logger.debugf("I2CBridge: Sending to queue - status: %d, no data", status);
    }
#endif
    
    // Log I2C read response received from Nano
    if (status != I2C_ERROR_NONE) {
        logger.warningf("ESP32: Received I2C read error from Nano - status: %d, data_len: %d", status, len);
    }
    
    // Send to queue for any waiting tasks
    BaseType_t result_sent = xQueueSend(_response_queue, &result, 0);
    if (result_sent != pdTRUE) {
        logger.warning("I2CBridge: Failed to send response to queue");
    }
}

void I2CBridge::processWriteResponse(uint8_t status) {
    Result result;
    result.success = (status == I2C_ERROR_NONE);
    result.error_code = status;
    result.data_len = 0;
    
    // Clear all data bytes to ensure clean state
    memset(result.data, 0, sizeof(result.data));
#ifdef I2C_BRIDGE_DEBUG
    logger.debugf("I2CBridge: Sending write response to queue - status: %d", status);
#endif
    
    // Log I2C write response received from Nano
    if (status != I2C_ERROR_NONE) {
        logger.warningf("ESP32: Received I2C write error from Nano - status: %d", status);
    }
    
    // Send to queue for any waiting tasks
    BaseType_t result_sent = xQueueSend(_response_queue, &result, 0);
    if (result_sent != pdTRUE) {
        logger.warning("I2CBridge: Failed to send write response to queue");
    }
}

// C-style functions for the HAL implementation
extern "C" {
    int i2c_write(uint8_t addr, uint8_t* data, uint8_t len) {
        I2CBridge::Result result = I2CBridge::getInstance().writeBytes(addr, data, len);
        return result.success ? 0 : result.error_code;
    }
    
    int i2c_read(uint8_t addr, uint8_t* data, uint8_t len) {
        I2CBridge::Result result = I2CBridge::getInstance().readBytes(addr, len);
        
        if (result.success && result.data_len > 0) {
            memcpy(data, result.data, result.data_len);
            return 0;
        }
        
        return result.error_code;
    }
}
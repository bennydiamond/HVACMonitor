#include "I2CBridge.h"
#include "NanoCommands.h"
#include "Logger.h"
#include "SerialMutex.h"
#ifdef ENABLE_TASK_WATCHDOG
#include <esp_task_wdt.h>
#endif

//#define I2C_BRIDGE_DEBUG // Uncomment for debug output

// Private member variables for the singleton
unsigned long I2CBridge::_timeout_ms = 1000;
QueueHandle_t I2CBridge::_response_queue = nullptr;
uint8_t I2CBridge::_next_seq = 0;

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

uint8_t I2CBridge::beginRequest() {
    // Drop replies left over from earlier requests
    xQueueReset(_response_queue);
    return _next_seq++;
}

bool I2CBridge::waitForResponse(uint8_t seq, Result& result, uint32_t timeout_ms) {
#ifdef ENABLE_TASK_WATCHDOG
    // An operation retries up to 3 waits and a sensor pass chains several operations:
    // feed the caller's watchdog before each wait, so slow replies can't add up past
    // its timeout. Silently skipped for a task the watchdog doesn't watch.
    if (esp_task_wdt_status(NULL) == ESP_OK) {
        esp_task_wdt_reset();
    }
#endif
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    TickType_t elapsed;
    while ((elapsed = xTaskGetTickCount() - start) < timeout) {
        if (xQueueReceive(_response_queue, &result, timeout - elapsed) != pdTRUE) {
            return false;
        }
        if (result.seq == seq) {
            return true;
        }
        logger.warningf("I2CBridge: Dropped stale I2C response (seq %02X, expected %02X)", result.seq, seq);
    }
    return false;
}

// I2C communication functions
void I2CBridge::send_i2c_write_request(uint8_t address, uint8_t* data, uint8_t data_len, uint8_t seq) {
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

    // Append each data byte as hex, then the sequence tag, checking remaining space each time
    for (uint16_t i = 0; i <= data_len; i++) {
        size_t remaining_size = buffer_size - written_len;
        result = snprintf(data_part + written_len, remaining_size, ",%02X", i < data_len ? data[i] : seq);

        // A truncated command would be parsed as a different request: don't send it
        if (result < 0 || (size_t)result >= remaining_size) {
            logger.errorf("Buffer overflow while formatting I2C write data. Command not sent.");
            return;
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

void I2CBridge::send_i2c_write_read_request(uint8_t address, uint8_t* write_data, uint8_t write_len, uint8_t read_len, uint8_t seq) {
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

    // Append each write byte as hex, then the sequence tag, checking remaining space each time
    for (uint16_t i = 0; i <= write_len; i++) {
        size_t remaining_size = buffer_size - written_len;
        result = snprintf(data_part + written_len, remaining_size, ",%02X", i < write_len ? write_data[i] : seq);

        // A truncated command would be parsed as a different request: don't send it
        if (result < 0 || (size_t)result >= remaining_size) {
            logger.errorf("Buffer overflow while formatting I2C write data. Command not sent.");
            return;
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
        _response_queue = xQueueCreate(RESPONSE_QUEUE_LENGTH, sizeof(Result));
        if (_response_queue == nullptr) {
            logger.error("Failed to create I2C response queue");
            return false;
        }
    }
    return true;
}

I2CBridge::Result I2CBridge::readBytes(uint8_t addr, uint8_t len) {
    Result result = {false, I2C_ERROR_PENDING, {0}, 0};
    const int max_retries = 3;
    int attempt = 0;
    bool got_response = false;

    while (attempt < max_retries) {
        // Each attempt gets its own tag, so a late reply to a previous attempt is dropped
        uint8_t seq = beginRequest();

        // Send the read request to the Nano (write-read with nothing to write)
        send_i2c_write_read_request(addr, nullptr, 0, len, seq);
#ifdef I2C_BRIDGE_DEBUG
        logger.debugf("I2CBridge: Waiting for read response (addr=0x%02X, len=%d, attempt=%d)", addr, len, attempt + 1);
#endif
        
        // Wait for response with timeout
        if (waitForResponse(seq, result, 1000)) {
#ifdef I2C_BRIDGE_DEBUG
            // Log the received data
            if (result.data_len > 0) {
                char hexData[100];
                size_t offset = 0;
                for (int i = 0; i < result.data_len && offset + 4 < sizeof(hexData); i++) {
                    offset += snprintf(hexData + offset, sizeof(hexData) - offset, (i == 0) ? "%02X" : " %02X", result.data[i]);
                }
                logger.debugf("I2CBridge: Read response received - addr=0x%02X, status=%d, data=%s, attempt=%d", 
                             addr, result.error_code, hexData, attempt + 1);
            } else {
                logger.debugf("I2CBridge: Read response received - addr=0x%02X, status=%d, no data, attempt=%d", 
                             addr, result.error_code, attempt + 1);
            }
#endif
            if (result.success) {
                return result;
            }
            got_response = true;
        } else {
            got_response = false;
        }

        // Timeout or error occurred
        attempt++;
        if (attempt < max_retries) {
            logger.warningf("I2CBridge: Read attempt %d failed for addr=0x%02X, retrying...", attempt, addr);
            delay(1); // Small delay before retry
        }
    }

    // All retries failed: keep the SensorStack's error code when the last attempt got one
    if (got_response) {
        logger.warningf("I2CBridge: Read failed after %d attempts for addr=0x%02X, status=%d", max_retries, addr, result.error_code);
    } else {
        logger.warningf("I2CBridge: Read response timeout after %d attempts for addr=0x%02X", max_retries, addr);
        result.error_code = I2C_ERROR_TIMEOUT;
    }
    return result;
}

I2CBridge::Result I2CBridge::writeBytes(uint8_t addr, uint8_t* data, uint8_t len) {
    Result result = {false, I2C_ERROR_PENDING, {0}, 0};
    const int max_retries = 3;
    int attempt = 0;
    bool got_response = false;

    while (attempt < max_retries) {
        // Each attempt gets its own tag, so a late reply to a previous attempt is dropped
        uint8_t seq = beginRequest();

        // Send the write request to the Nano
        send_i2c_write_request(addr, data, len, seq);

#ifdef I2C_BRIDGE_DEBUG
        // Log the data being sent
        char hexData[100];
        size_t offset = 0;
        for (int i = 0; i < len && offset + 4 < sizeof(hexData); i++) {
            offset += snprintf(hexData + offset, sizeof(hexData) - offset, (i == 0) ? "%02X" : " %02X", data[i]);
        }
        logger.debugf("I2CBridge: Waiting for write response (addr=0x%02X, data=%s, attempt=%d)", addr, hexData, attempt + 1);
#endif
        // Wait for response with timeout
        if (waitForResponse(seq, result, 1000)) {
#ifdef I2C_BRIDGE_DEBUG
            logger.debugf("I2CBridge: Write response received - addr=0x%02X, status=%d, attempt=%d", addr, result.error_code, attempt + 1);
#endif
            if (result.success) {
                return result;
            }
            got_response = true;
        } else {
            got_response = false;
        }

        // Timeout or error occurred
        attempt++;
        if (attempt < max_retries) {
            logger.warningf("I2CBridge: Write attempt %d failed for addr=0x%02X, retrying...", attempt, addr);
            delay(1); // Small delay before retry
        }
    }

    // All retries failed: keep the SensorStack's error code when the last attempt got one
    if (got_response) {
        logger.warningf("I2CBridge: Write failed after %d attempts for addr=0x%02X, status=%d", max_retries, addr, result.error_code);
    } else {
        logger.warningf("I2CBridge: Write response timeout after %d attempts for addr=0x%02X", max_retries, addr);
        result.error_code = I2C_ERROR_TIMEOUT;
    }
    return result;
}

I2CBridge::Result I2CBridge::writeReadBytes(uint8_t addr, uint8_t* writeData, uint8_t writeLen, uint8_t readLen) {
    Result result = {false, I2C_ERROR_PENDING, {0}, 0};

    uint8_t seq = beginRequest();

#ifdef I2C_BRIDGE_DEBUG
    // Log the write data
    char writeHexData[100];
    size_t writeOffset = 0;
    for (int i = 0; i < writeLen && writeOffset + 4 < sizeof(writeHexData); i++) {
        writeOffset += snprintf(writeHexData + writeOffset, sizeof(writeHexData) - writeOffset, (i == 0) ? "%02X" : " %02X", writeData[i]);
    }
    logger.debugf("I2CBridge: Sending write-read request (addr=0x%02X, writeData=%s, readLen=%d)", 
                 addr, writeHexData, readLen);
#endif
    // Send the write-read request to the Nano
    send_i2c_write_read_request(addr, writeData, writeLen, readLen, seq);

    // Wait for response with timeout
#ifdef I2C_BRIDGE_DEBUG
    uint32_t start_time = millis();
#endif
    if (waitForResponse(seq, result, 1000)) {
#ifdef I2C_BRIDGE_DEBUG
        // Log the received data
        if (result.data_len > 0) {
            char readHexData[100];
            size_t readOffset = 0;
            for (int i = 0; i < result.data_len && readOffset + 4 < sizeof(readHexData); i++) {
                readOffset += snprintf(readHexData + readOffset, sizeof(readHexData) - readOffset, (i == 0) ? "%02X" : " %02X", result.data[i]);
            }
            logger.debugf("I2CBridge: Write-read response received (%u ms round trip) - addr=0x%02X, status=%d, data=%s", 
                         millis() - start_time, addr, result.error_code, readHexData);
        } else if (result.error_code == I2C_ERROR_NONE) {
            logger.debugf("I2CBridge: Write-read response received (%u ms round trip) - addr=0x%02X, status=%d, no data", 
                         millis() - start_time, addr, result.error_code);
        } else {
            logger.debugf("I2CBridge: Failed Write-read response received (%u ms round trip) - addr=0x%02X, status=%d, no data", 
                         millis() - start_time, addr, result.error_code);
        }
#endif
        return result;
    }
#ifdef I2C_BRIDGE_DEBUG
    else {
        logger.debugf("I2CBridge: Failed Write-read response timeout (%u ms round trip) for addr=0x%02X, reg=0x%02X",
                  millis() - start_time, addr, writeLen > 0 ? writeData[0] : 0);
    }
#endif

    // Timeout occurred
    logger.warningf("I2CBridge: Write-read response timeout for addr=0x%02X, reg=0x%02X", 
                  addr, writeLen > 0 ? writeData[0] : 0);
    result.error_code = I2C_ERROR_TIMEOUT;
    return result;
}

void I2CBridge::processReadResponse(uint8_t status, uint8_t* data, uint8_t len, uint8_t seq) {
    Result result;
    result.success = (status == I2C_ERROR_NONE);
    result.error_code = status;
    result.data_len = len;
    result.seq = seq;

    if (len > 0 && len <= sizeof(result.data)) {
        memcpy(result.data, data, len);
    } else if (len > sizeof(result.data)) {
        // Truncate if data is too large
        memcpy(result.data, data, sizeof(result.data));
        result.data_len = sizeof(result.data);
        logger.warningf("I2CBridge: Truncated read response data from %d to %d bytes", len, result.data_len);
    }
    
#ifdef I2C_BRIDGE_DEBUG
    // Log the data being sent to the queue
    if (result.data_len > 0) {
        char hexData[100];
        size_t offset = 0;
        for (int i = 0; i < result.data_len && offset + 4 < sizeof(hexData); i++) {
            offset += snprintf(hexData + offset, sizeof(hexData) - offset, (i == 0) ? "%02X" : " %02X", result.data[i]);
        }
        logger.debugf("I2CBridge: Sending to queue - status: %d, data: %s", status, hexData);
    } else {
        logger.debugf("I2CBridge: Sending to queue - status: %d, no data", status);
    }
#endif
    
    // Log I2C read response received from Nano
    if (status != I2C_ERROR_NONE) {
        logger.warningf("ESP32: Received I2C read error from Nano - status: %d, data_len: %d", status, len);
    }

    // Send to queue for the waiting task
    if (xQueueSend(_response_queue, &result, 0) != pdTRUE) {
        logger.warning("I2CBridge: Failed to send response to queue");
    }
}

void I2CBridge::processWriteResponse(uint8_t status, uint8_t seq) {
    Result result;
    result.success = (status == I2C_ERROR_NONE);
    result.error_code = status;
    result.data_len = 0;
    result.seq = seq;

    // Clear all data bytes to ensure clean state
    memset(result.data, 0, sizeof(result.data));
#ifdef I2C_BRIDGE_DEBUG
    logger.debugf("I2CBridge: Sending write response to queue - status: %d", status);
#endif
    
    // Log I2C write response received from Nano
    if (status != I2C_ERROR_NONE) {
        logger.warningf("ESP32: Received I2C write error from Nano - status: %d", status);
    }

    // Send to queue for the waiting task
    if (xQueueSend(_response_queue, &result, 0) != pdTRUE) {
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
        
        if (result.success && result.data_len == len) {
            memcpy(data, result.data, len);
            return 0;
        }

        return result.success ? I2C_ERROR_DATA_NACK : result.error_code;
    }
}
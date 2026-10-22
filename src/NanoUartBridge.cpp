#include "NanoUartBridge.h"
#include <string.h>
#include <stdlib.h>

#include "Logger.h"
#include "SerialMutex.h"
#include "SensorStackStatusManager.h"
#include "SensorDataProcessor.h"
#include "SensorTask.h"
#include "I2CBridge.h"
#ifdef ENABLE_TASK_WATCHDOG
#include "WatchdogManager.h"
#endif

static const int NANO_PACKET_TIMEOUT_MS = 7000;
static const int HEALTH_CHECK_INTERVAL_MS = 5000;
static const int HEALTH_RESPONSE_TIMEOUT_MS = 500;
static const int SCD30_INFO_INTERVAL_MS = 20000;
static const int SENSOR_QUERY_INTERVAL_MS = 2000;
static const int INIT_COMMAND_TIMEOUT_MS = 100;

NanoUartBridge* NanoUartBridge::_instance = nullptr;

static const NanoUartBridge::InitCmd s_init_cmds[] = {
    NanoUartBridge::CMD_INIT_VERSION,
    NanoUartBridge::CMD_INIT_HEALTH,
    NanoUartBridge::CMD_INIT_SPS30_INFO,
    NanoUartBridge::CMD_INIT_SCD30_INFO,
    NanoUartBridge::CMD_INIT_NONE
};

NanoUartBridge::NanoUartBridge()
    : _statusManager(nullptr),
      _dataProcessor(nullptr),
      _sensorTask(nullptr),
      _serial_rx_len(0),
      _valid_packet_start_received(false),
      _sps30_info_response_received(false),
      _sgp41_test_response_received(false),
      _current_init_command_index(0),
      _init_sequence_active(true),
      _last_init_command_time(0),
      _last_health_check_time(0),
      _last_health_request_time(0),
      _health_request_pending(false),
      _last_sensor_query_time(0),
      _last_scd30_info_time(0) {
    _serial_rx_buffer[0] = '\0';
    _instance = this;
}

NanoUartBridge::~NanoUartBridge() {
    if (_instance == this) {
        _instance = nullptr;
    }
}

void NanoUartBridge::begin(long baud_rate,
                           SensorStackStatusManager* statusManager,
                           SensorDataProcessor* dataProcessor,
                           SensorTask* sensorTask) {
    _statusManager = statusManager;
    _dataProcessor = dataProcessor;
    _sensorTask = sensorTask;

    Serial.begin(baud_rate);
    _init_sequence_active = true;
    _current_init_command_index = 0;
    _last_init_command_time = 0;
    _last_health_check_time = 0;
    _last_sensor_query_time = 0;
    _last_scd30_info_time = 0;
    logger.info("NanoUartBridge initialized. Starting init sequence with Sensor Stack...");
}

uint8_t NanoUartBridge::calculateChecksum(const char* data_str) {
    if (!data_str) return 0;
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

void NanoUartBridge::sendCommandStr(const char* data_str) {
    if (!data_str) return;
    uint8_t checksum = calculateChecksum(data_str);
#ifdef SERIAL_PACKET_DEBUG
    logger.debugf("Sending command string to Nano: <%s,%d>", data_str, checksum);
#endif

    SerialMutex &serialMutex = SerialMutex::getInstance();
    if (serialMutex.lock()) {
        Serial.print('<');
        Serial.print(data_str);
        Serial.print(',');
        Serial.print(checksum);
        Serial.print('>');
        serialMutex.unlock();
    }
}

void NanoUartBridge::sendCommand(char cmd) {
    char data_part[2] = {cmd, '\0'};
    uint8_t checksum = calculateChecksum(data_part);
#ifdef SERIAL_PACKET_DEBUG
    logger.debugf("Sending command to Nano: <%c,%d>", cmd, checksum);
#endif

    SerialMutex &serialMutex = SerialMutex::getInstance();
    if (serialMutex.lock()) {
        Serial.print('<');
        Serial.print(cmd);
        Serial.print(',');
        Serial.print(checksum);
        Serial.print('>');
        serialMutex.unlock();
    }

    if (cmd == CMD_REBOOT) {
        logger.info("Reboot command sent. Marking sensor stack as disconnected.");
        if (_statusManager) {
            _statusManager->setConnected(false);
        }
        if (_sensorTask) {
            _sensorTask->notifyConnectionStatus(false, false);
        }
    }
}

void NanoUartBridge::sendCommandWithParam(char cmd, char param) {
    char data_part[3] = {cmd, param, '\0'};
    uint8_t checksum = calculateChecksum(data_part);
#ifdef SERIAL_PACKET_DEBUG
    logger.debugf("Sending command to Nano: <%c%c,%d>", cmd, param, checksum);
#endif

    SerialMutex &serialMutex = SerialMutex::getInstance();
    if (serialMutex.lock()) {
        Serial.print('<');
        Serial.print(cmd);
        Serial.print(param);
        Serial.print(',');
        Serial.print(checksum);
        Serial.print('>');
        serialMutex.unlock();
    }
}

bool NanoUartBridge::strEndsWith(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t len_str = strlen(str);
    size_t len_suf = strlen(suffix);
    if (len_suf > len_str) return false;
    return strcmp(str + len_str - len_suf, suffix) == 0;
}

bool NanoUartBridge::querySps30InfoBlocking(uint32_t timeout_ms) {
    _sps30_info_response_received = false;
    sendCommand(CMD_GET_SPS30_INFO);
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
#ifdef ENABLE_TASK_WATCHDOG
        WatchdogManager::reset();
#endif
        readAndProcessSerial();
        if (_sps30_info_response_received) return true;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

bool NanoUartBridge::runSgp41TestBlocking(uint32_t timeout_ms) {
    _sgp41_test_response_received = false;
    sendCommand(CMD_SGP41_TEST);
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
#ifdef ENABLE_TASK_WATCHDOG
        WatchdogManager::reset();
#endif
        readAndProcessSerial();
        if (_sgp41_test_response_received) return true;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

void NanoUartBridge::readAndProcessSerial() {
    while (Serial.available() > 0) {
        char incoming_char = Serial.read();

        if (incoming_char == '\r' || incoming_char == '\n') continue;

        if (!_valid_packet_start_received && incoming_char == '<') {
            _serial_rx_len = 0;
            _valid_packet_start_received = true;
        } else if (_valid_packet_start_received && incoming_char == '>') {
            if (_serial_rx_len > 0) {
                _serial_rx_buffer[_serial_rx_len] = '\0';
                processPacket(_serial_rx_buffer);
            }
            _serial_rx_len = 0;
            _valid_packet_start_received = false;
            break;
        } else if (_valid_packet_start_received &&
                   _serial_rx_len < (sizeof(_serial_rx_buffer) - 1)) {
            _serial_rx_buffer[_serial_rx_len++] = incoming_char;
        } else {
            logger.warningf("Invalid character received or buffer overflow: %c", incoming_char);
            _serial_rx_len = 0;
            _valid_packet_start_received = false;
        }
    }
}

void NanoUartBridge::processPacket(char* packet) {
    if (!packet) return;

    while (*packet == ' ' || *packet == '\t') packet++;
    if (*packet == '<') packet++;

    size_t len = strlen(packet);
    while (len > 0 && (packet[len - 1] == ' ' || packet[len - 1] == '\t' ||
                       packet[len - 1] == '\r' || packet[len - 1] == '\n' ||
                       packet[len - 1] == '>')) {
        packet[--len] = '\0';
    }

    char *last_comma = strrchr(packet, ',');
    if (!last_comma) {
        logger.warningf("Malformed packet (no checksum comma): %s", packet);
        return;
    }

    *last_comma = '\0';
    char *data_part = packet;
    uint8_t received_checksum = static_cast<uint8_t>(strtoul(last_comma + 1, nullptr, 10));
    if (calculateChecksum(data_part) != received_checksum) {
        logger.warningf("Checksum mismatch! Data: %s, Rcvd: %u", data_part, received_checksum);
        return;
    }

#ifdef SERIAL_PACKET_DEBUG
    logger.debugf("ESP32: Received packet from Nano: <%s,%u>", data_part, received_checksum);
#endif

    // Special handling for I2C recovery events
    if (strncmp(data_part, "E,I2C_RECOVER", 13) == 0) {
        if (strEndsWith(data_part, ",TIMEOUT")) {
            logger.error("SensorStack: I2C_RECOVER: I2C recovery timeout, bus may be stuck");
        } else if (strEndsWith(data_part, ",FAIL_SDA_LOW")) {
            logger.error("SensorStack: I2C_RECOVER: I2C recovery failed - SDA line still stuck low");
        } else if (strEndsWith(data_part, ",FAIL_BUS_BUSY")) {
            logger.info("SensorStack: I2C_RECOVER: I2C recovery failed - Manual STOP condition was not successful, bus is still busy");
        } else if (strEndsWith(data_part, ",BUS_STUCK,0")) {
            logger.info("SensorStack: I2C_RECOVER: I2C bus stuck detection was triggered in period loop() check");
        } else if (strEndsWith(data_part, ",BUS_STUCK,1")) {
            logger.error("SensorStack: I2C_RECOVER: I2C bus stuck detection was triggered in read_all_sensors()");
        } else if (strEndsWith(data_part, ",1")) {
            logger.info("SensorStack: I2C_RECOVER: I2C bus recovery started");
        } else if (strEndsWith(data_part, ",2")) {
            logger.error("SensorStack: I2C_RECOVER: I2C bus is stuck, attempting manual recovery");
        } else if (strEndsWith(data_part, ",0")) {
            logger.info("SensorStack: I2C_RECOVER: I2C bus recovery completed");
        } else {
            logger.warningf("I2C_RECOVER: Unknown I2C recovery status: %s", data_part);
        }
    } else if (strncmp(data_part, "E,SPS30_DATA_READY_ERROR", 24) == 0) {
        logger.errorf("SensorStack: Failed! reports SPS30 data ready error");
    } else if (strncmp(data_part, "E,SPS30_MEASUREMENT_ERROR", 25) == 0) {
        logger.errorf("SensorStack: Failed! reports SPS30 measurement error");
    } else if (strncmp(data_part, "E,SCD30_DATA_READY_ERROR", 24) == 0) {
        logger.errorf("SensorStack: Failed! reports SCD30 data ready error");
    } else if (strncmp(data_part, "E,SCD30_MEASUREMENT_ERROR", 25) == 0) {
        logger.errorf("SensorStack: Failed! reports SCD30 measurement error");
    } else if (strncmp(data_part, "E,SGP41_MEASUREMENT_ERROR", 25) == 0) {
        logger.errorf("SensorStack: Failed! reports SGP41 measurement error");
    }

    if (_statusManager) {
        _statusManager->onPacketReceived();
    }

    char cmd = data_part[0];
    char *payload = data_part + 1;

    switch (cmd) {
    case RSP_SENSORS: {
        if (_dataProcessor) {
            _dataProcessor->processPayload(payload);
        }
        break;
    }
    case RSP_VERSION: {
        if (_statusManager) {
            _statusManager->setVersion(payload);
        }
        if (_init_sequence_active &&
            s_init_cmds[_current_init_command_index] == CMD_INIT_VERSION) {
            _current_init_command_index++;
        }
        break;
    }
    case RSP_HEALTH: {
        int first_time_flag = -1;
        if (_statusManager && _statusManager->processHealthPayload(payload, &first_time_flag)) {
            if (first_time_flag == 0) {
                sendCommand(CMD_ACK_HEALTH);
                _init_sequence_active = true;
                _current_init_command_index = 0;
                _last_init_command_time = 0;
            }
        }

        if (_health_request_pending) {
            _health_request_pending = false;
        }
        if (_init_sequence_active &&
            s_init_cmds[_current_init_command_index] == CMD_INIT_HEALTH) {
            _current_init_command_index++;
        }
        break;
    }
    case RSP_SPS30_INFO: {
        if (_statusManager) {
            _statusManager->processSps30InfoPayload(payload);
        }
        _sps30_info_response_received = true;
        if (_init_sequence_active &&
            s_init_cmds[_current_init_command_index] == CMD_INIT_SPS30_INFO) {
            _current_init_command_index++;
        }
        break;
    }
    case RSP_SPS30_CLEAN: {
        if (_statusManager) {
            _statusManager->processSps30CleanPayload(payload);
        }
        break;
    }
    case RSP_SGP41_TEST: {
        if (_statusManager) {
            _statusManager->processSgp41TestPayload(payload);
        }
        _sgp41_test_response_received = true;
        break;
    }
    case RSP_SCD30_INFO: {
        if (_statusManager) {
            _statusManager->processScd30InfoPayload(payload);
        }
        if (_init_sequence_active &&
            s_init_cmds[_current_init_command_index] == CMD_INIT_SCD30_INFO) {
            _current_init_command_index++;
        }
        break;
    }
    case RSP_SCD30_AUTOCAL: {
        if (_statusManager) {
            _statusManager->processScd30AutoCalPayload(payload);
        }
        break;
    }
    case RSP_SCD30_FORCECAL: {
        if (_statusManager) {
            _statusManager->processScd30ForceCalPayload(payload);
        }
        break;
    }
    case RSP_I2C_READ: {
        char *token = strtok(payload, ",");
        if (!token) return;
        uint8_t status = strtol(token, nullptr, 16);
        token = strtok(NULL, ",");
        if (!token) return;
        uint8_t num_bytes = strtol(token, nullptr, 16);

        uint8_t data[32];
        uint8_t bytes_read = 0;
        while ((token = strtok(NULL, ",")) != NULL && bytes_read < num_bytes) {
            data[bytes_read++] = strtol(token, nullptr, 16);
        }
        if (bytes_read != num_bytes) {
            logger.warningf("I2C read: Expected %d bytes, got %d", num_bytes, bytes_read);
        }
        if (status != I2C_ERROR_NONE) {
            logger.warningf("I2C read error: 0x%02X", status);
        }
        I2CBridge::getInstance().processReadResponse(status, data, bytes_read);
        break;
    }
    case RSP_I2C_WRITE: {
        char *token = strtok(payload, ",");
        if (!token) return;
        uint8_t status = strtol(token, nullptr, 16);
        if (status != I2C_ERROR_NONE) {
            logger.warningf("I2C write error: 0x%02X", status);
        }
        I2CBridge::getInstance().processWriteResponse(status);
        break;
    }
    default:
        logger.warningf("Unknown response command from Nano: %c", cmd);
        break;
    }
}

void NanoUartBridge::handle() {
    bool serial_command_sent = false;

    readAndProcessSerial();

    bool is_conn = _statusManager ? _statusManager->isConnected() : false;
    unsigned long last_pkt_time = _statusManager ? _statusManager->getLastPacketTime() : 0;

    // Packet timeout check
    if (millis() - last_pkt_time > NANO_PACKET_TIMEOUT_MS) {
        if (is_conn) {
            if (_statusManager) {
                _statusManager->setConnected(false);
            }
            _init_sequence_active = true;
            _current_init_command_index = 0;
            if (_sensorTask) {
                _sensorTask->notifyConnectionStatus(false, false);
            }
        }
    }

    // Periodic health check
    if (!serial_command_sent && is_conn &&
        (millis() - _last_health_check_time > HEALTH_CHECK_INTERVAL_MS)) {
        _last_health_check_time = millis();
        sendCommand(CMD_GET_HEALTH);
        _last_health_request_time = millis();
        _health_request_pending = true;
        serial_command_sent = true;
    }

    if (_health_request_pending &&
        (millis() - _last_health_request_time > HEALTH_RESPONSE_TIMEOUT_MS)) {
        logger.warning("No response to CMD_GET_HEALTH from Sensor Stack within 500ms.");
        _health_request_pending = false;
    }

    // Init sequence
    if (_init_sequence_active) {
        if (s_init_cmds[_current_init_command_index] == CMD_INIT_NONE) {
            _init_sequence_active = false;
            logger.info("Init sequence completed successfully.");
            if (_sensorTask) {
                _sensorTask->notifyConnectionStatus(is_conn, true);
            }
        } else if ((millis() - _last_init_command_time > INIT_COMMAND_TIMEOUT_MS) ||
                   (_last_init_command_time == 0)) {
            char cmd_to_send = 0;
            switch (s_init_cmds[_current_init_command_index]) {
                case CMD_INIT_VERSION: cmd_to_send = CMD_GET_VERSION; break;
                case CMD_INIT_HEALTH: cmd_to_send = CMD_GET_HEALTH; break;
                case CMD_INIT_SPS30_INFO: cmd_to_send = CMD_GET_SPS30_INFO; break;
                case CMD_INIT_SCD30_INFO: cmd_to_send = CMD_GET_SCD30_INFO; break;
                default: break;
            }
            if (cmd_to_send) {
                logger.infof("Sending init command %d/%d: %c",
                             _current_init_command_index + 1, 4, cmd_to_send);
                sendCommand(cmd_to_send);
                _last_init_command_time = millis();
                serial_command_sent = true;
            }
        }
    }

    // Periodic sensor query every 2 seconds
    if (!serial_command_sent && is_conn && !_init_sequence_active &&
        (millis() - _last_sensor_query_time > SENSOR_QUERY_INTERVAL_MS)) {
        _last_sensor_query_time = millis();
        sendCommand(CMD_GET_SENSORS);
        serial_command_sent = true;
    }

    // Periodic SCD30 info query
    if (!serial_command_sent && is_conn &&
        (millis() - _last_scd30_info_time > SCD30_INFO_INTERVAL_MS)) {
        _last_scd30_info_time = millis();
        sendCommand(CMD_GET_SCD30_INFO);
        serial_command_sent = true;
    }
}

// Global C wrappers for compatibility
void send_command_to_nano(char cmd) {
    if (NanoUartBridge::getInstance()) {
        NanoUartBridge::getInstance()->sendCommand(cmd);
    }
}

void send_command_to_nano_with_param(char cmd, char param) {
    if (NanoUartBridge::getInstance()) {
        NanoUartBridge::getInstance()->sendCommandWithParam(cmd, param);
    }
}

void send_command_to_nano_str(const char *data_str) {
    if (NanoUartBridge::getInstance()) {
        NanoUartBridge::getInstance()->sendCommandStr(data_str);
    }
}

bool query_sps30_info_blocking(uint32_t timeout_ms) {
    return NanoUartBridge::getInstance() ? NanoUartBridge::getInstance()->querySps30InfoBlocking(timeout_ms) : false;
}

bool run_sgp41_test_blocking(uint32_t timeout_ms) {
    return NanoUartBridge::getInstance() ? NanoUartBridge::getInstance()->runSgp41TestBlocking(timeout_ms) : false;
}

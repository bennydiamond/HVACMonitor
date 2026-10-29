#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include "NanoCommands.h"

// Forward declarations of collaborator classes
class SensorStackStatusManager;
class SensorDataProcessor;
class SensorTask;

class NanoUartBridge {
public:
    NanoUartBridge();
    ~NanoUartBridge();

    static NanoUartBridge* getInstance() { return _instance; }

    // Lifecycle
    void begin(long baud_rate,
               SensorStackStatusManager* statusManager,
               SensorDataProcessor* dataProcessor,
               SensorTask* sensorTask = nullptr);

    void setSensorTask(SensorTask* sensorTask) { _sensorTask = sensorTask; }
    SensorStackStatusManager* getStatusManager() const { return _statusManager; }

    void handle();
    void loop() { handle(); }

    // Command transmission
    void sendCommand(char cmd);
    void sendCommandWithParam(char cmd, char param);
    void sendCommandStr(const char* data_str);

    // Flashing & Maintenance Control
    void setSuspended(bool suspended);
    bool isSuspended() const { return _suspended; }
    void restartInitSequence();

    static uint8_t calculateChecksum(const char* data_str);
    void readAndProcessSerial();

    enum InitCmd { CMD_INIT_NONE, CMD_INIT_VERSION, CMD_INIT_HEALTH, CMD_INIT_SPS30_INFO, CMD_INIT_SCD30_INFO };

private:
    void processPacket(char* packet);
    static bool strEndsWith(const char* str, const char* suffix);

    static NanoUartBridge* _instance;

    // References to collaborators
    SensorStackStatusManager* _statusManager;
    SensorDataProcessor* _dataProcessor;
    SensorTask* _sensorTask;

    // Serial RX buffer
    char _serial_rx_buffer[256];
    size_t _serial_rx_len;
    bool _valid_packet_start_received;

    // Init sequence & timing state
    int _current_init_command_index;
    bool _init_sequence_active;
    unsigned long _last_init_command_time;
    unsigned long _last_health_check_time;
    unsigned long _last_health_request_time;
    bool _health_request_pending;
    unsigned long _last_sensor_query_time;
    unsigned long _last_scd30_info_time;
    unsigned long _last_firmware_probe_time;
    unsigned long _firmware_probe_quiet_since; // 0 while not waiting for the line to go quiet
    bool _suspended;
};

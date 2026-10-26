#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include "NanoCommands.h"
#include "LiveDashboardData.h"

// Forward declarations
class UITask;
class HomeAssistantManager;

// Health of the SensorStack firmware itself (as opposed to the UART link)
enum class SensorStackFirmwareState : uint8_t {
    Ok = 0,        // Application present (or no evidence otherwise)
    Updating = 1,  // Firmware update in progress
    Faulty = 2     // urboot is waiting on the UART: no valid application
};

class SensorStackStatusManager {
public:
    SensorStackStatusManager();
    ~SensorStackStatusManager() = default;

    void begin(UITask* ui, HomeAssistantManager* ha);

    // Invoked by NanoUartBridge on connection & health telemetry
    void onPacketReceived();
    void setConnected(bool connected);
    void setVersion(const char* version_str);
    void setHealth(int first_time_flag, uint16_t free_ram, uint8_t reset_cause);
    void setSps30Info(const SPS30DeviceInfo& info);
    void setSgp41TestResult(const SGP41TestResult& result);
    void setScd30Info(bool auto_cal, uint16_t force_cal);
    void setScd30AutoCalState(bool auto_cal);
    void setScd30ForceCalValue(uint16_t force_cal);
    void setFirmwareState(SensorStackFirmwareState state);

    // Payload parsing methods
    bool processHealthPayload(char* payload, int* out_first_time_flag = nullptr);
    bool processSps30InfoPayload(char* payload);
    void processSps30CleanPayload(char* payload);
    bool processSgp41TestPayload(char* payload);
    bool processScd30InfoPayload(char* payload);
    bool processScd30AutoCalPayload(char* payload);
    bool processScd30ForceCalPayload(char* payload);

    // Queries
    bool isConnected() const { return _connected; }
    SensorStackFirmwareState getFirmwareState() const { return _firmware_state; }
    unsigned long getLastPacketTime() const { return _last_packet_time; }
    unsigned long getNanoBootMillis() const { return _nano_boot_millis; }
    uint16_t getFreeRam() const { return _free_ram; }
    const char* getVersion() const { return _version; }
    uint8_t getResetCauseCode() const { return _reset_cause_code; }
    const char* getResetCauseStr() const;

    const SPS30DeviceInfo& getSps30DeviceInfo() const { return _sps30_device_info; }
    const SGP41TestResult& getSgp41TestResult() const { return _sgp41_test_result_data; }

    // Dashboard population
    void populateDashboardData(LiveDashboardData& data) const;

    // Periodic tick for uptime & health diagnostics
    void updateDiagnostics();

    static const char* resetCauseToString(uint8_t code);

private:
    UITask* _ui;
    HomeAssistantManager* _ha;

    bool _connected;
    unsigned long _last_packet_time;
    unsigned long _nano_boot_millis;
    uint16_t _free_ram;
    char _version[12];
    uint8_t _reset_cause_code;
    bool _first_health_packet_received;
    SensorStackFirmwareState _firmware_state;

    SPS30DeviceInfo _sps30_device_info;
    SGP41TestResult _sgp41_test_result_data;
};

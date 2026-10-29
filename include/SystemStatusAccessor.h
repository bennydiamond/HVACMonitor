#pragma once

#include "BaseAccessor.h"
#include "SensorDefines.h"
#include "SensorStackFirmwareState.h"
#include "Logger.h"
#include <cstdint>
#include <cstring>

namespace SystemStatusDefaults {
    // Control & configuration defaults
    constexpr bool SCD30_AUTOCAL_STATE = false;
    constexpr uint16_t SCD30_FORCECAL_VALUE = SensorDefines::SCD30_FORCECAL_MIN_PPM;
    constexpr bool BACKLIGHT_STATE = true;
    constexpr uint8_t BACKLIGHT_BRIGHTNESS = 255;
    constexpr int INACTIVITY_TIMER_DELAY = 30; // seconds
    constexpr AppLogLevel LOG_LEVEL = APP_LOG_INFO;

    // Fixed-length string buffer capacities
    constexpr size_t SENSOR_STACK_VERSION_BUFFER_SIZE = 16;
    constexpr size_t NANO_RESET_CAUSE_BUFFER_SIZE = 16;
    constexpr size_t WIFI_SSID_BUFFER_SIZE = 33;
    constexpr size_t WIFI_IP_BUFFER_SIZE = 16;
    constexpr size_t WIFI_MAC_BUFFER_SIZE = 18;
}

struct SystemStatusData {
    // Control & configuration settings polled across tasks (MqttTask, WebServer, SensorStack, UITask)
    // SCD30 calibration as confirmed by the SensorStack. Each *Seq is incremented on
    // every reply to a change request (applied, refused or unconfirmed): consumers
    // compare it with the last one they saw and republish/redraw even when the value
    // did not change, which reverts a control showing the refused request.
    // false until the SensorStack first reported its SCD30 calibration settings: the
    // values below are only defaults until then
    bool scd30CalKnown = false;
    bool scd30AutoCalState = SystemStatusDefaults::SCD30_AUTOCAL_STATE;
    uint8_t scd30AutoCalSeq = 0;

    uint16_t scd30ForceCalValue = SystemStatusDefaults::SCD30_FORCECAL_VALUE;
    uint8_t scd30ForceCalSeq = 0;
    uint16_t scd30ForceCalTarget = 0; // Pending forced recalibration request (ppm), 0: none
    bool scd30AutoCalTarget = false;  // Requested state, sent with EVT_SCD30_AUTOCAL

    bool backlightState = SystemStatusDefaults::BACKLIGHT_STATE;
    uint8_t backlightBrightness = SystemStatusDefaults::BACKLIGHT_BRIGHTNESS;

    int inactivityTimerDelay = SystemStatusDefaults::INACTIVITY_TIMER_DELAY;
    // Incremented to have MqttTask republish with force (same pattern as the SCD30
    // *Seq counters: each consumer compares with the last value it saw, nothing is
    // cleared, so a request can't be lost between a read and a clear)
    uint8_t inactivityTimerDelaySeq = 0;

    AppLogLevel logLevel = SystemStatusDefaults::LOG_LEVEL;

    // Sensor Stack Status (from Nano)
    bool sensorConnectionStatus = false;
    char sensorStackVersion[SystemStatusDefaults::SENSOR_STACK_VERSION_BUFFER_SIZE] = {0};
    bool sensorStackVersionUnavailable = false;
    uint32_t sensorStackUptime = 0;
    uint8_t sensorStackUptimeSeq = 0; // Incremented to force a republish
    uint16_t sensorStackFreeRam = 0;
    char nanoResetCause[SystemStatusDefaults::NANO_RESET_CAUSE_BUFFER_SIZE] = {0};
    uint32_t sensorStackSecondsSincePacket = 0;
    SensorStackFirmwareState sensorStackFirmwareState = SensorStackFirmwareState::Ok;
    uint8_t sensorStackSensorFaults = 0;

    // WiFi & Network Status
    bool wifiConnected = false;
    int8_t wifiRssi = 0;
    char wifiSsid[SystemStatusDefaults::WIFI_SSID_BUFFER_SIZE] = {0};
    char wifiIp[SystemStatusDefaults::WIFI_IP_BUFFER_SIZE] = {0};
    char wifiMac[SystemStatusDefaults::WIFI_MAC_BUFFER_SIZE] = {0};

    // ESP32 System Health
    uint32_t esp32FreeHeap = 0;
    uint32_t esp32Uptime = 0;

    // MQTT connection status
    bool mqttConnected = false;
};

/**
 * @brief Thread-safe accessor inheriting from BaseAccessor, providing centralized
 *        mutex-protected read/write access to system status and settings across all tasks.
 */
class SystemStatusAccessor : public BaseAccessor {
public:
    SystemStatusAccessor();
    ~SystemStatusAccessor() override = default;

    SystemStatusData* operator->() { return &_state; }
    SystemStatusData& operator*() { return _state; }
    const SystemStatusData* operator->() const { return &_state; }
    const SystemStatusData& operator*() const { return _state; }

    // Static initialization of the underlying mutex
    static void init();

private:
    static SemaphoreHandle_t _sharedMutex;
    static StaticSemaphore_t _mutexBuffer;
    static SystemStatusData _state;
};


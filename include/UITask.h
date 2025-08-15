#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/stream_buffer.h>
#include <type_traits>
#include <atomic>
#include "ui/ui_common.h" // For FanStatus

// Forward declarations
class LGFX;
class CST820;
class IUIManager;
class IUIUpdater;

// X-macro list for UI message types
#define UI_MESSAGE_TYPE_LIST \
    X(UI_PRESSURE) \
    X(UI_FAN_STATUS) \
    X(UI_HIGH_PRESSURE) \
    X(UI_GEIGER_CPM) \
    X(UI_GEIGER_USVH) \
    X(UI_TEMP) \
    X(UI_HUMI) \
    X(UI_FAN_CURRENT) \
    X(UI_COMPRESSOR_AMPS) \
    X(UI_PUMP_AMPS) \
    X(UI_WATER_SENSOR) \
    X(UI_CO2) \
    X(UI_VOC) \
    X(UI_NO2) \
    X(UI_O3) \
    X(UI_NOX) \
    X(UI_CO) \
    X(UI_PM1) \
    X(UI_PM25) \
    X(UI_PM4) \
    X(UI_PM10) \
    X(UI_WIFI_CONNECTED) \
    X(UI_WIFI_RSSI) \
    X(UI_SENSOR_STATUS) \
    X(UI_CLEAR_SENSOR_READINGS) \
    X(UI_SPS30_FAN_INTERVAL) \
    X(UI_SPS30_FAN_DAYS) \
    X(UI_SGP41_TEST_STATUS) \
    X(UI_SGP41_TEST_VALUE) \
    X(UI_SCD30_AUTOCAL) \
    X(UI_SCD30_FORCECAL) \
    X(UI_LAST_PACKET_TIME) \
    X(UI_RUNTIME_FREE_HEAP) \
    X(UI_RUNTIME_UPTIME) \
    X(UI_SENSORSTACK_UPTIME) \
    X(UI_SENSORSTACK_RAM) \
    X(UI_NETWORK_RSSI) \
    X(UI_NETWORK_HA_CONN)

// X-macro for string message types
#define UI_STRING_TYPE_LIST \
    X(UI_STR_SSID) \
    X(UI_STR_IP) \
    X(UI_STR_MAC) \
    X(UI_STR_FW_VERSION)

class UITask {
public:
    UITask();
    ~UITask();
    void start();
    static UITask& getInstance();

    // Enum definition using X-macro
    enum UIMessageType {
#define X(name) name,
        UI_MESSAGE_TYPE_LIST
#undef X
        UI_MSG_COUNT
    };

    // String array definition using X-macro
    static const char* UIMessageTypeNames[UI_MSG_COUNT];

    struct UIMessage {
        UIMessage() = default;
        UIMessageType type;
        union {
            float f;
            int i;
            bool b;
        } value;
        template<typename T>
        UIMessage(UIMessageType t, T v) : type(t) {
            if constexpr (std::is_same<T, float>::value) value.f = v;
            else if constexpr (std::is_same<T, int>::value) value.i = v;
            else if constexpr (std::is_same<T, bool>::value) value.b = v;
            else static_assert(sizeof(T) == 0, "Unsupported UIMessage value type");
        }
    };

    uint8_t get_brightness() const {
        return brightness.load();
    }   

    // UI update methods (enqueue messages)
    void update_pressure(float p);
    void update_high_pressure_status(bool high);
    void update_geiger_reading(int cpm, float usvh);
    void update_temp_humi(float t, float h);
    void update_fan_current(float amps, FanStatus fan_status);
    void update_compressor_amps(float amps);
    void update_pump_amps(float amps);
    void update_water_sensor(bool water_ok);
    void update_co2(float value);
    void update_voc(float value);
    void update_no2(float value);
    void update_o3(float value);
    void update_nox(float value);
    void update_co(float value);
    void update_pm_values(float v1, float v2, float v3, float v4);
    void update_wifi_status(bool connected, long rssi);
    void update_sensor_status(bool connected);
    void set_brightness(uint8_t value);
    void update_ssid(const char* ssid);
    void update_ip(const char* ip);
    void update_mac(const char* mac);
    void update_fw_version(const char* fw);

    // Complex/event methods (leave as stubs for now)
    void clearSensorReadings();
    void update_sps30_fan_interval(unsigned long fan_interval);
    void update_sps30_fan_days(unsigned long fan_days);
    void update_sgp41_test_status(int ret_status);
    void update_sgp41_test_value(uint16_t raw_value);
    void update_scd30_autocal(bool enabled);
    void update_scd30_forcecal(uint16_t value);
    void update_last_packet_time(uint32_t seconds_since_packet);
    void update_runtime_free_heap(uint32_t free_heap);
    void update_runtime_uptime(unsigned long system_uptime);
    void update_sensorstack_uptime(uint32_t uptime);
    void update_sensorstack_ram(uint16_t ram);
    void update_network_rssi(int8_t rssi);
    void update_network_ha_conn(bool ha_conn);

private:
    static const uint32_t UI_DEQUEUE_TIME_MS = 20;
    static const uint32_t UITASK_SLEEP_TIME_MS = 15; // 66 fps
    static const uint32_t UI_QUEUE_WAIT_MAX_TIMES = 30;
    static const uint32_t STACK_CHECK_INTERVAL_MS = 60000;

    static const uint8_t DEFAULT_LCD_BRIGHTNESS = 255;

    static void taskFunc(void* param);
    void run();
    void dispatchMessage(const UIMessage& msg);
    bool queueSendOrWarn(const UIMessage& msg);

    // Owned UI objects
    LGFX* tft;
    CST820* touch;
    IUIManager* uiManager;
    IUIUpdater* uiUpdater;

    TaskHandle_t taskHandle;
    QueueHandle_t uiQueue;

    std::atomic<uint8_t> brightness = DEFAULT_LCD_BRIGHTNESS;

    // Static table of function pointers for dispatch
    using UIUpdateFunc = void(*)(IUIUpdater*, const UIMessage&);
    static const UIUpdateFunc updateTable[UI_MSG_COUNT];

    static constexpr size_t UI_STRING_MAX_LEN = 20;
    static constexpr size_t UI_STRING_MSG_SIZE = sizeof(uint8_t) + UI_STRING_MAX_LEN + 1;
    QueueHandle_t uiStringQueue = nullptr;
    enum UIStringType : uint8_t {
#define X(name) name,
        UI_STRING_TYPE_LIST
#undef X
        UI_STRING_COUNT
    };
    static const char* UIStringTypeNames[UI_STRING_COUNT];

    struct UIStringMessage {
        uint8_t type;
        char value[UI_STRING_MAX_LEN + 1];
        template<size_t N>
        UIStringMessage(UIStringType t, const char (&str)[N]) : type(t) {
            snprintf(value, UI_STRING_MAX_LEN + 1, "%s", str);
        }
        UIStringMessage() = default;
    };

    using UIStringUpdateFunc = void(*)(IUIUpdater*, const UIStringMessage&);
    static const UIStringUpdateFunc stringUpdateTable[UI_STRING_COUNT];

    bool queueSendStringOrWarn(const UIStringMessage& msg);
}; 
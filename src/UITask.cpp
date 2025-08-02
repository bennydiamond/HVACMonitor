#include "UITask.h"
#include "LGFX_ESP32_2432S022C.h"
#include "CST820.h"
#include "ui.h"
#include "ui/interfaces/IUIManager.h"
#include "ui/interfaces/IUIUpdater.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "ResetUtils.h"
#include "HVACMonitor.h"

// Singleton instance
static UITask* instance = nullptr;

// Static function pointer table for UI dispatch
const UITask::UIUpdateFunc UITask::updateTable[UI_MSG_COUNT] = {
    // UI_PRESSURE
    [](IUIUpdater* u, const UIMessage& m) { u->update_pressure(m.value.f); },
    // UI_FAN_STATUS
    [](IUIUpdater* u, const UIMessage& m) { u->update_fan_status(static_cast<FanStatus>(m.value.i)); },
    // UI_HIGH_PRESSURE
    [](IUIUpdater* u, const UIMessage& m) { u->update_high_pressure_status(m.value.b); },
    // UI_GEIGER_CPM
    [](IUIUpdater* u, const UIMessage& m) { u->update_geiger_cpm(m.value.i); },
    // UI_GEIGER_USVH
    [](IUIUpdater* u, const UIMessage& m) { u->update_geiger_usvh(m.value.f); },
    // UI_TEMP
    [](IUIUpdater* u, const UIMessage& m) { u->update_temp(m.value.f); },
    // UI_HUMI
    [](IUIUpdater* u, const UIMessage& m) { u->update_humi(m.value.f); },
    // UI_FAN_CURRENT
    [](IUIUpdater* u, const UIMessage& m) { u->update_fan_amps(m.value.f); },
    // UI_COMPRESSOR_AMPS
    [](IUIUpdater* u, const UIMessage& m) { u->update_compressor_amps(m.value.f); },
    // UI_PUMP_AMPS
    [](IUIUpdater* u, const UIMessage& m) { u->update_pump_amps(m.value.f); },
    // UI_WATER_SENSOR
    [](IUIUpdater* u, const UIMessage& m) { u->update_water_sensor(m.value.b); },
    // UI_CO2
    [](IUIUpdater* u, const UIMessage& m) { u->update_co2(m.value.f); },
    // UI_VOC
    [](IUIUpdater* u, const UIMessage& m) { u->update_voc(m.value.f); },
    // UI_NO2
    [](IUIUpdater* u, const UIMessage& m) { u->update_no2(m.value.f); },
    // UI_O3
    [](IUIUpdater* u, const UIMessage& m) { u->update_o3(m.value.f); },
    // UI_NOX
    [](IUIUpdater* u, const UIMessage& m) { u->update_nox(m.value.f); },
    // UI_CO
    [](IUIUpdater* u, const UIMessage& m) { u->update_co(m.value.f); },
    // UI_PM1
    [](IUIUpdater* u, const UIMessage& m) { u->update_pm1(m.value.f); },
    // UI_PM25
    [](IUIUpdater* u, const UIMessage& m) { u->update_pm25(m.value.f); },
    // UI_PM4
    [](IUIUpdater* u, const UIMessage& m) { u->update_pm4(m.value.f); },
    // UI_PM10
    [](IUIUpdater* u, const UIMessage& m) { u->update_pm10(m.value.f); },
    // UI_WIFI_CONNECTED
    [](IUIUpdater* u, const UIMessage& m) { u->update_wifi_status(m.value.b, 0); },
    // UI_WIFI_RSSI
    [](IUIUpdater* u, const UIMessage& m) { u->update_wifi_status(true, m.value.i); },
    // UI_SENSOR_STATUS
    [](IUIUpdater* u, const UIMessage& m) { u->update_sensor_status(m.value.b); },
    // UI_CLEAR_SENSOR_READINGS
    [](IUIUpdater* u, const UIMessage&) { u->clearSensorReadings(); },
    // UI_SPS30_FAN_INTERVAL
    [](IUIUpdater* u, const UIMessage& m) { u->update_sps30_fan_interval(m.value.i); },
    // UI_SPS30_FAN_DAYS
    [](IUIUpdater* u, const UIMessage& m) { u->update_sps30_fan_days(m.value.i); },
    // UI_SGP41_TEST_STATUS
    [](IUIUpdater* u, const UIMessage& m) { u->update_sgp41_test_status(m.value.i); },
    // UI_SGP41_TEST_VALUE
    [](IUIUpdater* u, const UIMessage& m) { u->update_sgp41_test_value(m.value.i); },
    // UI_SCD30_AUTOCAL
    [](IUIUpdater* u, const UIMessage& m) { u->update_scd30_autocal(m.value.b); },
    // UI_SCD30_FORCECAL
    [](IUIUpdater* u, const UIMessage& m) { u->update_scd30_forcecal(m.value.i); },
    // UI_LAST_PACKET_TIME
    [](IUIUpdater* u, const UIMessage& m) { u->update_last_packet_time(m.value.i); },
    // UI_RUNTIME_FREE_HEAP
    [](IUIUpdater* u, const UIMessage& m) { u->update_runtime_free_heap(m.value.i); },
    // UI_RUNTIME_UPTIME
    [](IUIUpdater* u, const UIMessage& m) { u->update_runtime_uptime(m.value.i); },
    // UI_SENSORSTACK_UPTIME
    [](IUIUpdater* u, const UIMessage& m) { u->update_sensorstack_uptime(m.value.i); },
    // UI_SENSORSTACK_RAM
    [](IUIUpdater* u, const UIMessage& m) { u->update_sensorstack_ram(m.value.i); },
    // UI_NETWORK_RSSI
    [](IUIUpdater* u, const UIMessage& m) { u->update_network_rssi(m.value.i); },
    // UI_NETWORK_HA_CONN
    [](IUIUpdater* u, const UIMessage& m) { u->update_network_ha_conn(m.value.b); },
};

UITask& UITask::getInstance() {
    if (!instance) {
        instance = new UITask();
    }
    return *instance;
}

UITask::UITask()
    : tft(nullptr), touch(nullptr), uiManager(nullptr), uiUpdater(nullptr),
      taskHandle(nullptr), uiQueue(nullptr)
{
    uiQueue = xQueueCreate(16, sizeof(UIMessage));
    uiStringQueue = xQueueCreate(8, sizeof(UIStringMessage));
}

UITask::~UITask() {
    // Cleanup if needed
}

void UITask::start() {
    xTaskCreatePinnedToCore(
        UITask::taskFunc,
        "UITask",
        4096,
        this,
        tskIDLE_PRIORITY + 1,
        &taskHandle,
        xPortGetCoreID()      // Core ID (use running core to avoid executing on WIFI core)
    );
}

void UITask::taskFunc(void* param) {
    static_cast<UITask*>(param)->run();
}

void UITask::run() {
    // Initialize UI objects here
    uint8_t local_bightness = brightness.load();
    tft = new LGFX();
    tft->init();
    tft->setRotation(1);
    tft->setBrightness(local_bightness);
    tft->fillScreen(TFT_BLACK);
    
    touch = new CST820(21, 22, -1, -1);
    touch->begin();

    uiManager = &UI::getInstance();
    uiUpdater = &UI::getInstance();
    uiManager->init(tft, touch);
    uiManager->create_widgets();
    uiUpdater->set_initial_debug_info(HVACMONITOR_FIRMWARE_VERSION, get_reset_reason_string());
    
    logger.info("UI Task initialized.");

    // Variables for periodic stack size logging
    unsigned long lastStackCheckTime = 0;

    while (true) {

        if(local_bightness != brightness.load()) {
            local_bightness = brightness.load();
            tft->setBrightness(local_bightness);
            logger.infof("UI brightness set to %u", local_bightness);
        }   

        // Periodically log the remaining stack size for this task
        uint32_t start = millis();
        if (start - lastStackCheckTime > STACK_CHECK_INTERVAL_MS) {
            lastStackCheckTime = start;
            UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL);
            logger.debugf("UITask task remaining stack: %u bytes", remaining_stack);
        }
        
        while (millis() - start < UI_DEQUEUE_TIME_MS) {
            // Prioritize string messages
            UIStringMessage strmsg;
            if (uiStringQueue && xQueueReceive(uiStringQueue, &strmsg, 0) == pdTRUE) {
                if (strmsg.type < UI_STRING_COUNT) {
                    stringUpdateTable[strmsg.type](uiUpdater, strmsg);
                }
            } else {
                UIMessage msg{};
                if (xQueueReceive(uiQueue, &msg, 0) == pdTRUE) {
                    dispatchMessage(msg);
                } else {
                    break;
                }
            }
        }
        uiManager->run();
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(UITASK_SLEEP_TIME_MS));
    }
}

void UITask::dispatchMessage(const UIMessage& msg) {
    if (msg.type < UI_MSG_COUNT) {
        updateTable[msg.type](uiUpdater, msg);
    }
}

// String array definition using X-macro
#define X(name) #name,
const char* UITask::UIMessageTypeNames[UI_MSG_COUNT] = {
    UI_MESSAGE_TYPE_LIST
};
#undef X

// Helper function to send to queue or log warning
bool UITask::queueSendOrWarn(const UIMessage& msg) {
    if (xQueueSend(uiQueue, &msg, UI_QUEUE_WAIT_MAX_TIMES) != pdTRUE) {
        logger.warningf("UI queue full, message dropped (%s)", UIMessageTypeNames[msg.type]);
        return false;
    }
    return true;
}

// Enqueue UI update messages
void UITask::update_pressure(float p) {
    queueSendOrWarn(UIMessage{UI_PRESSURE, p});
}
void UITask::update_high_pressure_status(bool high) {
    queueSendOrWarn(UIMessage{UI_HIGH_PRESSURE, high});
}
void UITask::update_co2(float value) {
    queueSendOrWarn(UIMessage{UI_CO2, value});
}
void UITask::update_voc(float value) {
    queueSendOrWarn(UIMessage{UI_VOC, value});
}
void UITask::update_no2(float value) {
    queueSendOrWarn(UIMessage{UI_NO2, value});
}
void UITask::update_o3(float value) {
    queueSendOrWarn(UIMessage{UI_O3, value});
}
void UITask::update_nox(float value) {
    queueSendOrWarn(UIMessage{UI_NOX, value});
}
void UITask::update_co(float value) {
    queueSendOrWarn(UIMessage{UI_CO, value});
}
void UITask::update_geiger_reading(int cpm, float usvh) {
    queueSendOrWarn(UIMessage{UI_GEIGER_CPM, cpm});
    queueSendOrWarn(UIMessage{UI_GEIGER_USVH, usvh});
}
void UITask::update_temp_humi(float temp, float humi) {
    queueSendOrWarn(UIMessage{UI_TEMP, temp});
    queueSendOrWarn(UIMessage{UI_HUMI, humi});
}
void UITask::update_fan_current(float amps, FanStatus fan_status) {
    queueSendOrWarn(UIMessage{UI_FAN_CURRENT, amps});
    queueSendOrWarn(UIMessage{UI_FAN_STATUS, static_cast<int>(fan_status)});
}
void UITask::update_compressor_amps(float amps) {
    queueSendOrWarn(UIMessage{UI_COMPRESSOR_AMPS, amps});
}
void UITask::update_pump_amps(float amps) {
    queueSendOrWarn(UIMessage{UI_PUMP_AMPS, amps});
}
void UITask::update_water_sensor(bool water_ok) {
    queueSendOrWarn(UIMessage{UI_WATER_SENSOR, water_ok});
}
void UITask::update_pm_values(float v1, float v2, float v3, float v4) {
    queueSendOrWarn(UIMessage{UI_PM1, v1});
    queueSendOrWarn(UIMessage{UI_PM25, v2});
    queueSendOrWarn(UIMessage{UI_PM4, v3});
    queueSendOrWarn(UIMessage{UI_PM10, v4});
}
void UITask::update_wifi_status(bool connected, long rssi) {
    queueSendOrWarn(UIMessage{UI_WIFI_CONNECTED, connected});
    queueSendOrWarn(UIMessage{UI_WIFI_RSSI, static_cast<int>(rssi)});
}
void UITask::update_sensor_status(bool connected) {
    queueSendOrWarn(UIMessage{UI_SENSOR_STATUS, connected});
}
void UITask::set_brightness(uint8_t value) {
    brightness.store(value);
}

// Complex/event methods (leave as stubs for now)
void UITask::clearSensorReadings() {
    queueSendOrWarn(UIMessage{UI_CLEAR_SENSOR_READINGS, 0});
}
void UITask::update_sps30_fan_interval(unsigned long fan_interval) {
    queueSendOrWarn(UIMessage{UI_SPS30_FAN_INTERVAL, static_cast<int>(fan_interval)});
}
void UITask::update_sps30_fan_days(unsigned long fan_days) {
    queueSendOrWarn(UIMessage{UI_SPS30_FAN_DAYS, static_cast<int>(fan_days)});
}
void UITask::update_sgp41_test_status(int ret_status) {
    queueSendOrWarn(UIMessage{UI_SGP41_TEST_STATUS, ret_status});
}
void UITask::update_sgp41_test_value(uint16_t raw_value) {
    queueSendOrWarn(UIMessage{UI_SGP41_TEST_VALUE, static_cast<int>(raw_value)});
}
void UITask::update_last_packet_time(uint32_t seconds_since_packet) {
    queueSendOrWarn(UIMessage{UI_LAST_PACKET_TIME, static_cast<int>(seconds_since_packet)});
}
void UITask::update_runtime_free_heap(uint32_t free_heap) {
    queueSendOrWarn(UIMessage{UI_RUNTIME_FREE_HEAP, static_cast<int>(free_heap)});
}
void UITask::update_runtime_uptime(unsigned long system_uptime) {
    queueSendOrWarn(UIMessage{UI_RUNTIME_UPTIME, static_cast<int>(system_uptime)});
}
void UITask::update_sensorstack_uptime(uint32_t uptime) {
    queueSendOrWarn(UIMessage{UI_SENSORSTACK_UPTIME, static_cast<int>(uptime)});
}
void UITask::update_sensorstack_ram(uint16_t ram) {
    queueSendOrWarn(UIMessage{UI_SENSORSTACK_RAM, static_cast<int>(ram)});
}
void UITask::update_network_rssi(int8_t rssi) {
    queueSendOrWarn(UIMessage{UI_NETWORK_RSSI, static_cast<int>(rssi)});
}
void UITask::update_network_ha_conn(bool ha_conn) {
    queueSendOrWarn(UIMessage{UI_NETWORK_HA_CONN, ha_conn});
} 
void UITask::update_scd30_autocal(bool enabled) {
    queueSendOrWarn(UIMessage{UI_SCD30_AUTOCAL, enabled});
}
void UITask::update_scd30_forcecal(uint16_t value) {
    queueSendOrWarn(UIMessage{UI_SCD30_FORCECAL, static_cast<int>(value)});
}
 
#define X(name) #name,
const char* UITask::UIStringTypeNames[UI_STRING_COUNT] = {
    UI_STRING_TYPE_LIST
};
#undef X

const UITask::UIStringUpdateFunc UITask::stringUpdateTable[UI_STRING_COUNT] = {
    [](IUIUpdater* u, const UIStringMessage& m) { u->update_ssid(m.value); },
    [](IUIUpdater* u, const UIStringMessage& m) { u->update_ip(m.value); },
    [](IUIUpdater* u, const UIStringMessage& m) { u->update_mac(m.value); },
    [](IUIUpdater* u, const UIStringMessage& m) { u->update_fw_version(m.value); },
};

bool UITask::queueSendStringOrWarn(const UIStringMessage& msg) {
    if (!uiStringQueue || xQueueSend(uiStringQueue, &msg, UI_QUEUE_WAIT_MAX_TIMES) != pdTRUE) {
        logger.warningf("UI string queue full, message dropped (%s)", UIStringTypeNames[msg.type]);
        return false;
    }
    return true;
}

void UITask::update_ssid(const char* ssid) {
    UIStringMessage msg;
    msg.type = UI_STR_SSID;
    snprintf(msg.value, UI_STRING_MAX_LEN + 1, "%s", ssid);
    queueSendStringOrWarn(msg);
}
void UITask::update_ip(const char* ip) {
    UIStringMessage msg;
    msg.type = UI_STR_IP;
    snprintf(msg.value, UI_STRING_MAX_LEN + 1, "%s", ip);
    queueSendStringOrWarn(msg);
}
void UITask::update_mac(const char* mac) {
    UIStringMessage msg;
    msg.type = UI_STR_MAC;
    snprintf(msg.value, UI_STRING_MAX_LEN + 1, "%s", mac);
    queueSendStringOrWarn(msg);
}
void UITask::update_fw_version(const char* fw) {
    UIStringMessage msg;
    msg.type = UI_STR_FW_VERSION;
    snprintf(msg.value, UI_STRING_MAX_LEN + 1, "%s", fw);
    queueSendStringOrWarn(msg);
} 
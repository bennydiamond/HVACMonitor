#include "UITask.h"
#include "TaskPriorities.h"
#include "LGFX_ESP32_2432S022C.h"
#include "CST820.h"
#include "ui.h"
#include "ui/interfaces/IUIManager.h"
#include "ui/interfaces/IUIUpdater.h"
#include "SensorDataAccessor.h"
#include "SystemStatusAccessor.h"
#include "DiagnosticDataAccessor.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "ResetUtils.h"
#include "FirmwareVersion.h"

#ifdef ENABLE_TASK_WATCHDOG
#include "WatchdogManager.h"
#endif

// Singleton instance
static UITask* instance = nullptr;
static std::atomic<bool> otaPopupDrawn{false};

static bool is_ota_popup_drawn() {
    return otaPopupDrawn.load();
}

UITask& UITask::getInstance() {
    if (!instance) {
        instance = new UITask();
    }
    return *instance;
}

UITask::UITask() : 
    tft(nullptr),
    touch(nullptr),
    uiManager(nullptr),
    uiUpdater(nullptr),
    taskHandle(nullptr),
    otaPopupRequested(false),
    uiIsInit(false)
{
    otaPopupDrawn.store(false);
}

UITask::~UITask() {
#ifdef ENABLE_TASK_WATCHDOG
    WatchdogManager::removeCurrentTask();
#endif
    if (uiManager != nullptr) {
        delete uiManager;
        uiManager = nullptr;
        uiUpdater = nullptr;
    }
    if (tft != nullptr) {
        delete tft;
        tft = nullptr;
    }
    if (touch != nullptr) {
        delete touch;
        touch = nullptr;
    }
    if (taskHandle != nullptr) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
}

void UITask::start() {
    BaseType_t result = xTaskCreatePinnedToCore(
        UITask::taskFunc,
        "UITask",
        STACK_SIZE_UI,
        this,
        TASK_PRIORITY_UI,
        &taskHandle,
        xPortGetCoreID() // Core ID (use running core to avoid executing on WIFI core)
    );

    if (result != pdPASS) {
        logger.errorf("Failed to create UITask. Free: %d, Min: %d, Max: %d bytes",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
        taskHandle = nullptr;
    } else {
        logger.infof("UITask created successfully on core %d", xPortGetCoreID());
    }
}

void UITask::taskFunc(void* param) {
    static_cast<UITask*>(param)->run();
}

void UITask::run() {
    while (1) {
#ifdef ENABLE_TASK_WATCHDOG
        WatchdogManager::addCurrentTask();
#endif
        uint8_t local_brightness = DEFAULT_LCD_BRIGHTNESS;
        {
            SystemStatusAccessor sys;
            local_brightness = sys->backlightState ? sys->backlightBrightness : 0;
        }

        if (tft == nullptr) {
            tft = new LGFX();
            tft->init();
            tft->setRotation(1);
            tft->setBrightness(local_brightness);
        }
        tft->fillScreen(TFT_BLACK);
        
        if (touch == nullptr) {
            touch = new CST820(21, 22, -1, -1);
            touch->begin();
        }

        UI* ui = new UI();
        uiManager = ui;
        uiUpdater = ui;
        uiManager->init(tft, touch);
        uiManager->create_widgets();

        uiUpdater->set_initial_debug_info(firmwareVersion(), get_reset_reason_string());

        uiIsInit = true;
        logger.info("UITask initialized.");
        unsigned long lastStackCheckTime = 0;

        SensorStateData lastSensorState;
        SystemStatusData lastSysState;
        bool firstRun = true;
        // Readings blanked on a SensorStack disconnect: values that come back
        // unchanged would otherwise never be redrawn
        bool readingsCleared = false;

        auto floatDiffers = [](float a, float b) -> bool {
            if (std::isnan(a) && std::isnan(b)) return false;
            if (std::isnan(a) != std::isnan(b)) return true;
            return a != b;
        };

        while (uiIsInit) {
            if (otaPopupRequested.load()) {
                show_ota_popup();
                otaPopupRequested.store(false);
                otaPopupDrawn.store(false);
                break;
            }

            uint32_t start = millis();
            if (start - lastStackCheckTime > STACK_CHECK_INTERVAL_MS) {
                lastStackCheckTime = start;
                UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL);
                logger.debugf("UITask task remaining stack: %u bytes", remaining_stack);
            }

            // Snapshot sensor and system states
            SensorStateData sensorState;
            {
                SensorDataAccessor sensor;
                sensorState = *sensor;
            }

            SystemStatusData sysState;
            {
                SystemStatusAccessor sys;
                sysState = *sys;
            }

            if (firstRun || sysState.backlightState != lastSysState.backlightState ||
                sysState.backlightBrightness != lastSysState.backlightBrightness) {
                uint8_t target_brightness = sysState.backlightState ? sysState.backlightBrightness : 0;
                if (local_brightness != target_brightness) {
                    local_brightness = target_brightness;
                    tft->setBrightness(local_brightness);
                    logger.infof("UI brightness set to %u", local_brightness);
                }
            }

            if (firstRun || sysState.sensorConnectionStatus != lastSysState.sensorConnectionStatus) {
                uiUpdater->update_sensor_status(sysState.sensorConnectionStatus);
                if (!sysState.sensorConnectionStatus) {
                    uiUpdater->clearSensorReadings();
                    readingsCleared = true;
                }
            }

            // Everything is redrawn with the first packet after the readings were cleared
            const bool redraw = firstRun ||
                (readingsCleared && sensorState.nano_updated_ms != lastSensorState.nano_updated_ms);
            readingsCleared = readingsCleared && !redraw;

            if (redraw || floatDiffers(sensorState.pressure, lastSensorState.pressure) ||
                floatDiffers(sensorState.filter_pressure, lastSensorState.filter_pressure)) {
                float displayPressure = (sensorState.filter_pressure > 0.0f) ? sensorState.filter_pressure : sensorState.pressure;
                uiUpdater->update_pressure(displayPressure);
            }

            if (redraw || sensorState.high_pressure != lastSensorState.high_pressure) {
                uiUpdater->update_high_pressure_status(sensorState.high_pressure);
            }
            if (redraw || sensorState.cpm != lastSensorState.cpm) {
                uiUpdater->update_geiger_cpm(sensorState.cpm);
            }
            if (redraw || floatDiffers(sensorState.geiger_usvh, lastSensorState.geiger_usvh)) {
                uiUpdater->update_geiger_usvh(sensorState.geiger_usvh);
            }
            if (redraw || floatDiffers(sensorState.temp, lastSensorState.temp)) {
                uiUpdater->update_temp(sensorState.temp);
            }
            if (redraw || floatDiffers(sensorState.humi, lastSensorState.humi)) {
                uiUpdater->update_humi(sensorState.humi);
            }
            if (redraw || floatDiffers(sensorState.co2, lastSensorState.co2)) {
                uiUpdater->update_co2(sensorState.co2);
            }
            if (redraw || floatDiffers(sensorState.fan_amps, lastSensorState.fan_amps)) {
                uiUpdater->update_fan_amps(sensorState.fan_amps);
            }
            if (redraw || sensorState.fan_status != lastSensorState.fan_status) {
                uiUpdater->update_fan_status(sensorState.fan_status);
            }
            if (redraw || floatDiffers(sensorState.compressor_amps, lastSensorState.compressor_amps)) {
                uiUpdater->update_compressor_amps(sensorState.compressor_amps);
            }
            if (redraw || floatDiffers(sensorState.geothermal_pump_amps, lastSensorState.geothermal_pump_amps)) {
                uiUpdater->update_pump_amps(sensorState.geothermal_pump_amps);
            }
            if (redraw || sensorState.liquid_level_sensor_state != lastSensorState.liquid_level_sensor_state) {
                uiUpdater->update_water_sensor(sensorState.liquid_level_sensor_state);
            }
            if (redraw || sensorState.voc_index != lastSensorState.voc_index) {
                uiUpdater->update_voc(sensorState.voc_index);
            }
            if (redraw || sensorState.nox_index != lastSensorState.nox_index) {
                uiUpdater->update_nox(sensorState.nox_index);
            }
            if (redraw || floatDiffers(sensorState.pm1, lastSensorState.pm1)) {
                uiUpdater->update_pm1(sensorState.pm1);
            }
            if (redraw || floatDiffers(sensorState.pm25, lastSensorState.pm25)) {
                uiUpdater->update_pm25(sensorState.pm25);
            }
            if (redraw || floatDiffers(sensorState.pm4, lastSensorState.pm4)) {
                uiUpdater->update_pm4(sensorState.pm4);
            }
            if (redraw || floatDiffers(sensorState.pm10, lastSensorState.pm10)) {
                uiUpdater->update_pm10(sensorState.pm10);
            }
            if (redraw || floatDiffers(sensorState.co_ppm, lastSensorState.co_ppm)) {
                uiUpdater->update_co(sensorState.co_ppm);
            }

            if (redraw || floatDiffers(sensorState.no2_conc_ug_per_m3, lastSensorState.no2_conc_ug_per_m3)) {
                uiUpdater->update_no2(sensorState.no2_conc_ug_per_m3);
            }
            if (redraw || floatDiffers(sensorState.o3_conc_ug_per_m3, lastSensorState.o3_conc_ug_per_m3)) {
                uiUpdater->update_o3(sensorState.o3_conc_ug_per_m3);
            }

            if (firstRun || strcmp(sysState.sensorStackVersion, lastSysState.sensorStackVersion) != 0 ||
                sysState.sensorStackVersionUnavailable != lastSysState.sensorStackVersionUnavailable) {
                uiUpdater->update_fw_version(sysState.sensorStackVersionUnavailable ? "" : sysState.sensorStackVersion);
            }

            if (firstRun || sysState.sensorStackSecondsSincePacket != lastSysState.sensorStackSecondsSincePacket) {
                uiUpdater->update_last_packet_time(sysState.sensorStackSecondsSincePacket);
            }
            if (firstRun || sysState.sensorStackUptime != lastSysState.sensorStackUptime) {
                uiUpdater->update_sensorstack_uptime(sysState.sensorStackUptime);
            }
            if (firstRun || sysState.sensorStackFreeRam != lastSysState.sensorStackFreeRam) {
                uiUpdater->update_sensorstack_ram(sysState.sensorStackFreeRam);
            }

            if (firstRun || sysState.sensorStackFirmwareState != lastSysState.sensorStackFirmwareState) {
                uiUpdater->update_sensorstack_firmware_state(sysState.sensorStackFirmwareState);
            }

            if (firstRun || sysState.sensorStackSensorFaults != lastSysState.sensorStackSensorFaults) {
                uiUpdater->update_sensorstack_sensor_faults(sysState.sensorStackSensorFaults);
            }

            static uint32_t lastSps30Timestamp = 0;
            static uint32_t lastSgp41Timestamp = 0;
            {
                DiagnosticDataAccessor diag;
                if (diag->sps30.valid && (firstRun || diag->sps30.timestamp != lastSps30Timestamp)) {
                    lastSps30Timestamp = diag->sps30.timestamp;
                    uiUpdater->update_sps30_fan_interval(diag->sps30.fanInterval);
                    uiUpdater->update_sps30_fan_days(diag->sps30.fanDays);
                }
                if (diag->sgp41.valid && (firstRun || diag->sgp41.timestamp != lastSgp41Timestamp)) {
                    lastSgp41Timestamp = diag->sgp41.timestamp;
                    uiUpdater->update_sgp41_test_status(diag->sgp41.retStatus);
                    uiUpdater->update_sgp41_test_value(diag->sgp41.rawValue);
                }
            }

            // A new reply from the Nano redraws even an unchanged value: the switch
            // may still show a request the Nano refused
            // Drawn once the SensorStack reported the actual settings
            const bool scd30CalRedraw = sysState.scd30CalKnown &&
                (firstRun || !lastSysState.scd30CalKnown);
            if (scd30CalRedraw ||
                (sysState.scd30CalKnown &&
                 (sysState.scd30AutoCalState != lastSysState.scd30AutoCalState ||
                  sysState.scd30AutoCalSeq != lastSysState.scd30AutoCalSeq))) {
                uiUpdater->update_scd30_autocal(sysState.scd30AutoCalState);
            }

            if (scd30CalRedraw ||
                (sysState.scd30CalKnown &&
                 (sysState.scd30ForceCalValue != lastSysState.scd30ForceCalValue ||
                  sysState.scd30ForceCalSeq != lastSysState.scd30ForceCalSeq))) {
                uiUpdater->update_scd30_forcecal(sysState.scd30ForceCalValue);
            }

            if (firstRun || sysState.wifiConnected != lastSysState.wifiConnected || sysState.wifiRssi != lastSysState.wifiRssi) {
                uiUpdater->update_wifi_status(sysState.wifiConnected, sysState.wifiRssi);
                uiUpdater->update_network_rssi(sysState.wifiRssi);
            }
            if (firstRun || strcmp(sysState.wifiSsid, lastSysState.wifiSsid) != 0) {
                uiUpdater->update_ssid(sysState.wifiSsid);
            }
            if (firstRun || strcmp(sysState.wifiIp, lastSysState.wifiIp) != 0) {
                uiUpdater->update_ip(sysState.wifiIp);
            }
            if ((firstRun && sysState.wifiMac[0] != '\0') || strcmp(sysState.wifiMac, lastSysState.wifiMac) != 0) {
                uiUpdater->update_mac(sysState.wifiMac);
            }

            if (firstRun || sysState.esp32FreeHeap != lastSysState.esp32FreeHeap) {
                uiUpdater->update_runtime_free_heap(sysState.esp32FreeHeap);
            }
            if (firstRun || sysState.esp32Uptime != lastSysState.esp32Uptime) {
                uiUpdater->update_runtime_uptime(sysState.esp32Uptime);
            }

            if (firstRun || sysState.mqttConnected != lastSysState.mqttConnected) {
                uiUpdater->update_network_ha_conn(sysState.mqttConnected);
            }

            lastSensorState = sensorState;
            lastSysState = sysState;
            firstRun = false;

            uint32_t sleepMs = UITASK_SLEEP_TIME_MS;
            if (uiManager != nullptr) {
                uint32_t next = uiManager->run();
                sleepMs = next < 1 ? 1 : (next > UITASK_SLEEP_TIME_MS_MAX ? UITASK_SLEEP_TIME_MS_MAX : next);
            }
#ifdef ENABLE_TASK_WATCHDOG
            WatchdogManager::reset();
#endif
            vTaskDelay(pdMS_TO_TICKS(sleepMs));
        }
    }
}

void UITask::show_ota_popup() {
    logger.debug("UITask: Show OTA popup");
    uiIsInit = false;
#ifdef ENABLE_TASK_WATCHDOG
    WatchdogManager::removeCurrentTask();
#endif
    delete uiManager;
    uiManager = nullptr;
    uiUpdater = nullptr;
    if (tft) {
        tft->setBrightness(255);
        tft->fillScreen(TFT_BLACK);
        
        // Draw rounded box
        tft->fillRoundRect(40, 80, 240, 80, 10, TFT_DARKGREY);
        tft->drawRoundRect(40, 80, 240, 80, 10, TFT_WHITE);
        
        tft->setTextColor(TFT_WHITE);
        tft->setTextSize(2);
        tft->setTextDatum(textdatum_t::top_center);
        tft->drawString("OTA UPDATE", 160, 100);
        tft->drawString("IN PROGRESS...", 160, 130);
        otaPopupDrawn.store(true);
    }
    vTaskSuspend(NULL); // self suspend
}

void UITask::queue_ota_popup() {
    logger.debug("UITask: Queue OTA popup");
    otaPopupRequested.store(true);
}

void UITask::prepare_for_ota() {
    logger.info("Killing UITask to recover heap");
    if (instance) {
        logger.debug("Killing UITask: Send msg");
        instance->queue_ota_popup();
        while (!is_ota_popup_drawn()) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        logger.debug("Killing UITask: UITask has processed self kill");
    }
}

void UITask::restore_after_ota() {
    logger.info("Resuming UITask");
    if (instance && instance->taskHandle) {
        vTaskResume(instance->taskHandle);
#ifdef ENABLE_TASK_WATCHDOG
        WatchdogManager::addTask(instance->taskHandle);
#endif
    }
}

void UITask::SetPauseState(bool paused) {
    if (instance && instance->taskHandle) {
        if (paused) {
            vTaskSuspend(instance->taskHandle);
        } else {
            vTaskResume(instance->taskHandle);
        }
    }
}

#include "CST820.h"
#include "ConfigManager.h"
#include "FilterPressureTracker.h"
#include "GasConcentrationConverter.h"
#include "HVACMonitor.h"
#include "HomeAssistantManager.h"
#include "I2CBridge.h"
#include "LGFX_ESP32_2432S022C.h"
#include "Logger.h"
#include "MainTaskEvents.h"
#include "NanoCommands.h"
#include "NanoUartBridge.h"
#include "SensorStackStatusManager.h"
#include "SensorDataProcessor.h"
#include "OtaManager.h"
#include "RebootManager.h"
#include "ResetUtils.h"
#include "RollingAverage.h"
#include "BootloopGuard.h"
#include "SdUpdateManager.h"
#include "SensorDefines.h"
#include "SensorTask.h"
#include "SerialMutex.h"
#include "SensorDataAccessor.h"
#include "SystemStatusAccessor.h"
#include "TaskPriorities.h"
#include "UITask.h"
#include "WebServerManager.h"
#include "ZMOD4510Sensor.h"
#include "LiveDashboardData.h"
#include "secrets.h"
#include "ui.h"
#include "ui/interfaces/IUIManager.h"
#include "ui/interfaces/IUIUpdater.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_bootloader_desc.h>
#include <lvgl.h>
#include <math.h>
#ifdef ENABLE_TASK_WATCHDOG
#include "WatchdogManager.h"
#endif

// =================== TIMING CONSTANTS & GLOBALS ===================
const int WIFI_CHECK_INTERVAL_MS = 10000;
const int DEBUG_UPDATE_INTERVAL_MS = 5000;
const int STACK_CHECK_INTERVAL_MS = 60000;

// Task watchdog timeout. Longer during setup() (Wi-Fi, sensors, tasks starting).
// MQTT runs in its own task, off the watchdog: see MQTT_TASK_STALL_TIMEOUT_MS.
const uint32_t WATCHDOG_TIMEOUT_S = 10;
const uint32_t WATCHDOG_SETUP_TIMEOUT_S = 30;
// MqttTask's network calls can block for a while (DNS, a slow write on a weak link)
// but never this long: a heartbeat older than this means the task is stuck
const uint32_t MQTT_TASK_STALL_TIMEOUT_MS = 120000;

unsigned long last_wifi_check_time = 0;
unsigned long last_debug_update_time = 0;
unsigned long last_stack_check_time = 0;

// Application Managers & Collaborators
ConfigManager configManager;
HomeAssistantManager haManager;
OtaManager otaManager;
WebServerManager webServerManager;
FilterPressureTracker filterPressureTracker;
SensorTask sensorTask;

SensorStackStatusManager statusManager;
SensorDataProcessor dataProcessor;
NanoUartBridge nanoUartBridge;

// Rolling Averages for ESP32 Local Sensors
RollingAverage<uint16_t> o3_avg(100);
RollingAverage<uint16_t> no2_avg(100);
RollingAverage<uint16_t> fast_aqi_avg(100);
RollingAverage<uint16_t> epa_aqi_avg(100);
#ifdef BMP280_ENABLED
RollingAverage<float> bmp280_pressure_avg(10);
RollingAverage<float> bmp280_temperature_avg(10);
#endif
#ifdef AHT20_ENABLED
RollingAverage<float> aht20_temperature_avg(10);
RollingAverage<float> aht20_humidity_avg(10);
#endif

TaskHandle_t mainTaskHandle = nullptr;

static volatile bool wifi_state_changed = false;

// =================== UTILITY FUNCTIONS ===================
void update_wifi_status() {
  char current_ssid[33];
  {
    ConfigManagerAccessor config;
    snprintf(current_ssid, sizeof(current_ssid), "%s", config->getWifiSsid());
  }
  if (strlen(current_ssid) == 0) {
    snprintf(current_ssid, sizeof(current_ssid), "%s", WIFI_SSID);
  }
  bool connected = (WiFi.status() == WL_CONNECTED);
  int8_t rssi = connected ? WiFi.RSSI() : 0;

  char ip_str[16] = "0.0.0.0";
  if (connected) {
    IPAddress ip = WiFi.localIP();
    snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }

  char mac_str[18];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  {
    SystemStatusAccessor status;
    status->wifiConnected = connected;
    status->wifiRssi = rssi;
    snprintf(status->wifiSsid, sizeof(status->wifiSsid), "%s", current_ssid);
    snprintf(status->wifiIp, sizeof(status->wifiIp), "%s", ip_str);
    snprintf(status->wifiMac, sizeof(status->wifiMac), "%s", mac_str);
  }
}

// =================== SETUP & LOOP ===================
void setup_wifi() {
  logger.info("Connecting to WiFi...");
  char ssid[33];
  char password[65];
  {
    ConfigManagerAccessor config;
    snprintf(ssid, sizeof(ssid), "%s", config->getWifiSsid());
    snprintf(password, sizeof(password), "%s", config->getWifiPassword());
  }

  size_t ssid_len = strlen(ssid);
  if (ssid_len == 0 || ssid_len > 32) {
    logger.warningf("WiFi SSID empty or invalid; falling back to default '%s'.", WIFI_SSID);
    snprintf(ssid, sizeof(ssid), "%s", WIFI_SSID);
  }

  size_t pass_len = strlen(password);
  if (pass_len > 0 && (pass_len < 8 || pass_len > 64)) {
    logger.errorf("Invalid WiFi password (length %u); falling back to default password.", pass_len);
    snprintf(password, sizeof(password), "%s", WIFI_PASSWORD);
    pass_len = strlen(password);
  }

  logger.infof("Initiating WiFi connection to SSID '%s'%s", ssid,
               (pass_len == 0) ? " (open network)" : "");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    wifi_state_changed = true;
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    wifi_state_changed = true;
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(ssid, (pass_len > 0) ? password : nullptr);
  unsigned long start_time = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start_time < 15000) {
    // Without Wi-Fi this waits the full 15 s: keep it off the watchdog's account
#ifdef ENABLE_TASK_WATCHDOG
    WatchdogManager::reset();
#endif
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setSleep(false);
    logger.infof("WiFi connected. IP address: %s, RSSI: %d dBm",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    logger.errorf("WiFi connection failed (status: %d).", WiFi.status());
  }
  update_wifi_status();
}

void setup() {
  SdUpdateManager::checkForUpdate();

  BootloopGuard::begin();

#ifdef ENABLE_TASK_WATCHDOG
  // Panic: a hung task reboots the device (reset reason "Task watchdog" is logged
  // at the next boot); BootloopGuard rolls back a firmware that keeps doing it
  WatchdogManager::init(WATCHDOG_SETUP_TIMEOUT_S, true);
  WatchdogManager::addCurrentTask();
#endif

  SerialMutex::getInstance().init();
  MainTaskEventNotifier::getInstance().setMainTaskHandle(xTaskGetCurrentTaskHandle());
  vTaskPrioritySet(NULL, TASK_PRIORITY_MAIN);

  logger.info("--- System Booting ---");
  AppLogLevel log_level;
  int inactivity_delay;
  {
    ConfigManagerAccessor config;
    config->init();
    log_level = config->getLogLevel();
    inactivity_delay = static_cast<int>(config->getInactivityTimerDelay());
  }
  logger.init(&haManager);
  logger.setLogLevel(log_level);
  {
    // Shared copies of the saved settings, published by MqttTask and shown by the UI
    SystemStatusAccessor status;
    status->logLevel = log_level;
    status->inactivityTimerDelay = inactivity_delay;
  }

  const char *reset_reason = get_reset_reason_string();
  logger.infof("Firmware Version: %s", HVACMONITOR_FIRMWARE_VERSION);
  logger.infof("Reset Reason: %s", reset_reason);
  FlashMode_t fMode = ESP.getFlashChipMode();
  const char *fModeStr = (fMode == FM_QIO)    ? "QIO"
                         : (fMode == FM_QOUT) ? "QOUT"
                         : (fMode == FM_DIO)  ? "DIO"
                         : (fMode == FM_DOUT) ? "DOUT"
                                              : "UNKNOWN";
  logger.infof("Hardware: CPU %lu MHz | Flash %lu MHz [%s]",
               (unsigned long)getCpuFrequencyMhz(),
               (unsigned long)(ESP.getFlashChipSpeed() / 1000000),
               fModeStr);

  esp_bootloader_desc_t boot_desc;
  if (esp_ota_get_bootloader_description(nullptr, &boot_desc) == ESP_OK) {
    logger.infof("Bootloader: IDF %s | Built: %s", boot_desc.idf_ver, boot_desc.date_time);
  } else {
    logger.warning("Bootloader: Legacy (No esp_bootloader_desc found)");
  }



  filterPressureTracker.begin();
  RebootManager::init(filterPressureTracker);
  UITask::getInstance().start();
  float initial_filter_p = filterPressureTracker.getFilterPressure();
  if (initial_filter_p > 0.0f) {
    SensorDataAccessor()->filter_pressure = initial_filter_p;
  }

  setup_wifi();
  haManager.init(HVACMONITOR_FIRMWARE_VERSION);
  webServerManager.init(HVACMONITOR_FIRMWARE_VERSION);
  otaManager.init();

  sensorTask.init();

  // Initialize modular collaborators
  statusManager.begin();
  dataProcessor.begin(&filterPressureTracker, &sensorTask);
  nanoUartBridge.begin(SERIAL_BAUD_RATE, &statusManager, &dataProcessor, &sensorTask);

#ifdef ENABLE_TASK_WATCHDOG
  WatchdogManager::addCurrentTask();
  WatchdogManager::setTimeout(WATCHDOG_TIMEOUT_S); // Setup done
#endif
}

void dispatch_main_task_event(MainTaskEventNotifier::EventBits event) {
  switch (event) {
  case MainTaskEventNotifier::EVT_SCD30_AUTOCAL: {
    // The latest request wins
    bool enabled = SystemStatusAccessor()->scd30AutoCalTarget;
    nanoUartBridge.sendCommandWithParam(CMD_SET_SCD30_AUTOCAL, enabled ? '1' : '0');
    break;
  }
  case MainTaskEventNotifier::EVT_SPS30_CLEAN:
    nanoUartBridge.sendCommand(CMD_SPS30_CLEAN);
    break;
  case MainTaskEventNotifier::EVT_SGP41_TEST:
    nanoUartBridge.sendCommand(CMD_SGP41_TEST);
    break;
  case MainTaskEventNotifier::EVT_REBOOT_ESP32:
    RebootManager::getInstance().requestReboot("Event", 500);
    break;
  case MainTaskEventNotifier::EVT_REBOOT_NANO:
    logger.warning("Rebooting SensorStack as requested by event.");
    nanoUartBridge.sendCommand(CMD_REBOOT);
    break;
  case MainTaskEventNotifier::EVT_SPS30_GET_INFO:
    logger.info("Requesting SPS30 info via event.");
    nanoUartBridge.sendCommand(CMD_GET_SPS30_INFO);
    break;
  case MainTaskEventNotifier::EVT_SCD30_GET_INFO:
    logger.info("Requesting SCD30 info via event.");
    nanoUartBridge.sendCommand(CMD_GET_SCD30_INFO);
    break;
  case MainTaskEventNotifier::EVT_SCD30_FORCECAL: {
    uint16_t ppm = 0;
    {
      SystemStatusAccessor status;
      ppm = status->scd30ForceCalTarget;
    }
    if (SensorDefines::isValidForceCal(ppm)) {
      logger.infof("Sending SCD30 Force Cal to %u ppm via event.", ppm);
      char cmd_with_param[8];
      snprintf(cmd_with_param, sizeof(cmd_with_param), "%c%u",
               CMD_SET_SCD30_FORCECAL, ppm);
      nanoUartBridge.sendCommandStr(cmd_with_param);
    } else {
      logger.warningf("Ignored out-of-range SCD30 Force Cal event: %u ppm", ppm);
    }
    break;
  }
  default:
    break;
  }
}

void loop() {
#ifdef ENABLE_TASK_WATCHDOG
  WatchdogManager::reset();
#endif

  BootloopGuard::handle();

  static bool previous_wifi_connected = (WiFi.status() == WL_CONNECTED);
  bool current_wifi_connected = (WiFi.status() == WL_CONNECTED);
  if (wifi_state_changed || (current_wifi_connected != previous_wifi_connected)) {
    wifi_state_changed = false;
    previous_wifi_connected = current_wifi_connected;
    update_wifi_status();
  }

  static bool previous_mqtt_connect_status = false;

  bool ha_conn = haManager.isMqttConnected();

  if (previous_mqtt_connect_status != ha_conn) {
    previous_mqtt_connect_status = ha_conn;
    SystemStatusAccessor()->mqttConnected = ha_conn;
  }

  static bool mqtt_stall_reported = false;
  if (haManager.msSinceMqttTaskHeartbeat() > MQTT_TASK_STALL_TIMEOUT_MS &&
      !mqtt_stall_reported) {
    mqtt_stall_reported = true;
    logger.errorf("MQTT task stuck for more than %lu s; rebooting.",
                  (unsigned long)(MQTT_TASK_STALL_TIMEOUT_MS / 1000));
    RebootManager::getInstance().requestReboot("MQTT task stuck");
  }
  otaManager.handle();

  static unsigned long lastHeapCheck = 0;
  static size_t cached_free_heap = 0;
  if (millis() - lastHeapCheck >= 250 || cached_free_heap == 0) {
    cached_free_heap = ESP.getFreeHeap();
    lastHeapCheck = millis();
  }
  if (cached_free_heap > 15000) {
    webServerManager.handle();
  } else {
    static unsigned long lastMemWarning = 0;
    if (millis() - lastMemWarning > 30000) {
      logger.warningf("Skipping web server handling due to low memory: "
                      "free=%d, min=%d, max=%d bytes",
                      cached_free_heap, ESP.getMinFreeHeap(),
                      ESP.getMaxAllocHeap());
      lastMemWarning = millis();
    }
  }

  logger.loop();

  // Handle incoming UART bytes, framing, and periodic queries
  nanoUartBridge.handle();

  if (millis() - last_wifi_check_time > WIFI_CHECK_INTERVAL_MS) {
    last_wifi_check_time = millis();
    // Wi-Fi retry, whether it never connected or was lost. Arduino's auto-reconnect
    // skips some cases (e.g. after an authentication failure); this covers them all.
    static uint32_t wifi_retry_attempts = 0;
    static unsigned long wifi_down_since = 0;
    if (WiFi.status() != WL_CONNECTED) {
      if (wifi_retry_attempts == 0) {
        wifi_down_since = millis();
        logger.warningf("WiFi not connected (status %d); retrying every %d s.",
                        WiFi.status(), WIFI_CHECK_INTERVAL_MS / 1000);
      } else if (wifi_retry_attempts % 30 == 0) { // every 5 min
        logger.warningf("WiFi still not connected (status %d), retry attempt %lu.",
                        WiFi.status(), (unsigned long)wifi_retry_attempts);
      }
      wifi_retry_attempts++;
      WiFi.reconnect();
    } else if (wifi_retry_attempts > 0) {
      // Logs queued while offline reach syslog now; this one sums up the outage
      logger.infof("WiFi reconnected after %lu attempts (down %lu s). IP: %s, RSSI: %d dBm",
                   (unsigned long)wifi_retry_attempts, (millis() - wifi_down_since) / 1000,
                   WiFi.localIP().toString().c_str(), WiFi.RSSI());
      wifi_retry_attempts = 0;
    }
    update_wifi_status();
    SystemStatusAccessor()->mqttConnected = ha_conn;
  }

  if (millis() - last_debug_update_time > DEBUG_UPDATE_INTERVAL_MS) {
    last_debug_update_time = millis();
    statusManager.updateDiagnostics();

    {
      SystemStatusAccessor status;
      status->esp32FreeHeap = ESP.getFreeHeap();
      status->esp32Uptime = millis() / 1000;
    }
  }

  // Get ZMOD4510 sensor data
  SensorTask::ZMOD4510Values zmod_values;
  const bool freshValues = sensorTask.getZMOD4510Data(zmod_values);

  if (freshValues) {
    o3_avg.add(zmod_values.o3_conc_ppb);
    no2_avg.add(zmod_values.no2_conc_ppb);
    fast_aqi_avg.add(zmod_values.fast_aqi);
    epa_aqi_avg.add(zmod_values.epa_aqi);

#ifdef BMP280_ENABLED
    float current_temperature = bmp280_temperature_avg.getAverage();
    float current_pressure = bmp280_pressure_avg.getAverage();
    bool values_fresh =
        !bmp280_temperature_avg.isEmpty() && !bmp280_pressure_avg.isEmpty();
#else
    float current_temperature = 20.0f;
    float current_pressure = 101325.0f;
    bool values_fresh = true;
#endif

    if (values_fresh) {
      float o3_ug_per_m3 = GasConcentrationConverter::convertO3PpbToUgPerM3(
          o3_avg.getAverage(), current_temperature, current_pressure);
      float no2_ug_per_m3 = GasConcentrationConverter::convertNO2PpbToUgPerM3(
          no2_avg.getAverage(), current_temperature, current_pressure);

      logger.debugf("Gas Conversion: O3=%u ppb -> %.2f µg/m³, NO2=%u ppb -> "
                    "%.2f µg/m³ (T=%.1f°C, P=%.1f Pa)",
                    o3_avg.getAverage(), o3_ug_per_m3, no2_avg.getAverage(),
                    no2_ug_per_m3, current_temperature, current_pressure);

      {
        SensorDataAccessor sensor;
        sensor->o3_conc_ug_per_m3 = o3_ug_per_m3;
        sensor->no2_conc_ug_per_m3 = no2_ug_per_m3;
        sensor->fast_aqi = fast_aqi_avg.getAverage();
        sensor->epa_aqi = epa_aqi_avg.getAverage();
      }
    }
  }

  // Get BMP280 sensor data
#ifdef BMP280_ENABLED
  SensorTask::BMP280Values bmp280_values;
  const bool freshBMP280Values = sensorTask.getBMP280Data(bmp280_values);

  if (freshBMP280Values) {
    bmp280_pressure_avg.add(bmp280_values.pressure_pa);
    bmp280_temperature_avg.add(bmp280_values.temperature_degc);

    float averaged_pressure = bmp280_pressure_avg.getAverage();
    float averaged_temperature = bmp280_temperature_avg.getAverage();
    {
      SensorDataAccessor sensor;
      sensor->bmp_pressure_pa = averaged_pressure;
      sensor->bmp_temp_c = averaged_temperature;
    }

    logger.debugf("BMP280: P=%.1f Pa (avg: %.1f Pa), T=%.1f°C (avg: %.1f°C)",
                  bmp280_values.pressure_pa, averaged_pressure,
                  bmp280_values.temperature_degc, averaged_temperature);
  }
#endif

  // Get AHT20 sensor data
#ifdef AHT20_ENABLED
  SensorTask::AHT20Values aht20_values;
  const bool freshAHT20Values = sensorTask.getAHT20Data(aht20_values);

  if (freshAHT20Values) {
    aht20_temperature_avg.add(aht20_values.temperature_degc);
    aht20_humidity_avg.add(aht20_values.humidity_pct);
    {
      SensorDataAccessor sensor;
      sensor->aht_temp_c = aht20_temperature_avg.getAverage();
      sensor->aht_humi_pct = aht20_humidity_avg.getAverage();
    }

    logger.debugf("AHT20: T=%.1f°C (avg: %.1f°C), H=%.1f%% (avg: %.1f%%)",
                  aht20_values.temperature_degc,
                  aht20_temperature_avg.getAverage(), aht20_values.humidity_pct,
                  aht20_humidity_avg.getAverage());
  }
#endif

  if (millis() - last_stack_check_time > STACK_CHECK_INTERVAL_MS) {
    last_stack_check_time = millis();
    UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL);
    logger.debugf("Main loop task remaining stack: %u bytes, free heap: %u "
                  "bytes, min free: %u bytes, max alloc: %u bytes",
                  remaining_stack, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  ESP.getMaxAllocHeap());
  }

  MainTaskEventNotifier::getInstance().processOneEvent(dispatch_main_task_event);

  if (Serial.available() > 0) {
    vTaskDelay(pdMS_TO_TICKS(1));
  } else {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // Cancel ESP-IDF bootloader rollback after completing main loop pass
  BootloopGuard::cancelBootloaderRollback();
}

void get_live_dashboard_data(LiveDashboardData &data) {
  // Populate sensor telemetry from SensorDataProcessor
  dataProcessor.populateDashboardData(data);

  // Populate Nano SensorStack health and diagnostics from SensorStackStatusManager
  statusManager.populateDashboardData(data);

  // ESP32 local sensor data (ZMOD4510 & BMP280)
  data.o3_ug = o3_avg.isEmpty() ? -1.0f : o3_avg.getAverage();
  data.no2_ug = no2_avg.isEmpty() ? -1.0f : no2_avg.getAverage();
  data.fast_aqi = fast_aqi_avg.isEmpty() ? -1 : fast_aqi_avg.getAverage();
  data.epa_aqi = epa_aqi_avg.isEmpty() ? -1 : epa_aqi_avg.getAverage();
#ifdef BMP280_ENABLED
  data.baro_pressure_hpa = bmp280_pressure_avg.isEmpty() ? -1.0f : bmp280_pressure_avg.getAverage() / 100.0f;
#else
  data.baro_pressure_hpa = 1013.25f;
#endif

  // System & Network (Integrator)
  bool wifi_conn = (WiFi.status() == WL_CONNECTED);
  data.wifi_connected = wifi_conn;
  data.wifi_rssi = wifi_conn ? WiFi.RSSI() : 0;
  if (wifi_conn) {
    IPAddress ip = WiFi.localIP();
    snprintf(data.wifi_ip, sizeof(data.wifi_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(data.wifi_mac, sizeof(data.wifi_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    strncpy(data.wifi_ip, "0.0.0.0", sizeof(data.wifi_ip) - 1);
  }
  {
    ConfigManagerAccessor config;
    if (strlen(config->getWifiSsid()) > 0) {
      strncpy(data.wifi_ssid, config->getWifiSsid(), sizeof(data.wifi_ssid) - 1);
    } else {
      strncpy(data.wifi_ssid, WIFI_SSID, sizeof(data.wifi_ssid) - 1);
    }
    strncpy(data.mqtt_host, config->getMqttHost(), sizeof(data.mqtt_host) - 1);
    data.mqtt_port = config->getMqttPort();
  }
  data.mqtt_connected = haManager.isMqttConnected();

  // Diagnostics & ESP32 Health (Developer)
  data.esp32_free_heap = esp_get_free_heap_size();
  data.esp32_min_free_heap = esp_get_minimum_free_heap_size();
  data.esp32_max_alloc_heap = ESP.getMaxAllocHeap();
  data.esp32_uptime_seconds = millis() / 1000;
  strncpy(data.esp32_reset_reason, get_reset_reason_string(), sizeof(data.esp32_reset_reason) - 1);
  strncpy(data.firmware_version, HVACMONITOR_FIRMWARE_VERSION, sizeof(data.firmware_version) - 1);
}
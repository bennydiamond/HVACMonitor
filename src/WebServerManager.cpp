#include "WebServerManager.h"
#include "ConfigManager.h"
#include "HomeAssistantManager.h"
#include "Logger.h"
#include "RebootManager.h"
#include <Update.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "webserver/WebServerAssets.h"
#include "webserver/WebServerDashboard.h"
#include "webserver/WebServerOTA.h"
#include "webserver/WebServerConfigClimate.h"
#include "webserver/WebServerConfigMaintenance.h"
#include "webserver/WebServerConfigSystem.h"
#include "webserver/WebServerConfigTabs.h"
#include "webserver/WebServerConfigTabsExtra.h"
#include "LiveDashboardData.h"
#include "NanoCommands.h"
#include "UITask.h"
#include "MainTaskEvents.h"

WebServer server(80);
char WebServerManager::_firmware_version[16] = {0};

template <typename F>
static void streamTemplate(const char *html_template, F resolver) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", "");

  char chunk[512];
  size_t chunk_pos = 0;

  auto flush_chunk = [&]() {
    if (chunk_pos > 0) {
      server.sendContent(chunk, chunk_pos);
      chunk_pos = 0;
    }
  };

  auto write_str = [&](const char *s, size_t len) {
    while (len > 0) {
      size_t space = sizeof(chunk) - chunk_pos;
      if (space == 0) {
        flush_chunk();
        space = sizeof(chunk);
      }
      size_t to_copy = (len < space) ? len : space;
      memcpy(chunk + chunk_pos, s, to_copy);
      chunk_pos += to_copy;
      s += to_copy;
      len -= to_copy;
    }
  };

  const char *p = html_template;
  while (*p) {
    const char *token_start = strchr(p, '%');
    if (!token_start) {
      write_str(p, strlen(p));
      break;
    }

    if (token_start > p) {
      write_str(p, token_start - p);
    }

    const char *token_end = strchr(token_start + 1, '%');
    bool valid_token = false;
    char key[32] = {0};

    if (token_end && (token_end - token_start > 1) &&
        (token_end - token_start < (int)sizeof(key))) {
      size_t klen = token_end - token_start - 1;
      valid_token = true;
      for (size_t i = 0; i < klen; i++) {
        char c = token_start[1 + i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
          valid_token = false;
          break;
        }
      }
      if (valid_token) {
        memcpy(key, token_start + 1, klen);
        key[klen] = '\0';
      }
    }

    if (valid_token) {
      char val_buf[96] = {0};
      if (resolver(key, val_buf, sizeof(val_buf))) {
        write_str(val_buf, strlen(val_buf));
        p = token_end + 1;
      } else {
        // Unknown key, output literal token
        write_str(token_start, token_end - token_start + 1);
        p = token_end + 1;
      }
    } else {
      // Lone '%' or not a placeholder token, output '%'
      write_str(token_start, 1);
      p = token_start + 1;
    }
  }

  flush_chunk();
  server.sendContent("", 0);
}

void WebServerManager::init(const char *firmware_version) {
  strncpy(_firmware_version, firmware_version ? firmware_version : "", sizeof(_firmware_version) - 1);
  _firmware_version[sizeof(_firmware_version) - 1] = '\0';

  // Static assets (no-cache ensures browsers always check for latest asset versions)
  server.on("/style.css", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("ETag", _firmware_version);
    server.send_P(200, "text/css", style_css);
  });
  server.on("/app.js", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("ETag", _firmware_version);
    server.send_P(200, "application/javascript", app_js);
  });
  server.on("/favicon.ico", HTTP_GET, []() { server.send(204); });

  server.on("/", HTTP_GET, handleRoot);
  server.on("/ota", HTTP_GET, handleOTA);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/config", HTTP_GET, handleConfig);

  server.on("/config/gas", HTTP_GET, handleConfigGas);
  server.on("/config/pm", HTTP_GET, handleConfigPM);
  server.on("/config/hvac", HTTP_GET, handleConfigHVAC);
  server.on("/config/climate", HTTP_GET, handleConfigClimate);
  server.on("/config/system", HTTP_GET, handleConfigSystem);
  server.on("/config/maintenance", HTTP_GET, handleConfigMaintenance);
  server.on("/config/update", HTTP_POST, handleConfigUpdate);

  server.on(
      "/upload", HTTP_POST,
      []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "Update complete! Rebooting...");
        RebootManager::getInstance().requestReboot("Web OTA update complete", 500);
      },
      handleUpload);

  server.onNotFound(handleNotFound);

  server.begin();
  logger.info("HTTP server for OTA started.");
}

void WebServerManager::handle() {
  // Reset watchdog to prevent crashes during long operations
#ifdef ENABLE_TASK_WATCHDOG
  esp_task_wdt_reset();
#endif

  // Handle client requests
  server.handleClient();

  // Yield to other tasks to prevent blocking
  yield();
}

void WebServerManager::handleRoot() {
  streamTemplate(dashboard_page_html, [](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    return false;
  });
  logger.debug("Served live dashboard web page.");
}

void WebServerManager::handleOTA() {
  streamTemplate(ota_page_html, [](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    return false;
  });
  logger.debug("Served OTA update web page.");
}

void WebServerManager::handleApiStatus() {
  LiveDashboardData data;
  get_live_dashboard_data(data);

  // Read configured thresholds
  int co2_w = 1000, co2_d = 1500;
  int voc_w = 250, voc_d = 400;
  int nox_w = 10, nox_d = 20;
  int pm1_w = 25, pm1_d = 50;
  int pm25_w = 35, pm25_d = 75;
  int pm4_w = 50, pm4_d = 100;
  int pm10_w = 50, pm10_d = 150;
  int co_w = 9, co_d = 25;
  int o3_w = 100, o3_d = 180;
  int no2_w = 100, no2_d = 200;
  float fan_off = 0.5f;

  {
    ConfigManagerAccessor config;
    co2_w = config->getCo2WarnThreshold();
    co2_d = config->getCo2DangerThreshold();
    voc_w = config->getVocWarnThreshold();
    voc_d = config->getVocDangerThreshold();
    nox_w = config->getNoxWarnThreshold();
    nox_d = config->getNoxDangerThreshold();
    pm1_w = config->getPm1WarnThreshold();
    pm1_d = config->getPm1DangerThreshold();
    pm25_w = config->getPm25WarnThreshold();
    pm25_d = config->getPm25DangerThreshold();
    pm4_w = config->getPm4WarnThreshold();
    pm4_d = config->getPm4DangerThreshold();
    pm10_w = config->getPm10WarnThreshold();
    pm10_d = config->getPm10DangerThreshold();
    co_w = config->getCoWarnThreshold();
    co_d = config->getCoDangerThreshold();
    o3_w = config->getO3WarnThreshold();
    o3_d = config->getO3DangerThreshold();
    no2_w = config->getNo2WarnThreshold();
    no2_d = config->getNo2DangerThreshold();
    fan_off = config->getFanOffCurrentThreshold();
  }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", "");

  char chunk[384];

  // Section: iaq
  snprintf(chunk, sizeof(chunk),
           "{\"iaq\":{\"epa_aqi\":%u,\"fast_aqi\":%u,\"co2\":%.1f,\"voc\":%ld,\"nox\":%ld,"
           "\"pm1\":%.1f,\"pm25\":%.1f,\"pm4\":%.1f,\"pm10\":%.1f,\"t\":%.1f,\"h\":%.1f,"
           "\"baro\":%.1f,\"co\":%u,\"o3\":%.1f,\"no2\":%.1f,\"usv\":%.2f,\"cpm\":%d},",
           data.epa_aqi, data.fast_aqi, data.co2_ppm, (long)data.voc_index, (long)data.nox_index,
           data.pm1_ug, data.pm25_ug, data.pm4_ug, data.pm10_ug, data.temperature_c, data.humidity_pct,
           data.baro_pressure_hpa, data.co_ppm, data.o3_ug, data.no2_ug, data.geiger_usv_h, data.geiger_cpm);
  server.sendContent(chunk);

  // Section: hvac
  snprintf(chunk, sizeof(chunk),
           "\"hvac\":{\"fp\":%.1f,\"dp\":%.1f,\"hp\":%s,\"fa\":%.2f,\"fs\":%u,"
           "\"ca\":%.2f,\"pa\":%.2f,\"water\":%s},",
           data.filter_pressure_pa, data.diff_pressure_pa, data.is_pressure_high ? "true" : "false",
           data.fan_amps, data.fan_status, data.compressor_amps, data.pump_amps,
           data.water_detected ? "true" : "false");
  server.sendContent(chunk);

  // Section: sys
  snprintf(chunk, sizeof(chunk),
           "\"sys\":{\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"mac\":\"%s\",\"wc\":%s,"
           "\"mh\":\"%s\",\"mp\":%d,\"mc\":%s,\"nv\":\"%s\",\"dsp\":%lu,\"nu\":%lu,"
           "\"sc\":%s,\"nrc\":\"%s\",\"nr\":%u},",
           data.wifi_ssid, data.wifi_ip, (int)data.wifi_rssi, data.wifi_mac,
           data.wifi_connected ? "true" : "false",
           data.mqtt_host, data.mqtt_port, data.mqtt_connected ? "true" : "false",
           data.nano_version, (unsigned long)data.seconds_since_packet,
           (unsigned long)data.nano_uptime_seconds,
           data.sensor_module_connected ? "true" : "false",
           data.nano_reset_cause, (unsigned int)data.nano_free_ram);
  server.sendContent(chunk);

  // Section: diag
  snprintf(chunk, sizeof(chunk),
           "\"diag\":{\"heap\":%lu,\"min_heap\":%lu,\"max_alloc\":%lu,\"uptime\":%lu,"
           "\"reset\":\"%s\",\"ver\":\"%s\"},",
           (unsigned long)data.esp32_free_heap, (unsigned long)data.esp32_min_free_heap,
           (unsigned long)data.esp32_max_alloc_heap, (unsigned long)data.esp32_uptime_seconds,
           data.esp32_reset_reason, data.firmware_version);
  server.sendContent(chunk);

  // Section: th
  snprintf(chunk, sizeof(chunk),
           "\"th\":{\"co2_w\":%d,\"co2_d\":%d,\"voc_w\":%d,\"voc_d\":%d,\"nox_w\":%d,\"nox_d\":%d,"
           "\"pm1_w\":%d,\"pm1_d\":%d,\"pm25_w\":%d,\"pm25_d\":%d,\"pm4_w\":%d,\"pm4_d\":%d,"
           "\"pm10_w\":%d,\"pm10_d\":%d,\"co_w\":%d,\"co_d\":%d,\"o3_w\":%d,\"o3_d\":%d,"
           "\"no2_w\":%d,\"no2_d\":%d,\"fan_off\":%.1f}}",
           co2_w, co2_d, voc_w, voc_d, nox_w, nox_d,
           pm1_w, pm1_d, pm25_w, pm25_d, pm4_w, pm4_d,
           pm10_w, pm10_d, co_w, co_d, o3_w, o3_d,
           no2_w, no2_d, fan_off);
  server.sendContent(chunk);
}

void WebServerManager::handleConfig() {
  size_t freeHeap = ESP.getFreeHeap();

  streamTemplate(config_main_html, [](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    return false;
  });
  logger.infof("Served main config page. Free: %u, Min: %u, Max: %u bytes",
               freeHeap, ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
}

void WebServerManager::handleConfigUpdate() {
  // Check available memory before processing
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 12000) {
    logger.warningf(
        "Low memory (free=%u, min=%u, max=%u bytes), rejecting config update",
        freeHeap, ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    server.send(503, "text/plain",
                "Service temporarily unavailable - low memory");
    return;
  }

  String section = server.arg("section");

  if (section == "wifi" && server.hasArg("action") &&
      server.arg("action") == "save") {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    ssid.trim();
    if (ssid.length() == 0) {
      server.send(400, "text/plain", "SSID cannot be empty");
      return;
    }
    if (ssid.length() > 32) {
      server.send(400, "text/plain", "SSID cannot exceed 32 characters");
      return;
    }
    if (password.length() > 64) {
      server.send(400, "text/plain", "Password cannot exceed 64 characters");
      return;
    }

    {
      ConfigManagerAccessor config;
      config->setWifiSsid(ssid.c_str());
      config->setWifiPassword(password.c_str());
    }

    logger.infof("Wi-Fi configuration saved (SSID: %s). Rebooting device...",
                 ssid.c_str());
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain",
                "Wi-Fi configuration saved successfully. Rebooting device...");

    RebootManager::getInstance().requestReboot("Wi-Fi configuration updated", 2000);
    return;
  }

  if (section == "mqtt_device" && server.hasArg("action") &&
      server.arg("action") == "save") {
    String host = server.arg("host");
    String port = server.arg("port");
    String user = server.arg("user");
    String password = server.arg("password");
    String deviceId = server.arg("device_id");
    String deviceName = server.arg("device_name");

    if (host.length() == 0 || user.length() == 0 || password.length() == 0 ||
        deviceId.length() == 0 || deviceName.length() == 0) {
      server.send(400, "text/plain", "All MQTT fields are required");
      return;
    }

    int portNum = port.toInt();
    if (portNum <= 0 || portNum > 65535) {
      server.send(400, "text/plain", "Invalid port number");
      return;
    }

    ConfigManagerAccessor config;
    config->setMqttHost(host.c_str());
    config->setMqttPort(portNum);
    config->setMqttUser(user.c_str());
    config->setMqttPassword(password.c_str());
    config->setMqttDeviceId(deviceId.c_str());
    config->setMqttDeviceName(deviceName.c_str());

    logger.info("MQTT device configuration saved. Rebooting device...");
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain",
                "Configuration saved successfully. Rebooting device...");

    RebootManager::getInstance().requestReboot("MQTT configuration updated", 2000);
    return;
  }

  // Action requests (button triggers)
  if (server.hasArg("action")) {
    String action = server.arg("action");
    if (action == "sps30_clean") {
      logger.info("SPS30 manual fan clean requested via Web Server");
      MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_SPS30_CLEAN);
      server.send(200, "text/plain", "SPS30 fan clean started (10s cycle)");
      return;
    }
    if (action == "sps30_info") {
      logger.info("SPS30 info query requested via Web Server");
      if (query_sps30_info_blocking(3000)) {
        const SPS30DeviceInfo& info = get_sps30_device_info();
        char resp[128];
        snprintf(resp, sizeof(resp), "Firmware: v%u.%u | Clean Interval: %lus (%lud) | Status: 0x%08lX (%s)",
                 info.fw_major, info.fw_minor, info.fan_interval, info.fan_days, info.status_reg,
                 info.status_reg == 0 ? "OK" : "Fan Speed Warning");
        server.send(200, "text/plain", resp);
      } else {
        server.send(504, "text/plain", "Timeout waiting for SPS30 info from sensor stack");
      }
      return;
    }
    if (action == "sgp41_test") {
      logger.info("SGP41 self-test requested via Web Server");
      if (run_sgp41_test_blocking(4000)) {
        const SGP41TestResult& res = get_sgp41_test_result();
        char resp[96];
        if (res.ret_status == 0 && res.raw_value == 0xD400) {
          snprintf(resp, sizeof(resp), "OK / Passed (Value: 0x%04X, Status: 0)", res.raw_value);
        } else {
          snprintf(resp, sizeof(resp), "Failed (Status: %d, Value: 0x%04X)",
                   res.ret_status, res.raw_value);
        }
        server.send(200, "text/plain", resp);
      } else {
        server.send(504, "text/plain", "Timeout waiting for SGP41 test from sensor stack");
      }
      return;
    }
    if (action == "reboot_nano") {
      logger.warning("Sensor Hub (Nano) reboot requested via Web Server");
      MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_REBOOT_NANO);
      server.send(200, "text/plain", "Sensor Hub reboot command sent");
      return;
    }
    if (action == "reboot_esp32") {
      logger.warning("ESP32 reboot requested via Web Server");
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", "Device rebooting in 2 seconds...");
      RebootManager::getInstance().requestReboot("Web System page reboot", 2000);
      return;
    }
    server.send(400, "text/plain", "Unknown action");
    return;
  }

  // Regular parameter updates (require section, type, and value)
  if (server.hasArg("section") && server.hasArg("type") &&
      server.hasArg("value")) {
    String type = server.arg("type");
    String valueStr = server.arg("value");

    int intVal = valueStr.toInt();
    float floatVal = valueStr.toFloat();

    ConfigManagerAccessor config;
    bool ok = true;

    if (section == "no2") {
      if (type == "warn")
        config->setNo2WarnThreshold(intVal);
      else if (type == "danger")
        config->setNo2DangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "o3") {
      if (type == "warn")
        config->setO3WarnThreshold(intVal);
      else if (type == "danger")
        config->setO3DangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "nox") {
      if (type == "warn")
        config->setNoxWarnThreshold(intVal);
      else if (type == "danger")
        config->setNoxDangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "co") {
      if (type == "warn")
        config->setCoWarnThreshold(intVal);
      else if (type == "danger")
        config->setCoDangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "pm1") {
      if (type == "warn")
        config->setPm1WarnThreshold(intVal);
      else if (type == "danger")
        config->setPm1DangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "pm25") {
      if (type == "warn")
        config->setPm25WarnThreshold(intVal);
      else if (type == "danger")
        config->setPm25DangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "pm4") {
      if (type == "warn")
        config->setPm4WarnThreshold(intVal);
      else if (type == "danger")
        config->setPm4DangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "pm10") {
      if (type == "warn")
        config->setPm10WarnThreshold(intVal);
      else if (type == "danger")
        config->setPm10DangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "co2") {
      if (type == "warn")
        config->setCo2WarnThreshold(intVal);
      else if (type == "danger")
        config->setCo2DangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "pressure") {
      if (type == "low")
        config->setPressureLowThreshold(floatVal);
      else if (type == "mid")
        config->setPressureMidThreshold(floatVal);
      else if (type == "high")
        config->setHighPressureThreshold(floatVal);
      else
        ok = false;
    } else if (section == "fan") {
      if (type == "on")
        config->setFanOnCurrentThreshold(floatVal);
      else if (type == "off")
        config->setFanOffCurrentThreshold(floatVal);
      else if (type == "high")
        config->setFanHighCurrentThreshold(floatVal);
      else
        ok = false;
    } else if (section == "compressor") {
      if (type == "on")
        config->setCompressorOnCurrentThreshold(floatVal);
      else if (type == "off")
        config->setCompressorOffCurrentThreshold(floatVal);
      else if (type == "high")
        config->setCompressorHighCurrentThreshold(floatVal);
      else
        ok = false;
    } else if (section == "pump") {
      if (type == "on")
        config->setPumpOnCurrentThreshold(floatVal);
      else if (type == "off")
        config->setPumpOffCurrentThreshold(floatVal);
      else if (type == "high")
        config->setPumpHighCurrentThreshold(floatVal);
      else
        ok = false;
    } else if (section == "geiger") {
      if (type == "abnormal_low")
        config->setGeigerAbnormalLowThreshold(floatVal);
      else if (type == "abnormal_high")
        config->setGeigerAbnormalHighThreshold(floatVal);
      else if (type == "danger_high")
        config->setGeigerDangerHighThreshold(floatVal);
      else
        ok = false;
    } else if (section == "temp") {
      if (type == "comfortable_low")
        config->setTempComfortableLow(floatVal);
      else if (type == "comfortable_high")
        config->setTempComfortableHigh(floatVal);
      else if (type == "acceptable_low")
        config->setTempAcceptableLow(floatVal);
      else if (type == "acceptable_high")
        config->setTempAcceptableHigh(floatVal);
      else
        ok = false;
    } else if (section == "humi") {
      if (type == "comfortable_low")
        config->setHumiComfortableLow(floatVal);
      else if (type == "comfortable_high")
        config->setHumiComfortableHigh(floatVal);
      else if (type == "acceptable_low")
        config->setHumiAcceptableLow(floatVal);
      else if (type == "acceptable_high")
        config->setHumiAcceptableHigh(floatVal);
      else
        ok = false;
    } else if (section == "inactivity") {
      if (type == "timer_delay") {
        config->setInactivityTimerDelay(intVal);
        HomeAssistantManager* ha = HomeAssistantManager::getInstance();
        if (ha) ha->updateInactivityTimerDelayState(true);
      } else {
        ok = false;
      }
    } else if (section == "log") {
      if (type == "level" && intVal >= 0 && intVal <= 3) {
        AppLogLevel level = static_cast<AppLogLevel>(intVal);
        config->setLogLevel(level);
        logger.setLogLevel(level);
        HomeAssistantManager* ha = HomeAssistantManager::getInstance();
        if (ha) ha->updateLogLevelState(level);
        logger.infof("Log level updated to %d via Web Server", intVal);
      } else {
        ok = false;
      }
    } else if (section == "mqtt") {
      if (type == "host")
        config->setMqttHost(valueStr.c_str());
      else if (type == "port")
        config->setMqttPort(intVal);
      else if (type == "user")
        config->setMqttUser(valueStr.c_str());
      else if (type == "password")
        config->setMqttPassword(valueStr.c_str());
      else if (type == "device_id")
        config->setMqttDeviceId(valueStr.c_str());
      else if (type == "device_name")
        config->setMqttDeviceName(valueStr.c_str());
      else
        ok = false;
    } else if (section == "voc") {
      if (type == "warn")
        config->setVocWarnThreshold(intVal);
      else if (type == "danger")
        config->setVocDangerThreshold(intVal);
      else
        ok = false;
    } else if (section == "display") {
      if (type == "brightness" && intVal >= 0 && intVal <= 255) {
        uint8_t b = static_cast<uint8_t>(intVal);
        UITask::getInstance().set_brightness(b);
        HomeAssistantManager* ha = HomeAssistantManager::getInstance();
        if (ha) ha->updateBacklightState(b > 0, b);
        logger.infof("Display brightness set to %u via Web Server", b);
      } else if (type == "backlight") {
        uint8_t curB = UITask::getInstance().get_brightness();
        uint8_t newB = (intVal != 0) ? (curB == 0 ? 255 : curB) : 0;
        UITask::getInstance().set_brightness(newB);
        HomeAssistantManager* ha = HomeAssistantManager::getInstance();
        if (ha) ha->updateBacklightState(intVal != 0, newB);
        logger.infof("Display backlight set to %s via Web Server", intVal != 0 ? "ON" : "OFF");
      } else {
        ok = false;
      }
    } else if (section == "scd30") {
      if (type == "autocal") {
        bool enabled = (intVal != 0);
        send_command_to_nano_with_param(CMD_SET_SCD30_AUTOCAL, enabled ? '1' : '0');
        HomeAssistantManager* ha = HomeAssistantManager::getInstance();
        if (ha) ha->updateScd30AutoCalState(enabled);
        UITask::getInstance().update_scd30_autocal(enabled);
        logger.infof("SCD30 AutoCalibration set to %s via Web Server", enabled ? "ON" : "OFF");
      } else if (type == "forcecal" && intVal >= 400 && intVal <= 2000) {
        uint16_t ppm = static_cast<uint16_t>(intVal);
        char cmd_str[8];
        snprintf(cmd_str, sizeof(cmd_str), "%c%u", CMD_SET_SCD30_FORCECAL, ppm);
        send_command_to_nano_str(cmd_str);
        HomeAssistantManager* ha = HomeAssistantManager::getInstance();
        if (ha) ha->updateScd30ForceCalValue(ppm);
        UITask::getInstance().update_scd30_forcecal(ppm);
        logger.infof("SCD30 Force Calibration set to %u ppm via Web Server", ppm);
      } else {
        ok = false;
      }
    } else {
      ok = false;
    }

    if (ok) {
      server.send(200, "text/plain", "Saved successfully");
      logger.infof("Threshold updated: %s_%s = %s", section.c_str(),
                   type.c_str(), valueStr.c_str());
    } else {
      server.send(400, "text/plain", "Invalid section or type");
    }
  } else {
    server.send(400, "text/plain", "Missing required parameters");
  }
}

void log_update_error() {
  logger.errorf("Firmware update failed. Error code: %d", Update.getError());
}

void WebServerManager::handleUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    logger.infof("Web OTA Update Started: %s", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      log_update_error();
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      log_update_error();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      logger.infof("Web OTA Update Success: %u bytes", upload.totalSize);
    } else {
      log_update_error();
    }
  }
}

void WebServerManager::handleConfigGas() {
  ConfigManagerAccessor config;
  streamTemplate(config_gas_html, [&](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    if (strcmp(key, "NO2_WARN") == 0) { snprintf(buf, len, "%d", config->getNo2WarnThreshold()); return true; }
    if (strcmp(key, "NO2_DANGER") == 0) { snprintf(buf, len, "%d", config->getNo2DangerThreshold()); return true; }
    if (strcmp(key, "O3_WARN") == 0) { snprintf(buf, len, "%d", config->getO3WarnThreshold()); return true; }
    if (strcmp(key, "O3_DANGER") == 0) { snprintf(buf, len, "%d", config->getO3DangerThreshold()); return true; }
    if (strcmp(key, "NOX_WARN") == 0) { snprintf(buf, len, "%d", config->getNoxWarnThreshold()); return true; }
    if (strcmp(key, "NOX_DANGER") == 0) { snprintf(buf, len, "%d", config->getNoxDangerThreshold()); return true; }
    if (strcmp(key, "CO_WARN") == 0) { snprintf(buf, len, "%d", config->getCoWarnThreshold()); return true; }
    if (strcmp(key, "CO_DANGER") == 0) { snprintf(buf, len, "%d", config->getCoDangerThreshold()); return true; }
    if (strcmp(key, "CO2_WARN") == 0) { snprintf(buf, len, "%d", config->getCo2WarnThreshold()); return true; }
    if (strcmp(key, "CO2_DANGER") == 0) { snprintf(buf, len, "%d", config->getCo2DangerThreshold()); return true; }
    if (strcmp(key, "VOC_WARN") == 0) { snprintf(buf, len, "%d", config->getVocWarnThreshold()); return true; }
    if (strcmp(key, "VOC_DANGER") == 0) { snprintf(buf, len, "%d", config->getVocDangerThreshold()); return true; }
    return false;
  });
  logger.info("Served gas config page");
}

void WebServerManager::handleConfigPM() {
  ConfigManagerAccessor config;
  streamTemplate(config_pm_html, [&](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    if (strcmp(key, "PM1_WARN") == 0) { snprintf(buf, len, "%d", config->getPm1WarnThreshold()); return true; }
    if (strcmp(key, "PM1_DANGER") == 0) { snprintf(buf, len, "%d", config->getPm1DangerThreshold()); return true; }
    if (strcmp(key, "PM25_WARN") == 0) { snprintf(buf, len, "%d", config->getPm25WarnThreshold()); return true; }
    if (strcmp(key, "PM25_DANGER") == 0) { snprintf(buf, len, "%d", config->getPm25DangerThreshold()); return true; }
    if (strcmp(key, "PM4_WARN") == 0) { snprintf(buf, len, "%d", config->getPm4WarnThreshold()); return true; }
    if (strcmp(key, "PM4_DANGER") == 0) { snprintf(buf, len, "%d", config->getPm4DangerThreshold()); return true; }
    if (strcmp(key, "PM10_WARN") == 0) { snprintf(buf, len, "%d", config->getPm10WarnThreshold()); return true; }
    if (strcmp(key, "PM10_DANGER") == 0) { snprintf(buf, len, "%d", config->getPm10DangerThreshold()); return true; }
    return false;
  });
  logger.info("Served PM config page");
}

void WebServerManager::handleConfigHVAC() {
  ConfigManagerAccessor config;
  streamTemplate(config_hvac_html, [&](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    if (strcmp(key, "PRESSURE_LOW") == 0) { snprintf(buf, len, "%.1f", config->getPressureLowThreshold()); return true; }
    if (strcmp(key, "PRESSURE_MID") == 0) { snprintf(buf, len, "%.1f", config->getPressureMidThreshold()); return true; }
    if (strcmp(key, "PRESSURE_HIGH") == 0) { snprintf(buf, len, "%.1f", config->getHighPressureThreshold()); return true; }
    if (strcmp(key, "FAN_ON") == 0) { snprintf(buf, len, "%.1f", config->getFanOnCurrentThreshold()); return true; }
    if (strcmp(key, "FAN_OFF") == 0) { snprintf(buf, len, "%.1f", config->getFanOffCurrentThreshold()); return true; }
    if (strcmp(key, "FAN_HIGH") == 0) { snprintf(buf, len, "%.1f", config->getFanHighCurrentThreshold()); return true; }
    if (strcmp(key, "COMPRESSOR_ON") == 0) { snprintf(buf, len, "%.1f", config->getCompressorOnCurrentThreshold()); return true; }
    if (strcmp(key, "COMPRESSOR_OFF") == 0) { snprintf(buf, len, "%.1f", config->getCompressorOffCurrentThreshold()); return true; }
    if (strcmp(key, "COMPRESSOR_HIGH") == 0) { snprintf(buf, len, "%.1f", config->getCompressorHighCurrentThreshold()); return true; }
    if (strcmp(key, "PUMP_ON") == 0) { snprintf(buf, len, "%.1f", config->getPumpOnCurrentThreshold()); return true; }
    if (strcmp(key, "PUMP_OFF") == 0) { snprintf(buf, len, "%.1f", config->getPumpOffCurrentThreshold()); return true; }
    if (strcmp(key, "PUMP_HIGH") == 0) { snprintf(buf, len, "%.1f", config->getPumpHighCurrentThreshold()); return true; }
    if (strcmp(key, "GEIGER_ABNORMAL_LOW") == 0) { snprintf(buf, len, "%.2f", config->getGeigerAbnormalLowThreshold()); return true; }
    if (strcmp(key, "GEIGER_ABNORMAL_HIGH") == 0) { snprintf(buf, len, "%.2f", config->getGeigerAbnormalHighThreshold()); return true; }
    if (strcmp(key, "GEIGER_DANGER_HIGH") == 0) { snprintf(buf, len, "%.2f", config->getGeigerDangerHighThreshold()); return true; }
    return false;
  });
  logger.info("Served HVAC config page");
}

void WebServerManager::handleConfigClimate() {
  ConfigManagerAccessor config;
  streamTemplate(config_climate_html, [&](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    if (strcmp(key, "TEMP_COMFORTABLE_LOW") == 0) { snprintf(buf, len, "%.1f", config->getTempComfortableLow()); return true; }
    if (strcmp(key, "TEMP_COMFORTABLE_HIGH") == 0) { snprintf(buf, len, "%.1f", config->getTempComfortableHigh()); return true; }
    if (strcmp(key, "TEMP_ACCEPTABLE_LOW") == 0) { snprintf(buf, len, "%.1f", config->getTempAcceptableLow()); return true; }
    if (strcmp(key, "TEMP_ACCEPTABLE_HIGH") == 0) { snprintf(buf, len, "%.1f", config->getTempAcceptableHigh()); return true; }
    if (strcmp(key, "HUMI_COMFORTABLE_LOW") == 0) { snprintf(buf, len, "%.1f", config->getHumiComfortableLow()); return true; }
    if (strcmp(key, "HUMI_COMFORTABLE_HIGH") == 0) { snprintf(buf, len, "%.1f", config->getHumiComfortableHigh()); return true; }
    if (strcmp(key, "HUMI_ACCEPTABLE_LOW") == 0) { snprintf(buf, len, "%.1f", config->getHumiAcceptableLow()); return true; }
    if (strcmp(key, "HUMI_ACCEPTABLE_HIGH") == 0) { snprintf(buf, len, "%.1f", config->getHumiAcceptableHigh()); return true; }
    return false;
  });
  logger.info("Served climate config page");
}

void WebServerManager::handleConfigSystem() {
  ConfigManagerAccessor config;
  streamTemplate(config_system_html, [&](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    AppLogLevel currentLevel = config->getLogLevel();
    if (strcmp(key, "LOG_DEBUG_SEL") == 0) { if (currentLevel == APP_LOG_DEBUG) snprintf(buf, len, " selected"); return true; }
    if (strcmp(key, "LOG_INFO_SEL") == 0) { if (currentLevel == APP_LOG_INFO) snprintf(buf, len, " selected"); return true; }
    if (strcmp(key, "LOG_WARN_SEL") == 0) { if (currentLevel == APP_LOG_WARNING) snprintf(buf, len, " selected"); return true; }
    if (strcmp(key, "LOG_ERROR_SEL") == 0) { if (currentLevel == APP_LOG_ERROR) snprintf(buf, len, " selected"); return true; }
    uint8_t currentBrightness = UITask::getInstance().get_brightness();
    if (strcmp(key, "DISPLAY_BRIGHTNESS") == 0) { snprintf(buf, len, "%u", currentBrightness); return true; }
    if (strcmp(key, "BACKLIGHT_ON_SEL") == 0) { if (currentBrightness > 0) snprintf(buf, len, " selected"); return true; }
    if (strcmp(key, "BACKLIGHT_OFF_SEL") == 0) { if (currentBrightness == 0) snprintf(buf, len, " selected"); return true; }
    if (strcmp(key, "INACTIVITY_TIMER_DELAY") == 0) { snprintf(buf, len, "%u", config->getInactivityTimerDelay()); return true; }
    if (strcmp(key, "WIFI_SSID") == 0) { snprintf(buf, len, "%s", config->getWifiSsid()); return true; }
    if (strcmp(key, "WIFI_PASSWORD") == 0) { snprintf(buf, len, "%s", config->getWifiPassword()); return true; }
    if (strcmp(key, "MQTT_HOST") == 0) { snprintf(buf, len, "%s", config->getMqttHost()); return true; }
    if (strcmp(key, "MQTT_PORT") == 0) { snprintf(buf, len, "%u", config->getMqttPort()); return true; }
    if (strcmp(key, "MQTT_USER") == 0) { snprintf(buf, len, "%s", config->getMqttUser()); return true; }
    if (strcmp(key, "MQTT_PASSWORD") == 0) { snprintf(buf, len, "%s", config->getMqttPassword()); return true; }
    if (strcmp(key, "MQTT_DEVICE_ID") == 0) { snprintf(buf, len, "%s", config->getMqttDeviceId()); return true; }
    if (strcmp(key, "MQTT_DEVICE_NAME") == 0) { snprintf(buf, len, "%s", config->getMqttDeviceName()); return true; }
    return false;
  });
  logger.info("Served system config page");
}

void WebServerManager::handleConfigMaintenance() {
  HomeAssistantManager *ha = HomeAssistantManager::getInstance();
  bool autoCalState = ha ? ha->getScd30AutoCalState() : false;
  uint16_t forceCalValue = ha ? ha->getScd30ForceCalValue() : 400;
  if (forceCalValue == 0) forceCalValue = 400;

  streamTemplate(config_maintenance_html, [&](const char *key, char *buf, size_t len) -> bool {
    if (strcmp(key, "FIRMWARE_VERSION") == 0) {
      snprintf(buf, len, "%s", _firmware_version);
      return true;
    }
    if (strcmp(key, "SCD30_AUTOCAL_ON_SEL") == 0) {
      if (autoCalState) snprintf(buf, len, " selected");
      return true;
    }
    if (strcmp(key, "SCD30_AUTOCAL_OFF_SEL") == 0) {
      if (!autoCalState) snprintf(buf, len, " selected");
      return true;
    }
    if (strcmp(key, "SCD30_FORCECAL") == 0) {
      snprintf(buf, len, "%u", forceCalValue);
      return true;
    }
    if (strcmp(key, "SPS30_INFO") == 0) {
      buf[0] = '\0';
      return true;
    }
    if (strcmp(key, "SGP41_RESULT") == 0) {
      buf[0] = '\0';
      return true;
    }
    return false;
  });
  logger.info("Served sensor maintenance config page");
}

void WebServerManager::handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
  logger.warningf("HTTP 404 Not Found for request to: %s",
                  server.uri().c_str());
}

#include "WebServerManager.h"
#include "ConfigManager.h"
#include "HomeAssistantManager.h"
#include "Logger.h"
#include <Update.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "webserver/WebServerAssets.h"
#include "webserver/WebServerConfigClimate.h"
#include "webserver/WebServerConfigSystem.h"
#include "webserver/WebServerConfigTabs.h"
#include "webserver/WebServerConfigTabsExtra.h"

WebServer server(80);
String WebServerManager::_firmware_version;

const char *update_page_html =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HVAC Sensor OTA Update</title><link rel="stylesheet" href="/style.css?v=%FIRMWARE_VERSION%"></head><body><div class="container" style="text-align:center"><h1>HVAC Sensor OTA Firmware Update</h1><div class="nav"><a href="/">OTA Update</a><a href="/config">Configuration</a></div><form method="POST" action="/upload" enctype="multipart/form-data"><input type="file" name="update" accept=".bin"><br><input type="submit" value="Upload Firmware"></form><div class="footer"><p>Device: HVAC Sensor Display | Version: %FIRMWARE_VERSION% | IP: %IP_ADDRESS%</p></div></div></body></html>)rawliteral";

void WebServerManager::init(const char *firmware_version) {
  _firmware_version = firmware_version;

  // Static assets (cached by browser with ETag matching firmware version)
  server.on("/style.css", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "public, max-age=86400");
    server.sendHeader("ETag", _firmware_version);
    server.send_P(200, "text/css", style_css);
  });
  server.on("/app.js", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "public, max-age=86400");
    server.sendHeader("ETag", _firmware_version);
    server.send_P(200, "application/javascript", app_js);
  });
  server.on("/favicon.ico", HTTP_GET, []() { server.send(204); });

  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_GET, handleConfig);

  server.on("/config/gas", HTTP_GET, handleConfigGas);
  server.on("/config/pm", HTTP_GET, handleConfigPM);
  server.on("/config/hvac", HTTP_GET, handleConfigHVAC);
  server.on("/config/climate", HTTP_GET, handleConfigClimate);
  server.on("/config/system", HTTP_GET, handleConfigSystem);
  server.on("/config/update", HTTP_POST, handleConfigUpdate);

  server.on(
      "/upload", HTTP_POST,
      []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "Update complete! Rebooting...");
        delay(500);
        ESP.restart();
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
  String html;
  html.reserve(1024);
  html = update_page_html;
  html.replace("%IP_ADDRESS%", WiFi.localIP().toString());
  html.replace("%FIRMWARE_VERSION%", _firmware_version);

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
  logger.debug("Served OTA update web page.");
}

void WebServerManager::handleConfig() {
  size_t freeHeap = ESP.getFreeHeap();

  String html;
  html.reserve(2048);
  html = config_main_html;
  html.replace("%IP_ADDRESS%", WiFi.localIP().toString());
  html.replace("%FREE_HEAP%", String(freeHeap));
  html.replace("%FIRMWARE_VERSION%", _firmware_version);

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
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

    delay(2000);
    ESP.restart();
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

    delay(2000);
    ESP.restart();
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
      if (type == "timer_delay")
        config->setInactivityTimerDelay(intVal);
      else
        ok = false;
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
  String html;
  html.reserve(3072);
  html = config_gas_html;
  html.replace("%FIRMWARE_VERSION%", _firmware_version);

  html.replace("%NO2_WARN%", String(config->getNo2WarnThreshold()));
  html.replace("%NO2_DANGER%", String(config->getNo2DangerThreshold()));
  html.replace("%O3_WARN%", String(config->getO3WarnThreshold()));
  html.replace("%O3_DANGER%", String(config->getO3DangerThreshold()));
  html.replace("%NOX_WARN%", String(config->getNoxWarnThreshold()));
  html.replace("%NOX_DANGER%", String(config->getNoxDangerThreshold()));
  html.replace("%CO_WARN%", String(config->getCoWarnThreshold()));
  html.replace("%CO_DANGER%", String(config->getCoDangerThreshold()));
  html.replace("%CO2_WARN%", String(config->getCo2WarnThreshold()));
  html.replace("%CO2_DANGER%", String(config->getCo2DangerThreshold()));
  html.replace("%VOC_WARN%", String(config->getVocWarnThreshold()));
  html.replace("%VOC_DANGER%", String(config->getVocDangerThreshold()));

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
  logger.info("Served gas config page");
}

void WebServerManager::handleConfigPM() {
  ConfigManagerAccessor config;
  String html;
  html.reserve(2560);
  html = config_pm_html;
  html.replace("%FIRMWARE_VERSION%", _firmware_version);

  html.replace("%PM1_WARN%", String(config->getPm1WarnThreshold()));
  html.replace("%PM1_DANGER%", String(config->getPm1DangerThreshold()));
  html.replace("%PM25_WARN%", String(config->getPm25WarnThreshold()));
  html.replace("%PM25_DANGER%", String(config->getPm25DangerThreshold()));
  html.replace("%PM4_WARN%", String(config->getPm4WarnThreshold()));
  html.replace("%PM4_DANGER%", String(config->getPm4DangerThreshold()));
  html.replace("%PM10_WARN%", String(config->getPm10WarnThreshold()));
  html.replace("%PM10_DANGER%", String(config->getPm10DangerThreshold()));

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
  logger.info("Served PM config page");
}

void WebServerManager::handleConfigHVAC() {
  ConfigManagerAccessor config;
  String html;
  html.reserve(4096);
  html = config_hvac_html;
  html.replace("%FIRMWARE_VERSION%", _firmware_version);

  html.replace("%PRESSURE_LOW%", String(config->getPressureLowThreshold(), 1));
  html.replace("%PRESSURE_MID%", String(config->getPressureMidThreshold(), 1));
  html.replace("%PRESSURE_HIGH%",
               String(config->getHighPressureThreshold(), 1));
  html.replace("%FAN_ON%", String(config->getFanOnCurrentThreshold(), 1));
  html.replace("%FAN_OFF%", String(config->getFanOffCurrentThreshold(), 1));
  html.replace("%FAN_HIGH%", String(config->getFanHighCurrentThreshold(), 1));
  html.replace("%COMPRESSOR_ON%",
               String(config->getCompressorOnCurrentThreshold(), 1));
  html.replace("%COMPRESSOR_OFF%",
               String(config->getCompressorOffCurrentThreshold(), 1));
  html.replace("%COMPRESSOR_HIGH%",
               String(config->getCompressorHighCurrentThreshold(), 1));
  html.replace("%PUMP_ON%", String(config->getPumpOnCurrentThreshold(), 1));
  html.replace("%PUMP_OFF%", String(config->getPumpOffCurrentThreshold(), 1));
  html.replace("%PUMP_HIGH%", String(config->getPumpHighCurrentThreshold(), 1));
  html.replace("%GEIGER_ABNORMAL_LOW%",
               String(config->getGeigerAbnormalLowThreshold(), 2));
  html.replace("%GEIGER_ABNORMAL_HIGH%",
               String(config->getGeigerAbnormalHighThreshold(), 2));
  html.replace("%GEIGER_DANGER_HIGH%",
               String(config->getGeigerDangerHighThreshold(), 2));

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
  logger.info("Served HVAC config page");
}

void WebServerManager::handleConfigClimate() {
  ConfigManagerAccessor config;
  String html;
  html.reserve(2560);
  html = config_climate_html;
  html.replace("%FIRMWARE_VERSION%", _firmware_version);

  html.replace("%TEMP_COMFORTABLE_LOW%",
               String(config->getTempComfortableLow(), 1));
  html.replace("%TEMP_COMFORTABLE_HIGH%",
               String(config->getTempComfortableHigh(), 1));
  html.replace("%TEMP_ACCEPTABLE_LOW%",
               String(config->getTempAcceptableLow(), 1));
  html.replace("%TEMP_ACCEPTABLE_HIGH%",
               String(config->getTempAcceptableHigh(), 1));
  html.replace("%HUMI_COMFORTABLE_LOW%",
               String(config->getHumiComfortableLow(), 1));
  html.replace("%HUMI_COMFORTABLE_HIGH%",
               String(config->getHumiComfortableHigh(), 1));
  html.replace("%HUMI_ACCEPTABLE_LOW%",
               String(config->getHumiAcceptableLow(), 1));
  html.replace("%HUMI_ACCEPTABLE_HIGH%",
               String(config->getHumiAcceptableHigh(), 1));

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
  logger.info("Served climate config page");
}

void WebServerManager::handleConfigSystem() {
  ConfigManagerAccessor config;
  String html;
  html.reserve(3072);
  html = config_system_html;
  html.replace("%FIRMWARE_VERSION%", _firmware_version);

  html.replace("%INACTIVITY_TIMER_DELAY%",
               String(config->getInactivityTimerDelay()));
  html.replace("%WIFI_SSID%", String(config->getWifiSsid()));
  html.replace("%WIFI_PASSWORD%", String(config->getWifiPassword()));
  html.replace("%MQTT_HOST%", String(config->getMqttHost()));
  html.replace("%MQTT_PORT%", String(config->getMqttPort()));
  html.replace("%MQTT_USER%", String(config->getMqttUser()));
  html.replace("%MQTT_PASSWORD%", String(config->getMqttPassword()));
  html.replace("%MQTT_DEVICE_ID%", String(config->getMqttDeviceId()));
  html.replace("%MQTT_DEVICE_NAME%", String(config->getMqttDeviceName()));

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
  logger.info("Served system config page");
}

void WebServerManager::handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
  logger.warningf("HTTP 404 Not Found for request to: %s",
                  server.uri().c_str());
}

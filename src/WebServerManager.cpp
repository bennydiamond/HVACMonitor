#include "WebServerManager.h"
#include "BootloopGuard.h"
#include "ConfigManager.h"
#include "HomeAssistantManager.h"
#include "Logger.h"
#include "RebootManager.h"
#include <Update.h>
#include <WiFi.h>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <esp_flash.h>
#include <esp_image_format.h>
#include <esp_task_wdt.h>

#include "LiveDashboardData.h"
#include "MainTaskEvents.h"
#include "NanoCommands.h"
#include "NanoFlasher.h"
#include "SensorDefines.h"
#include "UITask.h"
#include "webserver/WebServerAssets.h"
#include "webserver/WebServerConfigClimate.h"
#include "webserver/WebServerConfigMaintenance.h"
#include "webserver/WebServerConfigSystem.h"
#include "webserver/WebServerConfigTabs.h"
#include "webserver/WebServerConfigTabsExtra.h"
#include "webserver/WebServerDashboard.h"
#include "webserver/WebServerOTA.h"

// Forward declaration from ESP-IDF libspi_flash.a (spi_flash_os_func_app.c)
extern "C" esp_err_t
esp_flash_set_dangerous_write_protection(esp_flash_t *chip, const bool protect);

WebServer server(80);
char WebServerManager::_firmware_version[16] = {0};

static bool s_is_bootloader_update = false;
static uint8_t *s_bootloader_buf = nullptr;
static size_t s_bootloader_received = 0;
static bool s_bootloader_failed = false;

static bool s_is_nano_update = false;
static bool s_nano_failed = false;
static NanoFlasher::Uploader *s_nano_uploader = nullptr;

static bool s_app_header_checked = false;
static uint8_t s_app_header_buf[0x24] = {0};
static size_t s_app_header_len = 0;

enum class UploadState { Idle, InProgress, Success, Failed, Aborted };

static UploadState s_upload_state = UploadState::Idle;
static size_t s_upload_bytes_received = 0;
static String s_upload_error_msg = "";

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

  auto write_escaped_html = [&](const char *s) {
    const char *start = s;
    while (*s) {
      const char *esc = nullptr;
      size_t esc_len = 0;
      switch (*s) {
      case '&':
        esc = "&amp;";
        esc_len = 5;
        break;
      case '\"':
        esc = "&quot;";
        esc_len = 6;
        break;
      case '\'':
        esc = "&#39;";
        esc_len = 5;
        break;
      case '<':
        esc = "&lt;";
        esc_len = 4;
        break;
      case '>':
        esc = "&gt;";
        esc_len = 4;
        break;
      default:
        break;
      }
      if (esc) {
        if (s > start) {
          write_str(start, s - start);
        }
        write_str(esc, esc_len);
        start = s + 1;
      }
      s++;
    }
    if (s > start) {
      write_str(start, s - start);
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
        write_escaped_html(val_buf);
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
  strncpy(_firmware_version, firmware_version ? firmware_version : "",
          sizeof(_firmware_version) - 1);
  _firmware_version[sizeof(_firmware_version) - 1] = '\0';

  // Static assets (no-cache ensures browsers always check for latest asset
  // versions)
  server.on("/style.css", HTTP_GET, []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("ETag", _firmware_version);
    server.send_P(200, "text/css", (const char *)style_css_gz,
                  style_css_gz_len);
  });
  server.on("/app.js", HTTP_GET, []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("ETag", _firmware_version);
    server.send_P(200, "application/javascript", (const char *)app_js_gz,
                  app_js_gz_len);
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
        UploadState state = s_upload_state;

        // Reset state for future requests
        s_upload_state = UploadState::Idle;
        s_upload_bytes_received = 0;
        s_app_header_checked = false;
        s_app_header_len = 0;

        switch (state) {
        case UploadState::Idle:
          if (Update.isRunning()) {
            Update.abort();
          }
          server.send(400, "text/plain",
                      "No firmware file uploaded or empty payload");
          return;

        case UploadState::Aborted:
          if (Update.isRunning()) {
            Update.abort();
          }
          server.send(400, "text/plain",
                      s_upload_error_msg.length() > 0
                          ? s_upload_error_msg
                          : "Firmware upload aborted by client");
          return;

        case UploadState::InProgress:
          if (Update.isRunning()) {
            Update.abort();
          }
          server.send(400, "text/plain",
                      "Firmware upload interrupted prematurely");
          return;

        case UploadState::Failed:
          if (!s_is_nano_update && !s_is_bootloader_update &&
              Update.isRunning()) {
            Update.abort();
          }
          server.send(400, "text/plain",
                      s_upload_error_msg.length() > 0 ? s_upload_error_msg
                                                      : "Flash update failed");
          return;

        case UploadState::Success:
          if (s_is_nano_update) {
            server.send(200, "text/plain",
                        "Arduino Nano firmware updated successfully! "
                        "Reconnecting sensor hub...");
          } else {
            const char *targetName =
                s_is_bootloader_update ? "Bootloader" : "Firmware";
            String successMsg =
                String(targetName) + " updated successfully! Rebooting...";
            server.send(200, "text/plain", successMsg);
            RebootManager::getInstance().requestReboot(
                "Web OTA update complete", 500);
          }
          return;
        }
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
  streamTemplate(
      dashboard_page_html, [](const char *key, char *buf, size_t len) -> bool {
        if (strcmp(key, "FIRMWARE_VERSION") == 0) {
          snprintf(buf, len, "%s", _firmware_version);
          return true;
        }
        if (strcmp(key, "FLASH_INFO") == 0) {
          FlashMode_t fMode = ESP.getFlashChipMode();
          const char *fModeStr = (fMode == FM_QIO)    ? "QIO"
                                 : (fMode == FM_QOUT) ? "QOUT"
                                 : (fMode == FM_DIO)  ? "DIO"
                                 : (fMode == FM_DOUT) ? "DOUT"
                                                      : "UNKNOWN";
          snprintf(buf, len, "%lu MHz / %lu MHz %s",
                   (unsigned long)getCpuFrequencyMhz(),
                   (unsigned long)(ESP.getFlashChipSpeed() / 1000000),
                   fModeStr);
          return true;
        }
        return false;
      });
  logger.debug("Served live dashboard web page.");
}

void WebServerManager::handleOTA() {
  streamTemplate(ota_page_html,
                 [](const char *key, char *buf, size_t len) -> bool {
                   if (strcmp(key, "FIRMWARE_VERSION") == 0) {
                     snprintf(buf, len, "%s", _firmware_version);
                     return true;
                   }
                   return false;
                 });
  logger.debug("Served OTA update web page.");
}

static void appendJsonString(String &out, const char *src) {
  out += '"';
  if (src) {
    while (*src) {
      char c = *src++;
      switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if ((uint8_t)c < 0x20) {
          char uhex[7];
          snprintf(uhex, sizeof(uhex), "\\u%04x", (uint8_t)c);
          out += uhex;
        } else {
          out += c;
        }
        break;
      }
    }
  }
  out += '"';
}

static void appendFloat(String &out, float val, int decimals, float fallback) {
  float safeVal = (std::isnan(val) || std::isinf(val)) ? fallback : val;
  char buf[32];
  snprintf(buf, sizeof(buf), "%.*f", decimals, safeVal);
  out += buf;
}

void WebServerManager::handleApiStatus() {
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 8000) {
    logger.warningf("Low memory (%u bytes), rejecting /api/status", freeHeap);
    server.send(503, "application/json", "{\"error\":\"low memory\"}");
    return;
  }

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
  float temp_cl = 15.0f, temp_ch = 25.0f, temp_al = 10.0f, temp_ah = 30.0f;
  float humi_cl = 30.0f, humi_ch = 60.0f, humi_al = 20.0f, humi_ah = 70.0f;

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
    temp_cl = config->getTempComfortableLow();
    temp_ch = config->getTempComfortableHigh();
    temp_al = config->getTempAcceptableLow();
    temp_ah = config->getTempAcceptableHigh();
    humi_cl = config->getHumiComfortableLow();
    humi_ch = config->getHumiComfortableHigh();
    humi_al = config->getHumiAcceptableLow();
    humi_ah = config->getHumiAcceptableHigh();
  }

  String out;
  out.reserve(1500);

  // Section: iaq
  out += "{\"iaq\":{\"epa_aqi\":";
  out += data.epa_aqi;
  out += ",\"fast_aqi\":";
  out += data.fast_aqi;
  out += ",\"co2\":";
  appendFloat(out, data.co2_ppm, 1, -1.0f);
  out += ",\"voc\":";
  out += (long)data.voc_index;
  out += ",\"nox\":";
  out += (long)data.nox_index;
  out += ",\"pm1\":";
  appendFloat(out, data.pm1_ug, 1, -1.0f);
  out += ",\"pm25\":";
  appendFloat(out, data.pm25_ug, 1, -1.0f);
  out += ",\"pm4\":";
  appendFloat(out, data.pm4_ug, 1, -1.0f);
  out += ",\"pm10\":";
  appendFloat(out, data.pm10_ug, 1, -1.0f);
  out += ",\"t\":";
  appendFloat(out, data.temperature_c, 1, -999.0f);
  out += ",\"h\":";
  appendFloat(out, data.humidity_pct, 1, -1.0f);
  out += ",\"baro\":";
  appendFloat(out, data.baro_pressure_hpa, 1, -1.0f);
  out += ",\"co\":";
  out += data.co_ppm;
  out += ",\"o3\":";
  appendFloat(out, data.o3_ug, 1, -1.0f);
  out += ",\"no2\":";
  appendFloat(out, data.no2_ug, 1, -1.0f);
  out += ",\"usv\":";
  appendFloat(out, data.geiger_usv_h, 2, -1.0f);
  out += ",\"cpm\":";
  out += data.geiger_cpm;
  out += "},";

  // Section: hvac
  out += "\"hvac\":{\"fp\":";
  appendFloat(out, data.filter_pressure_pa, 1, 0.0f);
  out += ",\"dp\":";
  appendFloat(out, data.diff_pressure_pa, 1, 0.0f);
  out += ",\"hp\":";
  out += data.is_pressure_high ? "true" : "false";
  out += ",\"fa\":";
  appendFloat(out, data.fan_amps, 2, 0.0f);
  out += ",\"fs\":";
  out += (unsigned int)data.fan_status;
  out += ",\"ca\":";
  appendFloat(out, data.compressor_amps, 2, 0.0f);
  out += ",\"pa\":";
  appendFloat(out, data.pump_amps, 2, 0.0f);
  out += ",\"water\":";
  out += data.water_detected ? "true" : "false";
  out += "},";

  // Section: sys
  out += "\"sys\":{\"ssid\":";
  appendJsonString(out, data.wifi_ssid);
  out += ",\"ip\":";
  appendJsonString(out, data.wifi_ip);
  out += ",\"rssi\":";
  out += (int)data.wifi_rssi;
  out += ",\"mac\":";
  appendJsonString(out, data.wifi_mac);
  out += ",\"wc\":";
  out += data.wifi_connected ? "true" : "false";
  out += ",\"mh\":";
  appendJsonString(out, data.mqtt_host);
  out += ",\"mp\":";
  out += data.mqtt_port;
  out += ",\"mc\":";
  out += data.mqtt_connected ? "true" : "false";
  out += ",\"nv\":";
  appendJsonString(out, data.nano_version);
  out += ",\"dsp\":";
  out += (unsigned long)data.seconds_since_packet;
  out += ",\"nu\":";
  out += (unsigned long)data.nano_uptime_seconds;
  out += ",\"sc\":";
  out += data.sensor_module_connected ? "true" : "false";
  out += ",\"nrc\":";
  appendJsonString(out, data.nano_reset_cause);
  out += ",\"nr\":";
  out += (unsigned int)data.nano_free_ram;
  out += ",\"nfw\":";
  out += (unsigned int)data.sensorstack_firmware_state;
  out += ",\"nsf\":";
  out += (unsigned int)data.sensorstack_sensor_faults;
  out += "},";

  // Section: diag
  FlashMode_t fMode = ESP.getFlashChipMode();
  const char *fModeStr = (fMode == FM_QIO)    ? "QIO"
                         : (fMode == FM_QOUT) ? "QOUT"
                         : (fMode == FM_DIO)  ? "DIO"
                         : (fMode == FM_DOUT) ? "DOUT"
                                              : "UNKNOWN";

  out += "\"diag\":{\"heap\":";
  out += (unsigned long)data.esp32_free_heap;
  out += ",\"min_heap\":";
  out += (unsigned long)data.esp32_min_free_heap;
  out += ",\"max_alloc\":";
  out += (unsigned long)data.esp32_max_alloc_heap;
  out += ",\"uptime\":";
  out += (unsigned long)data.esp32_uptime_seconds;
  out += ",\"reset\":";
  appendJsonString(out, data.esp32_reset_reason);
  out += ",\"ver\":";
  appendJsonString(out, data.firmware_version);
  out += ",\"cpu\":";
  out += (unsigned long)getCpuFrequencyMhz();
  out += ",\"flash_spd\":";
  out += (unsigned long)(ESP.getFlashChipSpeed() / 1000000);
  out += ",\"flash_mode\":";
  appendJsonString(out, fModeStr);
  out += "},";

  // Section: th
  out += "\"th\":{\"co2_w\":";
  out += co2_w;
  out += ",\"co2_d\":";
  out += co2_d;
  out += ",\"voc_w\":";
  out += voc_w;
  out += ",\"voc_d\":";
  out += voc_d;
  out += ",\"nox_w\":";
  out += nox_w;
  out += ",\"nox_d\":";
  out += nox_d;
  out += ",\"pm1_w\":";
  out += pm1_w;
  out += ",\"pm1_d\":";
  out += pm1_d;
  out += ",\"pm25_w\":";
  out += pm25_w;
  out += ",\"pm25_d\":";
  out += pm25_d;
  out += ",\"pm4_w\":";
  out += pm4_w;
  out += ",\"pm4_d\":";
  out += pm4_d;
  out += ",\"pm10_w\":";
  out += pm10_w;
  out += ",\"pm10_d\":";
  out += pm10_d;
  out += ",\"co_w\":";
  out += co_w;
  out += ",\"co_d\":";
  out += co_d;
  out += ",\"o3_w\":";
  out += o3_w;
  out += ",\"o3_d\":";
  out += o3_d;
  out += ",\"no2_w\":";
  out += no2_w;
  out += ",\"no2_d\":";
  out += no2_d;
  out += ",\"fan_off\":";
  appendFloat(out, fan_off, 1, 0.5f);
  out += ",\"t_cl\":";
  appendFloat(out, temp_cl, 1, 15.0f);
  out += ",\"t_ch\":";
  appendFloat(out, temp_ch, 1, 25.0f);
  out += ",\"t_al\":";
  appendFloat(out, temp_al, 1, 10.0f);
  out += ",\"t_ah\":";
  appendFloat(out, temp_ah, 1, 30.0f);
  out += ",\"humi_cl\":";
  appendFloat(out, humi_cl, 1, 30.0f);
  out += ",\"humi_ch\":";
  appendFloat(out, humi_ch, 1, 60.0f);
  out += ",\"humi_al\":";
  appendFloat(out, humi_al, 1, 20.0f);
  out += ",\"humi_ah\":";
  appendFloat(out, humi_ah, 1, 70.0f);
  out += "}}";

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", out);
}

void WebServerManager::handleConfig() {
  size_t freeHeap = ESP.getFreeHeap();

  streamTemplate(config_main_html,
                 [](const char *key, char *buf, size_t len) -> bool {
                   if (strcmp(key, "FIRMWARE_VERSION") == 0) {
                     snprintf(buf, len, "%s", _firmware_version);
                     return true;
                   }
                   return false;
                 });
  logger.infof("Served main config page. Free: %u, Min: %u, Max: %u bytes",
               freeHeap, ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
}

static bool parseStrictInt(const String &str, int &outVal, int minVal, int maxVal) {
  if (str.length() == 0) return false;
  const char *p = str.c_str();
  while (*p == ' ' || *p == '\t') p++;
  if (*p == '\0') return false;
  char *endptr = nullptr;
  errno = 0;
  long val = strtol(p, &endptr, 10);
  if (errno == ERANGE || endptr == p) return false;
  while (*endptr == ' ' || *endptr == '\t' || *endptr == '\r' || *endptr == '\n') endptr++;
  if (*endptr != '\0') return false;
  if (val < minVal || val > maxVal) return false;
  outVal = static_cast<int>(val);
  return true;
}

static bool parseStrictFloat(const String &str, float &outVal, float minVal, float maxVal) {
  if (str.length() == 0) return false;
  const char *p = str.c_str();
  while (*p == ' ' || *p == '\t') p++;
  if (*p == '\0') return false;
  char *endptr = nullptr;
  errno = 0;
  float val = strtof(p, &endptr);
  if (errno == ERANGE || endptr == p) return false;
  while (*endptr == ' ' || *endptr == '\t' || *endptr == '\r' || *endptr == '\n') endptr++;
  if (*endptr != '\0') return false;
  if (std::isnan(val) || std::isinf(val)) return false;
  if (val < minVal || val > maxVal) return false;
  outVal = val;
  return true;
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
    bool isOpen =
        server.hasArg("open_network") && (server.arg("open_network") == "1" ||
                                          server.arg("open_network") == "true");

    ssid.trim();
    if (ssid.length() == 0) {
      server.send(400, "text/plain", "SSID cannot be empty");
      return;
    }
    if (ssid.length() > 32) {
      server.send(400, "text/plain", "SSID cannot exceed 32 characters");
      return;
    }
    if (isOpen) {
      password = "";
    } else if (password.length() > 0) {
      if (password.length() < 8 || password.length() > 64) {
        server.send(
            400, "text/plain",
            "Password must be 8 to 64 characters (or check open network)");
        return;
      }
    }

    bool updatePassword = isOpen || (password.length() > 0);

    {
      ConfigManagerAccessor config;
      if (!config->setWifiCredentials(
              ssid.c_str(), isOpen ? "" : password.c_str(), updatePassword)) {
        server.send(
            400, "text/plain",
            "Failed to save Wi-Fi configuration (invalid SSID or password)");
        return;
      }
    }

    logger.infof("Wi-Fi configuration saved (SSID: %s%s). Rebooting device...",
                 ssid.c_str(), isOpen ? ", open network" : "");
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain",
                "Wi-Fi configuration saved successfully. Rebooting device...");

    RebootManager::getInstance().requestReboot("Wi-Fi configuration updated",
                                               2000);
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

    host.trim();
    user.trim();
    deviceId.trim();
    deviceName.trim();

    if (host.length() == 0 || user.length() == 0 || deviceId.length() == 0 ||
        deviceName.length() == 0) {
      server.send(400, "text/plain",
                  "Host, user, device ID, and device name are required");
      return;
    }

    if (host.length() > 63 || user.length() > 31 || deviceId.length() > 31 ||
        deviceName.length() > 63) {
      server.send(400, "text/plain",
                  "One or more fields exceed maximum length");
      return;
    }

    if (password.length() > 63) {
      server.send(400, "text/plain", "Password cannot exceed 63 characters");
      return;
    }

    int portNum = 0;
    if (!parseStrictInt(port, portNum, 1, 65535)) {
      server.send(400, "text/plain", "Invalid port number (1-65535)");
      return;
    }

    {
      ConfigManagerAccessor config;
      if (password.length() == 0 && strlen(config->getMqttPassword()) == 0) {
        server.send(400, "text/plain", "MQTT password is required");
        return;
      }
      config->setMqttHost(host.c_str());
      config->setMqttPort(portNum);
      config->setMqttUser(user.c_str());
      if (password.length() > 0) {
        config->setMqttPassword(password.c_str());
      }
      config->setMqttDeviceId(deviceId.c_str());
      config->setMqttDeviceName(deviceName.c_str());
    }

    logger.info("MQTT device configuration saved. Rebooting device...");
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain",
                "Configuration saved successfully. Rebooting device...");

    RebootManager::getInstance().requestReboot("MQTT configuration updated",
                                               2000);
    return;
  }

  // Action requests (button triggers)
  if (server.hasArg("action")) {
    String action = server.arg("action");
    if (action == "sps30_clean") {
      logger.info("SPS30 manual fan clean requested via Web Server");
      MainTaskEventNotifier::getInstance().sendEvent(
          MainTaskEventNotifier::EVT_SPS30_CLEAN);
      server.send(200, "text/plain", "SPS30 fan clean started (10s cycle)");
      return;
    }
    if (action == "sps30_info") {
      logger.info("SPS30 info query requested via Web Server");
      if (query_sps30_info_blocking(3000)) {
        const SPS30DeviceInfo &info = get_sps30_device_info();
        char resp[128];
        snprintf(resp, sizeof(resp),
                 "Firmware: v%u.%u | Clean Interval: %lus (%lud) | Status: "
                 "0x%08lX (%s)",
                 info.fw_major, info.fw_minor, info.fan_interval, info.fan_days,
                 info.status_reg,
                 info.status_reg == 0 ? "OK" : "Fan Speed Warning");
        server.send(200, "text/plain", resp);
      } else {
        server.send(504, "text/plain",
                    "Timeout waiting for SPS30 info from sensor stack");
      }
      return;
    }
    if (action == "sgp41_test") {
      logger.info("SGP41 self-test requested via Web Server");
      if (run_sgp41_test_blocking(4000)) {
        const SGP41TestResult &res = get_sgp41_test_result();
        char resp[96];
        if (res.ret_status == 0 && res.raw_value == 0xD400) {
          snprintf(resp, sizeof(resp), "OK / Passed (Value: 0x%04X, Status: 0)",
                   res.raw_value);
        } else {
          snprintf(resp, sizeof(resp), "Failed (Status: %d, Value: 0x%04X)",
                   res.ret_status, res.raw_value);
        }
        server.send(200, "text/plain", resp);
      } else {
        server.send(504, "text/plain",
                    "Timeout waiting for SGP41 test from sensor stack");
      }
      return;
    }
    if (action == "reboot_nano") {
      logger.warning("Sensor Hub (Nano) reboot requested via Web Server");
      MainTaskEventNotifier::getInstance().sendEvent(
          MainTaskEventNotifier::EVT_REBOOT_NANO);
      server.send(200, "text/plain", "Sensor Hub reboot command sent");
      return;
    }
    if (action == "reboot_esp32") {
      logger.warning("ESP32 reboot requested via Web Server");
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", "Device rebooting in 2 seconds...");
      RebootManager::getInstance().requestReboot("Web System page reboot",
                                                 2000);
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

    int intVal = 0;
    float floatVal = 0.0f;
    const char *errorMsg = "Invalid value or out of allowed range";

    ConfigManagerAccessor config;
    bool ok = true;

    auto validateWarnDanger = [&](int warnVal, int dangerVal, bool isWarn) -> bool {
      if (isWarn && warnVal > dangerVal) {
        errorMsg = "Warning threshold cannot exceed Danger threshold";
        return false;
      }
      if (!isWarn && dangerVal < warnVal) {
        errorMsg = "Danger threshold cannot be lower than Warning threshold";
        return false;
      }
      return true;
    };

    auto validateOffOnHigh = [&](float offVal, float onVal, float highVal, const String &curType) -> bool {
      if (curType == "off" && offVal > onVal) {
        errorMsg = "Off threshold cannot exceed On threshold";
        return false;
      }
      if (curType == "on" && (onVal < offVal || onVal > highVal)) {
        errorMsg = "On threshold must be between Off and High thresholds";
        return false;
      }
      if (curType == "high" && highVal < onVal) {
        errorMsg = "High threshold cannot be lower than On threshold";
        return false;
      }
      return true;
    };

    if (section == "no2") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(intVal, config->getNo2DangerThreshold(), true))
          config->setNo2WarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(config->getNo2WarnThreshold(), intVal, false))
          config->setNo2DangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "o3") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(intVal, config->getO3DangerThreshold(), true))
          config->setO3WarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(config->getO3WarnThreshold(), intVal, false))
          config->setO3DangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "nox") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(intVal, config->getNoxDangerThreshold(), true))
          config->setNoxWarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(config->getNoxWarnThreshold(), intVal, false))
          config->setNoxDangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "co") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 100)) {
        if (validateWarnDanger(intVal, config->getCoDangerThreshold(), true))
          config->setCoWarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 100)) {
        if (validateWarnDanger(config->getCoWarnThreshold(), intVal, false))
          config->setCoDangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "pm1") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(intVal, config->getPm1DangerThreshold(), true))
          config->setPm1WarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(config->getPm1WarnThreshold(), intVal, false))
          config->setPm1DangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "pm25") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(intVal, config->getPm25DangerThreshold(), true))
          config->setPm25WarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(config->getPm25WarnThreshold(), intVal, false))
          config->setPm25DangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "pm4") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(intVal, config->getPm4DangerThreshold(), true))
          config->setPm4WarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(config->getPm4WarnThreshold(), intVal, false))
          config->setPm4DangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "pm10") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(intVal, config->getPm10DangerThreshold(), true))
          config->setPm10WarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(config->getPm10WarnThreshold(), intVal, false))
          config->setPm10DangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "co2") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 10000)) {
        if (validateWarnDanger(intVal, config->getCo2DangerThreshold(), true))
          config->setCo2WarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 10000)) {
        if (validateWarnDanger(config->getCo2WarnThreshold(), intVal, false))
          config->setCo2DangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "pressure") {
      if (type == "low" && parseStrictFloat(valueStr, floatVal, 0.0f, 1000.0f)) {
        if (floatVal > config->getPressureMidThreshold()) {
          ok = false;
          errorMsg = "Low threshold cannot exceed Mid threshold";
        } else {
          config->setPressureLowThreshold(floatVal);
        }
      } else if (type == "mid" && parseStrictFloat(valueStr, floatVal, 0.0f, 1000.0f)) {
        if (floatVal < config->getPressureLowThreshold() || floatVal > config->getHighPressureThreshold()) {
          ok = false;
          errorMsg = "Mid threshold must be between Low and High thresholds";
        } else {
          config->setPressureMidThreshold(floatVal);
        }
      } else if (type == "high" && parseStrictFloat(valueStr, floatVal, 0.0f, 1000.0f)) {
        if (floatVal < config->getPressureMidThreshold()) {
          ok = false;
          errorMsg = "High threshold cannot be lower than Mid threshold";
        } else {
          config->setHighPressureThreshold(floatVal);
        }
      } else {
        ok = false;
      }
    } else if (section == "fan") {
      if (type == "off" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(floatVal, config->getFanOnCurrentThreshold(), config->getFanHighCurrentThreshold(), "off"))
          config->setFanOffCurrentThreshold(floatVal);
        else
          ok = false;
      } else if (type == "on" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(config->getFanOffCurrentThreshold(), floatVal, config->getFanHighCurrentThreshold(), "on"))
          config->setFanOnCurrentThreshold(floatVal);
        else
          ok = false;
      } else if (type == "high" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(config->getFanOffCurrentThreshold(), config->getFanOnCurrentThreshold(), floatVal, "high"))
          config->setFanHighCurrentThreshold(floatVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "compressor") {
      if (type == "off" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(floatVal, config->getCompressorOnCurrentThreshold(), config->getCompressorHighCurrentThreshold(), "off"))
          config->setCompressorOffCurrentThreshold(floatVal);
        else
          ok = false;
      } else if (type == "on" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(config->getCompressorOffCurrentThreshold(), floatVal, config->getCompressorHighCurrentThreshold(), "on"))
          config->setCompressorOnCurrentThreshold(floatVal);
        else
          ok = false;
      } else if (type == "high" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(config->getCompressorOffCurrentThreshold(), config->getCompressorOnCurrentThreshold(), floatVal, "high"))
          config->setCompressorHighCurrentThreshold(floatVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "pump") {
      if (type == "off" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(floatVal, config->getPumpOnCurrentThreshold(), config->getPumpHighCurrentThreshold(), "off"))
          config->setPumpOffCurrentThreshold(floatVal);
        else
          ok = false;
      } else if (type == "on" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(config->getPumpOffCurrentThreshold(), floatVal, config->getPumpHighCurrentThreshold(), "on"))
          config->setPumpOnCurrentThreshold(floatVal);
        else
          ok = false;
      } else if (type == "high" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (validateOffOnHigh(config->getPumpOffCurrentThreshold(), config->getPumpOnCurrentThreshold(), floatVal, "high"))
          config->setPumpHighCurrentThreshold(floatVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "geiger") {
      if (type == "abnormal_low" && parseStrictFloat(valueStr, floatVal, 0.0f, 10.0f)) {
        if (floatVal > config->getGeigerAbnormalHighThreshold()) {
          ok = false;
          errorMsg = "Abnormal Low cannot exceed Abnormal High threshold";
        } else {
          config->setGeigerAbnormalLowThreshold(floatVal);
        }
      } else if (type == "abnormal_high" && parseStrictFloat(valueStr, floatVal, 0.0f, 10.0f)) {
        if (floatVal < config->getGeigerAbnormalLowThreshold() || floatVal > config->getGeigerDangerHighThreshold()) {
          ok = false;
          errorMsg = "Abnormal High must be between Abnormal Low and Danger High thresholds";
        } else {
          config->setGeigerAbnormalHighThreshold(floatVal);
        }
      } else if (type == "danger_high" && parseStrictFloat(valueStr, floatVal, 0.0f, 10.0f)) {
        if (floatVal < config->getGeigerAbnormalHighThreshold()) {
          ok = false;
          errorMsg = "Danger High cannot be lower than Abnormal High threshold";
        } else {
          config->setGeigerDangerHighThreshold(floatVal);
        }
      } else {
        ok = false;
      }
    } else if (section == "temp") {
      if (type == "acceptable_low" && parseStrictFloat(valueStr, floatVal, -50.0f, 50.0f)) {
        if (floatVal > config->getTempComfortableLow()) {
          ok = false;
          errorMsg = "Acceptable Low cannot exceed Comfortable Low";
        } else {
          config->setTempAcceptableLow(floatVal);
        }
      } else if (type == "comfortable_low" && parseStrictFloat(valueStr, floatVal, -50.0f, 50.0f)) {
        if (floatVal < config->getTempAcceptableLow() || floatVal > config->getTempComfortableHigh()) {
          ok = false;
          errorMsg = "Comfortable Low must be between Acceptable Low and Comfortable High";
        } else {
          config->setTempComfortableLow(floatVal);
        }
      } else if (type == "comfortable_high" && parseStrictFloat(valueStr, floatVal, -50.0f, 50.0f)) {
        if (floatVal < config->getTempComfortableLow() || floatVal > config->getTempAcceptableHigh()) {
          ok = false;
          errorMsg = "Comfortable High must be between Comfortable Low and Acceptable High";
        } else {
          config->setTempComfortableHigh(floatVal);
        }
      } else if (type == "acceptable_high" && parseStrictFloat(valueStr, floatVal, -50.0f, 50.0f)) {
        if (floatVal < config->getTempComfortableHigh()) {
          ok = false;
          errorMsg = "Acceptable High cannot be lower than Comfortable High";
        } else {
          config->setTempAcceptableHigh(floatVal);
        }
      } else {
        ok = false;
      }
    } else if (section == "humi") {
      if (type == "acceptable_low" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (floatVal > config->getHumiComfortableLow()) {
          ok = false;
          errorMsg = "Acceptable Low cannot exceed Comfortable Low";
        } else {
          config->setHumiAcceptableLow(floatVal);
        }
      } else if (type == "comfortable_low" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (floatVal < config->getHumiAcceptableLow() || floatVal > config->getHumiComfortableHigh()) {
          ok = false;
          errorMsg = "Comfortable Low must be between Acceptable Low and Comfortable High";
        } else {
          config->setHumiComfortableLow(floatVal);
        }
      } else if (type == "comfortable_high" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (floatVal < config->getHumiComfortableLow() || floatVal > config->getHumiAcceptableHigh()) {
          ok = false;
          errorMsg = "Comfortable High must be between Comfortable Low and Acceptable High";
        } else {
          config->setHumiComfortableHigh(floatVal);
        }
      } else if (type == "acceptable_high" && parseStrictFloat(valueStr, floatVal, 0.0f, 100.0f)) {
        if (floatVal < config->getHumiComfortableHigh()) {
          ok = false;
          errorMsg = "Acceptable High cannot be lower than Comfortable High";
        } else {
          config->setHumiAcceptableHigh(floatVal);
        }
      } else {
        ok = false;
      }
    } else if (section == "inactivity") {
      if (type == "timer_delay" && parseStrictInt(valueStr, intVal, 5, 120)) {
        config->setInactivityTimerDelay(intVal);
        HomeAssistantManager *ha = HomeAssistantManager::getInstance();
        if (ha)
          ha->updateInactivityTimerDelayState(true);
      } else {
        ok = false;
      }
    } else if (section == "log") {
      if (type == "level" && parseStrictInt(valueStr, intVal, 0, 3)) {
        AppLogLevel level = static_cast<AppLogLevel>(intVal);
        config->setLogLevel(level);
        logger.setLogLevel(level);
        HomeAssistantManager *ha = HomeAssistantManager::getInstance();
        if (ha)
          ha->updateLogLevelState(level);
        logger.infof("Log level updated to %d via Web Server", intVal);
      } else {
        ok = false;
      }
    } else if (section == "mqtt") {
      if (type == "host" && valueStr.length() > 0 && valueStr.length() <= 63)
        config->setMqttHost(valueStr.c_str());
      else if (type == "port" && parseStrictInt(valueStr, intVal, 1, 65535))
        config->setMqttPort(intVal);
      else if (type == "user" && valueStr.length() <= 31)
        config->setMqttUser(valueStr.c_str());
      else if (type == "password" && valueStr.length() <= 63) {
        if (valueStr.length() > 0) {
          config->setMqttPassword(valueStr.c_str());
        }
      } else if (type == "device_id" && valueStr.length() > 0 &&
                 valueStr.length() <= 31)
        config->setMqttDeviceId(valueStr.c_str());
      else if (type == "device_name" && valueStr.length() > 0 &&
                 valueStr.length() <= 63)
        config->setMqttDeviceName(valueStr.c_str());
      else
        ok = false;
    } else if (section == "voc") {
      if (type == "warn" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(intVal, config->getVocDangerThreshold(), true))
          config->setVocWarnThreshold(intVal);
        else
          ok = false;
      } else if (type == "danger" && parseStrictInt(valueStr, intVal, 0, 1000)) {
        if (validateWarnDanger(config->getVocWarnThreshold(), intVal, false))
          config->setVocDangerThreshold(intVal);
        else
          ok = false;
      } else {
        ok = false;
      }
    } else if (section == "display") {
      if (type == "brightness" && parseStrictInt(valueStr, intVal, 0, 255)) {
        uint8_t b = static_cast<uint8_t>(intVal);
        UITask::getInstance().set_brightness(b);
        HomeAssistantManager *ha = HomeAssistantManager::getInstance();
        if (ha)
          ha->updateBacklightState(b > 0, b);
        logger.infof("Display brightness set to %u via Web Server", b);
      } else if (type == "backlight" && parseStrictInt(valueStr, intVal, 0, 1)) {
        uint8_t curB = UITask::getInstance().get_brightness();
        uint8_t newB = (intVal != 0) ? (curB == 0 ? 255 : curB) : 0;
        UITask::getInstance().set_brightness(newB);
        HomeAssistantManager *ha = HomeAssistantManager::getInstance();
        if (ha)
          ha->updateBacklightState(intVal != 0, newB);
        logger.infof("Display backlight set to %s via Web Server",
                     intVal != 0 ? "ON" : "OFF");
      } else {
        ok = false;
      }
    } else if (section == "scd30") {
      if (type == "autocal" && parseStrictInt(valueStr, intVal, 0, 1)) {
        bool enabled = (intVal != 0);
        send_command_to_nano_with_param(CMD_SET_SCD30_AUTOCAL,
                                        enabled ? '1' : '0');
        HomeAssistantManager *ha = HomeAssistantManager::getInstance();
        if (ha)
          ha->updateScd30AutoCalState(enabled);
        UITask::getInstance().update_scd30_autocal(enabled);
        logger.infof("SCD30 AutoCalibration set to %s via Web Server",
                     enabled ? "ON" : "OFF");
      } else if (type == "forcecal" &&
                 parseStrictInt(valueStr, intVal, SCD30_FORCECAL_MIN_PPM, SCD30_FORCECAL_MAX_PPM)) {
        uint16_t ppm = static_cast<uint16_t>(intVal);
        char cmd_str[8];
        snprintf(cmd_str, sizeof(cmd_str), "%c%u", CMD_SET_SCD30_FORCECAL, ppm);
        send_command_to_nano_str(cmd_str);
        HomeAssistantManager *ha = HomeAssistantManager::getInstance();
        if (ha)
          ha->updateScd30ForceCalValue(ppm);
        UITask::getInstance().update_scd30_forcecal(ppm);
        logger.infof("SCD30 Force Calibration set to %u ppm via Web Server",
                     ppm);
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
      server.send(400, "text/plain", errorMsg);
    }
  } else {
    server.send(400, "text/plain", "Missing required parameters");
  }
}

void log_update_error() {
  logger.errorf("Firmware update failed. Error code: %d", Update.getError());
}

void WebServerManager::handleUpload() {
  // WebServer receives a whole upload inside one handleClient() call (tens of
  // seconds for a firmware image): feed the watchdog on every chunk
#ifdef ENABLE_TASK_WATCHDOG
  esp_task_wdt_reset();
#endif
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (Update.isRunning()) {
      Update.abort();
    }
    Update.clearError();
    s_upload_state = UploadState::InProgress;
    s_upload_bytes_received = 0;
    s_upload_error_msg = "";
    s_app_header_checked = false;
    s_app_header_len = 0;
    String target = server.hasArg("target") ? server.arg("target") : "";
    String lowerName = upload.filename;
    lowerName.toLowerCase();

    s_is_nano_update =
        (target.equalsIgnoreCase("nano") ||
         (target.length() == 0 &&
          (lowerName.endsWith(".hex") || lowerName.indexOf("nano") >= 0)));
    s_is_bootloader_update =
        !s_is_nano_update &&
        (target.equalsIgnoreCase("bootloader") ||
         (target.length() == 0 && lowerName.startsWith("bootloader")));

    if (s_is_nano_update) {
      logger.infof("[Web OTA] Arduino Nano Firmware Update Started: %s",
                   upload.filename.c_str());
      s_nano_failed = false;
      if (!s_nano_uploader) {
        s_nano_uploader = new NanoFlasher::Uploader();
      }
      if (!s_nano_uploader || !s_nano_uploader->begin(s_upload_error_msg)) {
        s_nano_failed = true;
        logger.errorf(
            "[Web OTA] Failed to allocate/initialize Nano uploader: %s",
            s_upload_error_msg.c_str());
      }
    } else if (s_is_bootloader_update) {
      logger.infof("[Web OTA] Bootloader Update Started: %s",
                   upload.filename.c_str());
      s_bootloader_received = 0;
      s_bootloader_failed = false;
      if (!s_bootloader_buf) {
        s_bootloader_buf =
            (uint8_t *)malloc(0x7000); // Max size between 0x1000 and 0x8000
      }
      if (!s_bootloader_buf) {
        s_bootloader_failed = true;
        s_upload_error_msg = "Out of memory allocating bootloader buffer";
        logger.error(
            "[Web OTA] Out of memory allocating bootloader staging buffer");
      }
    } else {
      logger.infof("[Web OTA] App Firmware Update Started: %s",
                   upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        log_update_error();
        s_upload_error_msg =
            String("Update.begin failed, error: ") + Update.getError();
      }
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (s_upload_state != UploadState::InProgress) {
      return;
    }
    s_upload_bytes_received += upload.currentSize;
    if (s_is_nano_update) {
      if (!s_nano_failed && s_nano_uploader) {
        if (!s_nano_uploader->write(upload.buf, upload.currentSize,
                                    s_upload_error_msg)) {
          s_nano_failed = true;
          logger.errorf("[Web OTA] Nano firmware upload write error: %s",
                        s_upload_error_msg.c_str());
        }
      }
    } else if (s_is_bootloader_update) {
      if (!s_bootloader_failed) {
        if (s_bootloader_received + upload.currentSize > 0x7000) {
          s_bootloader_failed = true;
          s_upload_error_msg =
              "Bootloader binary exceeds partition table boundary (28KB max)";
          logger.error(
              "[Web OTA] Bootloader binary exceeds 0x7000 (28KB) limit!");
        } else {
          memcpy(s_bootloader_buf + s_bootloader_received, upload.buf,
                 upload.currentSize);
          s_bootloader_received += upload.currentSize;
        }
      }
    } else {
      if (!s_app_header_checked) {
        size_t needed = sizeof(s_app_header_buf) - s_app_header_len;
        size_t to_copy =
            (upload.currentSize < needed) ? upload.currentSize : needed;
        memcpy(s_app_header_buf + s_app_header_len, upload.buf, to_copy);
        s_app_header_len += to_copy;

        if (s_app_header_len >= sizeof(s_app_header_buf)) {
          s_app_header_checked = true;
          if (s_app_header_buf[0] != ESP_IMAGE_HEADER_MAGIC) {
            Update.abort();
            s_upload_state = UploadState::Failed;
            s_upload_error_msg =
                "Invalid binary: missing ESP32 image magic byte (0xE9)";
            logger.errorf("[Web OTA] Firmware validation rejected: magic byte "
                          "is 0x%02X (expected 0x%02X)!",
                          s_app_header_buf[0], ESP_IMAGE_HEADER_MAGIC);
          } else {
            uint32_t word_at_0x20 = 0;
            memcpy(&word_at_0x20, s_app_header_buf + 0x20,
                   sizeof(word_at_0x20));
            if (word_at_0x20 != ESP_APP_DESC_MAGIC_WORD) {
              Update.abort();
              s_upload_state = UploadState::Failed;
              s_upload_error_msg = "Validation rejected: file is missing "
                                   "Application Descriptor. Did you upload a "
                                   "bootloader to the firmware target?";
              logger.errorf("[Web OTA] Firmware validation rejected: word at "
                            "0x20 is 0x%08X (expected 0x%08X)!",
                            word_at_0x20, ESP_APP_DESC_MAGIC_WORD);
            }
          }
        }
      }
      if (s_upload_state == UploadState::InProgress && !Update.hasError()) {
        if (Update.write(upload.buf, upload.currentSize) !=
            upload.currentSize) {
          s_upload_state = UploadState::Failed;
          log_update_error();
          s_upload_error_msg =
              String("Update.write failed, error: ") + Update.getError();
        }
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (s_is_nano_update) {
      if (s_upload_bytes_received == 0 || s_nano_failed || !s_nano_uploader) {
        s_upload_state = UploadState::Failed;
        if (s_upload_error_msg.length() == 0)
          s_upload_error_msg = "Nano update upload aborted/failed";
        logger.errorf("[Web OTA] Nano update upload aborted/failed: %s",
                      s_upload_error_msg.c_str());
      } else {
        if (!s_nano_uploader->finish(s_upload_error_msg)) {
          s_nano_failed = true;
          s_upload_state = UploadState::Failed;
          logger.errorf("[Web OTA] Nano firmware parsing failed: %s",
                        s_upload_error_msg.c_str());
        } else {
          logger.infof("[Web OTA] Nano firmware image parsed successfully (%u "
                       "bytes). Flashing to ATmega328P...",
                       s_nano_uploader->getFlashSize());
          if (!NanoFlasher::flashImage(s_nano_uploader->getFlashBuffer(),
                                       s_nano_uploader->getFlashSize(),
                                       s_upload_error_msg)) {
            s_nano_failed = true;
            s_upload_state = UploadState::Failed;
            logger.errorf("[Web OTA] Nano flashing failed: %s",
                          s_upload_error_msg.c_str());
          } else {
            s_upload_state = UploadState::Success;
            logger.info("[Web OTA] Arduino Nano flashed and verified "
                        "successfully via urprotocol!");
          }
        }
      }
      if (s_nano_uploader) {
        delete s_nano_uploader;
        s_nano_uploader = nullptr;
      }
    } else if (s_is_bootloader_update) {
      if (s_upload_bytes_received == 0 || s_bootloader_failed) {
        s_upload_state = UploadState::Failed;
        if (s_upload_error_msg.length() == 0)
          s_upload_error_msg = "Bootloader update failed";
        logger.errorf("[Web OTA] Bootloader update failed: %s",
                      s_upload_error_msg.c_str());
        if (s_bootloader_buf) {
          free(s_bootloader_buf);
          s_bootloader_buf = nullptr;
        }
      } else {
        logger.infof(
            "[Web OTA] Bootloader upload finished (%u bytes). Verifying...",
            s_bootloader_received);
        if (s_bootloader_received < 4096) {
          s_bootloader_failed = true;
          s_upload_state = UploadState::Failed;
          s_upload_error_msg = "Bootloader image is too small (< 4KB)";
          logger.error("[Web OTA] Bootloader binary is too small (< 4KB)");
        } else {
          // Enforce 4-point binary signature validation:
          esp_image_header_t *hdr = (esp_image_header_t *)s_bootloader_buf;

          // 1. ESP32 Magic byte check (0xE9)
          bool has_magic = (hdr->magic == ESP_IMAGE_HEADER_MAGIC);

          // 2. Chip architecture ID check (must match ESP32 target)
          bool is_esp32_chip = (hdr->chip_id == ESP_CHIP_ID_ESP32);

          // 3. Entry point address range: bootloader entry points to IRAM
          // (0x4007xxxx - 0x40085xxx)
          bool entry_in_bl_range =
              (hdr->entry_addr >= 0x40078000 && hdr->entry_addr <= 0x40085FFF);

          // 4. Must NOT contain the Application Descriptor magic word
          // (0xABCD5432) at offset 0x20
          uint32_t word_at_0x20 = 0;
          if (s_bootloader_received >= 0x24) {
            memcpy(&word_at_0x20, s_bootloader_buf + 0x20,
                   sizeof(word_at_0x20));
          }
          bool has_app_desc = (word_at_0x20 == ESP_APP_DESC_MAGIC_WORD);

          if (!has_magic) {
            s_bootloader_failed = true;
            s_upload_state = UploadState::Failed;
            s_upload_error_msg =
                "Invalid binary: missing ESP32 image magic byte (0xE9)";
            logger.errorf(
                "[Web OTA] Bootloader validation failed: bad magic byte 0x%02X",
                hdr->magic);
          } else if (!is_esp32_chip) {
            s_bootloader_failed = true;
            s_upload_state = UploadState::Failed;
            s_upload_error_msg = "Invalid binary: chip ID does not match ESP32";
            logger.errorf("[Web OTA] Bootloader validation failed: chip_id "
                          "0x%04X != ESP32",
                          hdr->chip_id);
          } else if (has_app_desc) {
            s_bootloader_failed = true;
            s_upload_state = UploadState::Failed;
            s_upload_error_msg = "Validation rejected: file is an Application "
                                 "Firmware binary, NOT a bootloader!";
            logger.error("[Web OTA] Bootloader validation rejected: detected "
                         "Application Descriptor (0xABCD5432)!");
          } else if (!entry_in_bl_range) {
            s_bootloader_failed = true;
            s_upload_state = UploadState::Failed;
            s_upload_error_msg = "Validation rejected: entry address outside "
                                 "valid bootloader IRAM range";
            logger.errorf("[Web OTA] Bootloader validation failed: entry_addr "
                          "0x%08X out of range",
                          hdr->entry_addr);
          } else {
            // All 4 detection points passed! Flash bootloader to offset 0x1000
            // Align erase size up to 4KB sector boundary
            size_t erase_len = ((s_bootloader_received + 4095) / 4096) * 4096;
            logger.infof("[Web OTA] Erasing flash at 0x1000 (%u bytes)...",
                         erase_len);

            // Temporarily disable dangerous write protection for bootloader
            // region (0x1000)
            esp_flash_set_dangerous_write_protection(esp_flash_default_chip,
                                                     false);
            esp_err_t err = esp_flash_erase_region(NULL, 0x1000, erase_len);
            if (err != ESP_OK) {
              esp_flash_set_dangerous_write_protection(esp_flash_default_chip,
                                                       true);
              s_bootloader_failed = true;
              s_upload_state = UploadState::Failed;
              s_upload_error_msg =
                  String("Flash erase failed: ") + esp_err_to_name(err);
              logger.errorf(
                  "[Web OTA] esp_flash_erase_region failed: 0x%x (%s)", err,
                  esp_err_to_name(err));
            } else {
              logger.infof("[Web OTA] Writing bootloader to flash at 0x1000 "
                           "(%u bytes)...",
                           s_bootloader_received);
              err = esp_flash_write(NULL, s_bootloader_buf, 0x1000,
                                    s_bootloader_received);
              esp_flash_set_dangerous_write_protection(esp_flash_default_chip,
                                                       true);
              if (err != ESP_OK) {
                s_bootloader_failed = true;
                s_upload_state = UploadState::Failed;
                s_upload_error_msg =
                    String("Flash write failed: ") + esp_err_to_name(err);
                logger.errorf("[Web OTA] esp_flash_write failed: 0x%x (%s)",
                              err, esp_err_to_name(err));
              } else {
                // Verify the written bootloader in flash
                esp_image_metadata_t meta = {};
                const esp_partition_pos_t part_pos = {
                    .offset = 0x1000,
                    .size = 0x7000,
                };
                err =
                    esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &part_pos, &meta);
                if (err != ESP_OK) {
                  s_bootloader_failed = true;
                  s_upload_state = UploadState::Failed;
                  s_upload_error_msg =
                      String("Flash image verification failed: ") +
                      esp_err_to_name(err);
                  logger.errorf("[Web OTA] Flash verification failed for "
                                "bootloader: 0x%x",
                                err);
                } else {
                  s_upload_state = UploadState::Success;
                  logger.info("[Web OTA] Bootloader passed all 4 checks, "
                              "successfully flashed and verified at 0x1000!");
                }
              }
            }
          }
        }
        if (s_bootloader_buf) {
          free(s_bootloader_buf);
          s_bootloader_buf = nullptr;
        }
      }
    } else {
      if (!s_app_header_checked) {
        s_upload_state = UploadState::Failed;
        if (s_upload_error_msg.length() == 0) {
          s_upload_error_msg = "Firmware image is too small (< 36 bytes)";
        }
        logger.error("[Web OTA] Firmware image is too small (< 36 bytes)");
      }
      if (s_upload_bytes_received == 0 ||
          s_upload_state != UploadState::InProgress || Update.hasError()) {
        s_upload_state = UploadState::Failed;
        if (Update.isRunning()) {
          Update.abort();
        }
        if (s_upload_error_msg.length() == 0) {
          s_upload_error_msg = (s_upload_bytes_received == 0)
                                   ? "Empty firmware image (0 bytes)"
                                   : "Firmware upload failed";
        }
      } else if (Update.end(true)) {
        s_upload_state = UploadState::Success;
        logger.infof("Web OTA Update Success: %u bytes", upload.totalSize);
        BootloopGuard::markOtaTrialPending();
      } else {
        s_upload_state = UploadState::Failed;
        log_update_error();
        s_upload_error_msg =
            String("Update.end failed, error: ") + Update.getError();
        if (Update.isRunning()) {
          Update.abort();
        }
      }
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    s_upload_state = UploadState::Aborted;
    s_app_header_checked = false;
    s_app_header_len = 0;
    if (s_upload_error_msg.length() == 0) {
      s_upload_error_msg = "Firmware upload aborted by client";
    }
    logger.warning("[Web OTA] Firmware upload aborted by client");
    if (Update.isRunning()) {
      Update.abort();
    }
    if (s_nano_uploader) {
      delete s_nano_uploader;
      s_nano_uploader = nullptr;
    }
    if (s_bootloader_buf) {
      free(s_bootloader_buf);
      s_bootloader_buf = nullptr;
    }
  }
}

void WebServerManager::handleConfigGas() {
  ConfigManagerAccessor config;
  streamTemplate(config_gas_html,
                 [&](const char *key, char *buf, size_t len) -> bool {
                   if (strcmp(key, "FIRMWARE_VERSION") == 0) {
                     snprintf(buf, len, "%s", _firmware_version);
                     return true;
                   }
                   if (strcmp(key, "NO2_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getNo2WarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "NO2_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getNo2DangerThreshold());
                     return true;
                   }
                   if (strcmp(key, "O3_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getO3WarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "O3_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getO3DangerThreshold());
                     return true;
                   }
                   if (strcmp(key, "NOX_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getNoxWarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "NOX_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getNoxDangerThreshold());
                     return true;
                   }
                   if (strcmp(key, "CO_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getCoWarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "CO_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getCoDangerThreshold());
                     return true;
                   }
                   if (strcmp(key, "CO2_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getCo2WarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "CO2_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getCo2DangerThreshold());
                     return true;
                   }
                   if (strcmp(key, "VOC_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getVocWarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "VOC_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getVocDangerThreshold());
                     return true;
                   }
                   return false;
                 });
  logger.info("Served gas config page");
}

void WebServerManager::handleConfigPM() {
  ConfigManagerAccessor config;
  streamTemplate(config_pm_html,
                 [&](const char *key, char *buf, size_t len) -> bool {
                   if (strcmp(key, "FIRMWARE_VERSION") == 0) {
                     snprintf(buf, len, "%s", _firmware_version);
                     return true;
                   }
                   if (strcmp(key, "PM1_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getPm1WarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "PM1_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getPm1DangerThreshold());
                     return true;
                   }
                   if (strcmp(key, "PM25_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getPm25WarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "PM25_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getPm25DangerThreshold());
                     return true;
                   }
                   if (strcmp(key, "PM4_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getPm4WarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "PM4_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getPm4DangerThreshold());
                     return true;
                   }
                   if (strcmp(key, "PM10_WARN") == 0) {
                     snprintf(buf, len, "%d", config->getPm10WarnThreshold());
                     return true;
                   }
                   if (strcmp(key, "PM10_DANGER") == 0) {
                     snprintf(buf, len, "%d", config->getPm10DangerThreshold());
                     return true;
                   }
                   return false;
                 });
  logger.info("Served PM config page");
}

void WebServerManager::handleConfigHVAC() {
  ConfigManagerAccessor config;
  streamTemplate(
      config_hvac_html, [&](const char *key, char *buf, size_t len) -> bool {
        if (strcmp(key, "FIRMWARE_VERSION") == 0) {
          snprintf(buf, len, "%s", _firmware_version);
          return true;
        }
        if (strcmp(key, "PRESSURE_LOW") == 0) {
          snprintf(buf, len, "%.1f", config->getPressureLowThreshold());
          return true;
        }
        if (strcmp(key, "PRESSURE_MID") == 0) {
          snprintf(buf, len, "%.1f", config->getPressureMidThreshold());
          return true;
        }
        if (strcmp(key, "PRESSURE_HIGH") == 0) {
          snprintf(buf, len, "%.1f", config->getHighPressureThreshold());
          return true;
        }
        if (strcmp(key, "FAN_ON") == 0) {
          snprintf(buf, len, "%.1f", config->getFanOnCurrentThreshold());
          return true;
        }
        if (strcmp(key, "FAN_OFF") == 0) {
          snprintf(buf, len, "%.1f", config->getFanOffCurrentThreshold());
          return true;
        }
        if (strcmp(key, "FAN_HIGH") == 0) {
          snprintf(buf, len, "%.1f", config->getFanHighCurrentThreshold());
          return true;
        }
        if (strcmp(key, "COMPRESSOR_ON") == 0) {
          snprintf(buf, len, "%.1f", config->getCompressorOnCurrentThreshold());
          return true;
        }
        if (strcmp(key, "COMPRESSOR_OFF") == 0) {
          snprintf(buf, len, "%.1f",
                   config->getCompressorOffCurrentThreshold());
          return true;
        }
        if (strcmp(key, "COMPRESSOR_HIGH") == 0) {
          snprintf(buf, len, "%.1f",
                   config->getCompressorHighCurrentThreshold());
          return true;
        }
        if (strcmp(key, "PUMP_ON") == 0) {
          snprintf(buf, len, "%.1f", config->getPumpOnCurrentThreshold());
          return true;
        }
        if (strcmp(key, "PUMP_OFF") == 0) {
          snprintf(buf, len, "%.1f", config->getPumpOffCurrentThreshold());
          return true;
        }
        if (strcmp(key, "PUMP_HIGH") == 0) {
          snprintf(buf, len, "%.1f", config->getPumpHighCurrentThreshold());
          return true;
        }
        if (strcmp(key, "GEIGER_ABNORMAL_LOW") == 0) {
          snprintf(buf, len, "%.2f", config->getGeigerAbnormalLowThreshold());
          return true;
        }
        if (strcmp(key, "GEIGER_ABNORMAL_HIGH") == 0) {
          snprintf(buf, len, "%.2f", config->getGeigerAbnormalHighThreshold());
          return true;
        }
        if (strcmp(key, "GEIGER_DANGER_HIGH") == 0) {
          snprintf(buf, len, "%.2f", config->getGeigerDangerHighThreshold());
          return true;
        }
        return false;
      });
  logger.info("Served HVAC config page");
}

void WebServerManager::handleConfigClimate() {
  ConfigManagerAccessor config;
  streamTemplate(
      config_climate_html, [&](const char *key, char *buf, size_t len) -> bool {
        if (strcmp(key, "FIRMWARE_VERSION") == 0) {
          snprintf(buf, len, "%s", _firmware_version);
          return true;
        }
        if (strcmp(key, "TEMP_COMFORTABLE_LOW") == 0) {
          snprintf(buf, len, "%.1f", config->getTempComfortableLow());
          return true;
        }
        if (strcmp(key, "TEMP_COMFORTABLE_HIGH") == 0) {
          snprintf(buf, len, "%.1f", config->getTempComfortableHigh());
          return true;
        }
        if (strcmp(key, "TEMP_ACCEPTABLE_LOW") == 0) {
          snprintf(buf, len, "%.1f", config->getTempAcceptableLow());
          return true;
        }
        if (strcmp(key, "TEMP_ACCEPTABLE_HIGH") == 0) {
          snprintf(buf, len, "%.1f", config->getTempAcceptableHigh());
          return true;
        }
        if (strcmp(key, "HUMI_COMFORTABLE_LOW") == 0) {
          snprintf(buf, len, "%.1f", config->getHumiComfortableLow());
          return true;
        }
        if (strcmp(key, "HUMI_COMFORTABLE_HIGH") == 0) {
          snprintf(buf, len, "%.1f", config->getHumiComfortableHigh());
          return true;
        }
        if (strcmp(key, "HUMI_ACCEPTABLE_LOW") == 0) {
          snprintf(buf, len, "%.1f", config->getHumiAcceptableLow());
          return true;
        }
        if (strcmp(key, "HUMI_ACCEPTABLE_HIGH") == 0) {
          snprintf(buf, len, "%.1f", config->getHumiAcceptableHigh());
          return true;
        }
        return false;
      });
  logger.info("Served climate config page");
}

void WebServerManager::handleConfigSystem() {
  ConfigManagerAccessor config;
  streamTemplate(
      config_system_html, [&](const char *key, char *buf, size_t len) -> bool {
        if (strcmp(key, "FIRMWARE_VERSION") == 0) {
          snprintf(buf, len, "%s", _firmware_version);
          return true;
        }
        AppLogLevel currentLevel = config->getLogLevel();
        if (strcmp(key, "LOG_DEBUG_SEL") == 0) {
          if (currentLevel == APP_LOG_DEBUG)
            snprintf(buf, len, " selected");
          return true;
        }
        if (strcmp(key, "LOG_INFO_SEL") == 0) {
          if (currentLevel == APP_LOG_INFO)
            snprintf(buf, len, " selected");
          return true;
        }
        if (strcmp(key, "LOG_WARN_SEL") == 0) {
          if (currentLevel == APP_LOG_WARNING)
            snprintf(buf, len, " selected");
          return true;
        }
        if (strcmp(key, "LOG_ERROR_SEL") == 0) {
          if (currentLevel == APP_LOG_ERROR)
            snprintf(buf, len, " selected");
          return true;
        }
        uint8_t currentBrightness = UITask::getInstance().get_brightness();
        if (strcmp(key, "DISPLAY_BRIGHTNESS") == 0) {
          snprintf(buf, len, "%u", currentBrightness);
          return true;
        }
        if (strcmp(key, "BACKLIGHT_ON_SEL") == 0) {
          if (currentBrightness > 0)
            snprintf(buf, len, " selected");
          return true;
        }
        if (strcmp(key, "BACKLIGHT_OFF_SEL") == 0) {
          if (currentBrightness == 0)
            snprintf(buf, len, " selected");
          return true;
        }
        if (strcmp(key, "INACTIVITY_TIMER_DELAY") == 0) {
          snprintf(buf, len, "%u", (unsigned)config->getInactivityTimerDelay());
          return true;
        }
        if (strcmp(key, "WIFI_SSID") == 0) {
          snprintf(buf, len, "%s", config->getWifiSsid());
          return true;
        }
        if (strcmp(key, "WIFI_PASS_PLACEHOLDER") == 0) {
          if (strlen(config->getWifiPassword()) > 0) {
            snprintf(buf, len, "•••••••• (leave blank to keep)");
          } else {
            snprintf(buf, len, "Enter Wi-Fi password");
          }
          return true;
        }
        if (strcmp(key, "MQTT_HOST") == 0) {
          snprintf(buf, len, "%s", config->getMqttHost());
          return true;
        }
        if (strcmp(key, "MQTT_PORT") == 0) {
          snprintf(buf, len, "%u", config->getMqttPort());
          return true;
        }
        if (strcmp(key, "MQTT_USER") == 0) {
          snprintf(buf, len, "%s", config->getMqttUser());
          return true;
        }
        if (strcmp(key, "MQTT_PASS_PLACEHOLDER") == 0) {
          if (strlen(config->getMqttPassword()) > 0) {
            snprintf(buf, len, "•••••••• (leave blank to keep)");
          } else {
            snprintf(buf, len, "Enter MQTT password");
          }
          return true;
        }
        if (strcmp(key, "MQTT_DEVICE_ID") == 0) {
          snprintf(buf, len, "%s", config->getMqttDeviceId());
          return true;
        }
        if (strcmp(key, "MQTT_DEVICE_NAME") == 0) {
          snprintf(buf, len, "%s", config->getMqttDeviceName());
          return true;
        }
        return false;
      });
  logger.info("Served system config page");
}

void WebServerManager::handleConfigMaintenance() {
  HomeAssistantManager *ha = HomeAssistantManager::getInstance();
  bool autoCalState = ha ? ha->getScd30AutoCalState() : false;
  uint16_t forceCalValue = ha ? ha->getScd30ForceCalValue() : 400;
  if (forceCalValue == 0)
    forceCalValue = 400;

  streamTemplate(config_maintenance_html,
                 [&](const char *key, char *buf, size_t len) -> bool {
                   if (strcmp(key, "FIRMWARE_VERSION") == 0) {
                     snprintf(buf, len, "%s", _firmware_version);
                     return true;
                   }
                   if (strcmp(key, "SCD30_AUTOCAL_ON_SEL") == 0) {
                     if (autoCalState)
                       snprintf(buf, len, " selected");
                     return true;
                   }
                   if (strcmp(key, "SCD30_AUTOCAL_OFF_SEL") == 0) {
                     if (!autoCalState)
                       snprintf(buf, len, " selected");
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

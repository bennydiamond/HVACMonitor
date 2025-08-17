#include "WebServerManager.h"
#include <WiFi.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include "Logger.h"
#include "ConfigManager.h"
#include "HomeAssistantManager.h"

#include "webserver/WebServerConfigTabs.h"
#include "webserver/WebServerConfigTabsExtra.h"
#include "webserver/WebServerConfigSystem.h"
#include "webserver/WebServerConfigClimate.h"

WebServer server(80);
String WebServerManager::_firmware_version;

// Large config page removed to save memory - now using split pages

const char* update_page_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>HVAC Sensor OTA Update</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial, sans-serif; background-color: #f4f4f4; text-align: center; padding: 50px; }
form { background: white; padding: 20px; border-radius: 8px; display: inline-block; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
input[type=file] { margin-bottom: 20px; }
input[type=submit] { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
input[type=submit]:hover { background: #0056b3; }
.footer { margin-top: 20px; font-size: 0.8em; color: #666; }
.nav { margin-bottom: 20px; }
.nav a { display: inline-block; margin: 0 10px; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 5px; }
.nav a:hover { background: #0056b3; }
</style>
</head>
<body>
    <div class="nav">
        <a href="/">OTA Update</a>
        <a href="/config">Configuration</a>
    </div>
    
    <h2>HVAC Sensor OTA Firmware Update</h2>
    <form method='POST' action='/upload' enctype='multipart/form-data'>
        <input type='file' name='update' accept='.bin'>
        <input type='submit' value='Upload Firmware'>
    </form>
    <div class='footer'>
        <p>Device: HVAC Sensor Display<br>Version: %FIRMWARE_VERSION%<br>IP: %IP_ADDRESS%</p>
    </div>
</body>
</html>
)rawliteral";


void WebServerManager::init(const char* firmware_version) {
    _firmware_version = firmware_version;

    server.on("/", HTTP_GET, handleRoot);
    server.on("/config", HTTP_GET, handleConfig);


    server.on("/config/gas", HTTP_GET, handleConfigGas);
    server.on("/config/pm", HTTP_GET, handleConfigPM);
    server.on("/config/hvac", HTTP_GET, handleConfigHVAC);
    server.on("/config/climate", HTTP_GET, handleConfigClimate);
    server.on("/config/system", HTTP_GET, handleConfigSystem);
    server.on("/config/update", HTTP_POST, handleConfigUpdate);
    
    server.on("/upload", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "Update complete! Rebooting...");
        delay(500);
        ESP.restart();
    }, handleUpload);

    server.onNotFound(handleNotFound);

    server.begin();
    logger.info("HTTP server for OTA started.");
}

void WebServerManager::handle() {
    // Reset watchdog to prevent crashes during long operations
    esp_task_wdt_reset();
    
    // Handle client requests
    server.handleClient();
    
    // Yield to other tasks to prevent blocking
    yield();
}

void WebServerManager::handleRoot() {
    String ip = WiFi.localIP().toString();
    String html = String(update_page_html);
    html.replace("%IP_ADDRESS%", ip);
    html.replace("%FIRMWARE_VERSION%", _firmware_version);
    
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", html);
    logger.debug("Served OTA update web page.");
}

void WebServerManager::handleConfig() {
    String ip = WiFi.localIP().toString();
    size_t freeHeap = ESP.getFreeHeap();
    
    String html = String(config_main_html);
    html.replace("%IP_ADDRESS%", ip);
    html.replace("%FREE_HEAP%", String(freeHeap));
    
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", html);
    logger.infof("Served main config page. Free heap: %d bytes", freeHeap);
}

void WebServerManager::handleConfigUpdate() {
    // Check available memory before processing
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 20000) {
        logger.warningf("Low memory (%d bytes), rejecting config update", freeHeap);
        server.send(503, "text/plain", "Service temporarily unavailable - low memory");
        return;
    }
    
    String section = server.arg("section");
    
    if (section == "mqtt_device" && server.hasArg("action") && server.arg("action") == "save") {
        // Validate MQTT settings before saving
        String host = server.arg("host");
        String port = server.arg("port");
        String user = server.arg("user");
        String password = server.arg("password");
        String deviceId = server.arg("device_id");
        String deviceName = server.arg("device_name");
        
        // Check for required fields
        if (host.length() == 0 || user.length() == 0 || password.length() == 0 || 
            deviceId.length() == 0 || deviceName.length() == 0) {
            server.send(400, "text/plain", "All MQTT fields are required");
            return;
        }
        
        // Validate port number
        int portNum = port.toInt();
        if (portNum <= 0 || portNum > 65535) {
            server.send(400, "text/plain", "Invalid port number");
            return;
        }
        
        // Get thread-safe access to ConfigManager
        ConfigManagerAccessor config;
        
        // Save all MQTT configuration at once
        config->setMqttHost(host.c_str());
        config->setMqttPort(portNum);
        config->setMqttUser(user.c_str());
        config->setMqttPassword(password.c_str());
        config->setMqttDeviceId(deviceId.c_str());
        config->setMqttDeviceName(deviceName.c_str());
        
        logger.info("MQTT device configuration saved. Rebooting device...");
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "Configuration saved successfully. Rebooting device...");
        
        // Delay to allow response to be sent before reboot
        delay(2000);
        ESP.restart();
        return;
    }
    
    // Regular parameter updates (require section, type, and value)
    if (server.hasArg("section") && server.hasArg("type") && server.hasArg("value")) {
        String type = server.arg("type");
        String valueStr = server.arg("value");
        
        int value = valueStr.toInt();
        
        // Get thread-safe access to ConfigManager
        ConfigManagerAccessor config;
        
        if (section == "no2") {
            if (type == "warn") {
                config->setNo2WarnThreshold(value);
                server.send(200, "text/plain", "NO2 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setNo2DangerThreshold(value);
                server.send(200, "text/plain", "NO2 Danger threshold updated successfully");
            }
        } else if (section == "o3") {
            if (type == "warn") {
                config->setO3WarnThreshold(value);
                server.send(200, "text/plain", "O3 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setO3DangerThreshold(value);
                server.send(200, "text/plain", "O3 Danger threshold updated successfully");
            }
        } else if (section == "nox") {
            if (type == "warn") {
                config->setNoxWarnThreshold(value);
                server.send(200, "text/plain", "NOx Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setNoxDangerThreshold(value);
                server.send(200, "text/plain", "NOx Danger threshold updated successfully");
            }
        } else if (section == "co") {
            if (type == "warn") {
                config->setCoWarnThreshold(value);
                server.send(200, "text/plain", "CO Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setCoDangerThreshold(value);
                server.send(200, "text/plain", "CO Danger threshold updated successfully");
            }
        } else if (section == "pm1") {
            if (type == "warn") {
                config->setPm1WarnThreshold(value);
                server.send(200, "text/plain", "PM1.0 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setPm1DangerThreshold(value);
                server.send(200, "text/plain", "PM1.0 Danger threshold updated successfully");
            }
        } else if (section == "pm25") {
            if (type == "warn") {
                config->setPm25WarnThreshold(value);
                server.send(200, "text/plain", "PM2.5 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setPm25DangerThreshold(value);
                server.send(200, "text/plain", "PM2.5 Danger threshold updated successfully");
            }
        } else if (section == "pm4") {
            if (type == "warn") {
                config->setPm4WarnThreshold(value);
                server.send(200, "text/plain", "PM4.0 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setPm4DangerThreshold(value);
                server.send(200, "text/plain", "PM4.0 Danger threshold updated successfully");
            }
        } else if (section == "pm10") {
            if (type == "warn") {
                config->setPm10WarnThreshold(value);
                server.send(200, "text/plain", "PM10 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setPm10DangerThreshold(value);
                server.send(200, "text/plain", "PM10 Danger threshold updated successfully");
            }
        } else if (section == "co2") {
            if (type == "warn") {
                config->setCo2WarnThreshold(value);
                server.send(200, "text/plain", "CO2 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setCo2DangerThreshold(value);
                server.send(200, "text/plain", "CO2 Danger threshold updated successfully");
            }
        } else if (section == "pressure") {
            float floatValue = valueStr.toFloat();
            if (type == "low") {
                config->setPressureLowThreshold(floatValue);
                server.send(200, "text/plain", "Pressure Low threshold updated successfully");
            } else if (type == "mid") {
                config->setPressureMidThreshold(floatValue);
                server.send(200, "text/plain", "Pressure Mid threshold updated successfully");
            } else if (type == "high") {
                config->setHighPressureThreshold(floatValue);
                server.send(200, "text/plain", "High Pressure threshold updated successfully");
            }
        } else if (section == "fan") {
            float floatValue = valueStr.toFloat();
            if (type == "on") {
                config->setFanOnCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Fan On Current threshold updated successfully");
            } else if (type == "off") {
                config->setFanOffCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Fan Off Current threshold updated successfully");
            } else if (type == "high") {
                config->setFanHighCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Fan High Current threshold updated successfully");
            }
        } else if (section == "compressor") {
            float floatValue = valueStr.toFloat();
            if (type == "on") {
                config->setCompressorOnCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Compressor On Current threshold updated successfully");
            } else if (type == "off") {
                config->setCompressorOffCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Compressor Off Current threshold updated successfully");
            } else if (type == "high") {
                config->setCompressorHighCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Compressor High Current threshold updated successfully");
            }
        } else if (section == "pump") {
            float floatValue = valueStr.toFloat();
            if (type == "on") {
                config->setPumpOnCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Pump On Current threshold updated successfully");
            } else if (type == "off") {
                config->setPumpOffCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Pump Off Current threshold updated successfully");
            } else if (type == "high") {
                config->setPumpHighCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Pump High Current threshold updated successfully");
            }
        } else if (section == "geiger") {
            float floatValue = valueStr.toFloat();
            if (type == "abnormal_low") {
                config->setGeigerAbnormalLowThreshold(floatValue);
                server.send(200, "text/plain", "Geiger Abnormal Low threshold updated successfully");
            } else if (type == "abnormal_high") {
                config->setGeigerAbnormalHighThreshold(floatValue);
                server.send(200, "text/plain", "Geiger Abnormal High threshold updated successfully");
            } else if (type == "danger_high") {
                config->setGeigerDangerHighThreshold(floatValue);
                server.send(200, "text/plain", "Geiger Danger High threshold updated successfully");
            }
        } else if (section == "temp") {
            float floatValue = valueStr.toFloat();
            if (type == "comfortable_low") {
                config->setTempComfortableLow(floatValue);
                server.send(200, "text/plain", "Temperature Comfortable Low threshold updated successfully");
            } else if (type == "comfortable_high") {
                config->setTempComfortableHigh(floatValue);
                server.send(200, "text/plain", "Temperature Comfortable High threshold updated successfully");
            } else if (type == "acceptable_low") {
                config->setTempAcceptableLow(floatValue);
                server.send(200, "text/plain", "Temperature Acceptable Low threshold updated successfully");
            } else if (type == "acceptable_high") {
                config->setTempAcceptableHigh(floatValue);
                server.send(200, "text/plain", "Temperature Acceptable High threshold updated successfully");
            }
        } else if (section == "humi") {
            float floatValue = valueStr.toFloat();
            if (type == "comfortable_low") {
                config->setHumiComfortableLow(floatValue);
                server.send(200, "text/plain", "Humidity Comfortable Low threshold updated successfully");
            } else if (type == "comfortable_high") {
                config->setHumiComfortableHigh(floatValue);
                server.send(200, "text/plain", "Humidity Comfortable High threshold updated successfully");
            } else if (type == "acceptable_low") {
                config->setHumiAcceptableLow(floatValue);
                server.send(200, "text/plain", "Humidity Acceptable Low threshold updated successfully");
            } else if (type == "acceptable_high") {
                config->setHumiAcceptableHigh(floatValue);
                server.send(200, "text/plain", "Humidity Acceptable High threshold updated successfully");
            }
        } else if (section == "inactivity") {
            int intValue = valueStr.toInt();
            if (type == "timer_delay") {
                config->setInactivityTimerDelay(intValue);
                server.send(200, "text/plain", "Inactivity timer delay updated successfully");
            }
        } else if (section == "mqtt") {
            if (type == "host") {
                config->setMqttHost(valueStr.c_str());
                server.send(200, "text/plain", "MQTT host updated successfully");
            } else if (type == "port") {
                int intValue = valueStr.toInt();
                config->setMqttPort(intValue);
                server.send(200, "text/plain", "MQTT port updated successfully");
            } else if (type == "user") {
                config->setMqttUser(valueStr.c_str());
                server.send(200, "text/plain", "MQTT username updated successfully");
            } else if (type == "password") {
                config->setMqttPassword(valueStr.c_str());
                server.send(200, "text/plain", "MQTT password updated successfully");
            } else if (type == "device_id") {
                config->setMqttDeviceId(valueStr.c_str());
                server.send(200, "text/plain", "MQTT device ID updated successfully");
            } else if (type == "device_name") {
                config->setMqttDeviceName(valueStr.c_str());
                server.send(200, "text/plain", "MQTT device name updated successfully");
            }
        } else if (section == "voc") {
            if (type == "warn") {
                config->setVocWarnThreshold(value);
                server.send(200, "text/plain", "VOC Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setVocDangerThreshold(value);
                server.send(200, "text/plain", "VOC Danger threshold updated successfully");
            }
        } else {
            server.send(400, "text/plain", "Invalid section or type");
        }
        
        logger.infof("Threshold updated: %s_%s = %d", section.c_str(), type.c_str(), value);
    } else {
        server.send(400, "text/plain", "Missing required parameters");
    }
}

void log_update_error() {
    logger.errorf("Firmware update failed. Error code: %d", Update.getError());
}

void WebServerManager::handleUpload() {
    HTTPUpload& upload = server.upload();
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
    String ip = WiFi.localIP().toString();
    String html = String(config_gas_html);
    html.replace("%IP_ADDRESS%", ip);
    
    ConfigManagerAccessor config;
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
    String ip = WiFi.localIP().toString();
    String html = String(config_pm_html);
    html.replace("%IP_ADDRESS%", ip);
    
    ConfigManagerAccessor config;
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
    String ip = WiFi.localIP().toString();
    String html = String(config_hvac_html);
    html.replace("%IP_ADDRESS%", ip);
    
    ConfigManagerAccessor config;
    html.replace("%PRESSURE_LOW%", String(config->getPressureLowThreshold()));
    html.replace("%PRESSURE_MID%", String(config->getPressureMidThreshold()));
    html.replace("%PRESSURE_HIGH%", String(config->getHighPressureThreshold()));
    html.replace("%FAN_ON%", String(config->getFanOnCurrentThreshold()));
    html.replace("%FAN_OFF%", String(config->getFanOffCurrentThreshold()));
    html.replace("%FAN_HIGH%", String(config->getFanHighCurrentThreshold()));
    html.replace("%COMPRESSOR_ON%", String(config->getCompressorOnCurrentThreshold()));
    html.replace("%COMPRESSOR_OFF%", String(config->getCompressorOffCurrentThreshold()));
    html.replace("%COMPRESSOR_HIGH%", String(config->getCompressorHighCurrentThreshold()));
    html.replace("%PUMP_ON%", String(config->getPumpOnCurrentThreshold()));
    html.replace("%PUMP_OFF%", String(config->getPumpOffCurrentThreshold()));
    html.replace("%PUMP_HIGH%", String(config->getPumpHighCurrentThreshold()));
    html.replace("%GEIGER_ABNORMAL_LOW%", String(config->getGeigerAbnormalLowThreshold()));
    html.replace("%GEIGER_ABNORMAL_HIGH%", String(config->getGeigerAbnormalHighThreshold()));
    html.replace("%GEIGER_DANGER_HIGH%", String(config->getGeigerDangerHighThreshold()));
    
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", html);
    logger.info("Served HVAC config page");
}

void WebServerManager::handleConfigClimate() {
    String ip = WiFi.localIP().toString();
    String html = String(config_climate_html);
    html.replace("%IP_ADDRESS%", ip);
    
    ConfigManagerAccessor config;
    html.replace("%TEMP_COMFORTABLE_LOW%", String(config->getTempComfortableLow()));
    html.replace("%TEMP_COMFORTABLE_HIGH%", String(config->getTempComfortableHigh()));
    html.replace("%TEMP_ACCEPTABLE_LOW%", String(config->getTempAcceptableLow()));
    html.replace("%TEMP_ACCEPTABLE_HIGH%", String(config->getTempAcceptableHigh()));
    html.replace("%HUMI_COMFORTABLE_LOW%", String(config->getHumiComfortableLow()));
    html.replace("%HUMI_COMFORTABLE_HIGH%", String(config->getHumiComfortableHigh()));
    html.replace("%HUMI_ACCEPTABLE_LOW%", String(config->getHumiAcceptableLow()));
    html.replace("%HUMI_ACCEPTABLE_HIGH%", String(config->getHumiAcceptableHigh()));
    
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", html);
    logger.info("Served climate config page");
}

void WebServerManager::handleConfigSystem() {
    String ip = WiFi.localIP().toString();
    String html = String(config_system_html);
    html.replace("%IP_ADDRESS%", ip);
    
    ConfigManagerAccessor config;
    html.replace("%INACTIVITY_TIMER_DELAY%", String(config->getInactivityTimerDelay()));
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

void WebServerManager::handleNotFound(){
  server.send(404, "text/plain", "404: Not found");
  logger.warningf("HTTP 404 Not Found for request to: %s", server.uri().c_str());
}

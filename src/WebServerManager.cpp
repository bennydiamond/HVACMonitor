#include "WebServerManager.h"
#include <WiFi.h>
#include <Update.h>
#include "Logger.h"

WebServer server(80);
// ADDED: Definition for the static member
String WebServerManager::_firmware_version;

// MODIFIED: HTML now includes a placeholder for the firmware version
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
</style>
</head>
<body>
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


// MODIFIED: init now accepts and stores the firmware version
void WebServerManager::init(const char* firmware_version) {
    _firmware_version = firmware_version;

    server.on("/", HTTP_GET, handleRoot);
    
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
    server.handleClient();
}

// MODIFIED: handleRoot now replaces the version placeholder in the HTML
void WebServerManager::handleRoot() {
    String ip = WiFi.localIP().toString();
    String html = String(update_page_html);
    html.replace("%IP_ADDRESS%", ip);
    html.replace("%FIRMWARE_VERSION%", _firmware_version);
    server.send(200, "text/html", html);
    logger.debug("Served OTA update web page.");
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

void WebServerManager::handleNotFound(){
  server.send(404, "text/plain", "404: Not found");
  logger.warningf("HTTP 404 Not Found for request to: %s", server.uri().c_str());
}

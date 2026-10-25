#include "SdUpdateManager.h"
#include "BootloopGuard.h"
#include <SPI.h>
#include <SD.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <cstring>
#include <cstdlib>

// Minimum reasonable ESP32 application binary size (64 KB)
static const size_t MIN_FIRMWARE_SIZE = 65536;

// Buffer size for reading from SD and writing to flash
static const size_t FLASH_BUFFER_SIZE = 4096;

void SdUpdateManager::trimString(char* str) {
    if (!str) return;
    char* start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    char* end = start + strlen(start);
    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\r' || *(end - 1) == '\n')) {
        end--;
    }
    *end = '\0';
    if (start != str) {
        memmove(str, start, end - start + 1);
    }
}

void SdUpdateManager::stripQuotes(char* str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len - 1] == '"') || (str[0] == '\'' && str[len - 1] == '\''))) {
        str[len - 1] = '\0';
        memmove(str, str + 1, len - 1);
    }
}

void SdUpdateManager::releaseSDBus(SPIClass& spi) {
    SD.end();
    spi.end();

    // Set SD SPI pins to high-impedance with pull-up to prevent floating pins
    // or bus contention with display/touch controllers during normal operation.
    pinMode(SD_CS_PIN, INPUT_PULLUP);
    pinMode(SD_SCK_PIN, INPUT_PULLUP);
    pinMode(SD_MISO_PIN, INPUT_PULLUP);
    pinMode(SD_MOSI_PIN, INPUT_PULLUP);
}

bool SdUpdateManager::processConfigFile(FS& fs) {
    const char* configCandidates[] = {
        "/config.txt", "/CONFIG.TXT",
        "/wifi.txt", "/WIFI.TXT"
    };

    const char* foundConfigFile = nullptr;
    for (const char* path : configCandidates) {
        if (fs.exists(path)) {
            foundConfigFile = path;
            break;
        }
    }

    if (!foundConfigFile) {
        return false;
    }

    File file = fs.open(foundConfigFile, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return false;
    }

    char wifiSsid[33] = {0};
    char wifiPass[65] = {0};
    char mqttHost[64] = {0};
    int mqttPort = 0;
    char mqttUser[32] = {0};
    char mqttPass[64] = {0};
    char mqttDeviceId[32] = {0};
    char mqttDeviceName[64] = {0};

    bool hasWifiSsid = false;
    bool hasWifiPass = false;
    bool hasMqttHost = false;
    bool hasMqttPort = false;
    bool hasMqttUser = false;
    bool hasMqttPass = false;
    bool hasMqttDeviceId = false;
    bool hasMqttDeviceName = false;

    char lineBuffer[256];
    while (file.available()) {
        size_t idx = 0;
        while (file.available() && idx < sizeof(lineBuffer) - 1) {
            char c = file.read();
            if (c == '\n') break;
            lineBuffer[idx++] = c;
        }
        lineBuffer[idx] = '\0';
        trimString(lineBuffer);

        // Skip comments and empty lines
        if (lineBuffer[0] == '\0' || lineBuffer[0] == '#' || lineBuffer[0] == ';' || 
            (lineBuffer[0] == '/' && lineBuffer[1] == '/')) {
            continue;
        }

        char* delimiter = strchr(lineBuffer, '=');
        if (!delimiter) {
            delimiter = strchr(lineBuffer, ':');
        }
        if (!delimiter) {
            continue;
        }

        *delimiter = '\0';
        char* key = lineBuffer;
        char* val = delimiter + 1;

        trimString(key);
        trimString(val);
        stripQuotes(val);

        if (strcasecmp(key, "ssid") == 0 || strcasecmp(key, "wifi_ssid") == 0 || strcasecmp(key, "wifi.ssid") == 0) {
            size_t valLen = strlen(val);
            if (valLen > 0 && valLen <= 32) {
                snprintf(wifiSsid, sizeof(wifiSsid), "%s", val);
                hasWifiSsid = true;
            }
        } else if (strcasecmp(key, "password") == 0 || strcasecmp(key, "pass") == 0 || 
                   strcasecmp(key, "wifi_password") == 0 || strcasecmp(key, "wifi_pass") == 0 || 
                   strcasecmp(key, "wifi.password") == 0) {
            size_t valLen = strlen(val);
            if (valLen == 0 || (valLen >= 8 && valLen <= 64)) {
                snprintf(wifiPass, sizeof(wifiPass), "%s", val);
                hasWifiPass = true;
            }
        } else if (strcasecmp(key, "mqtt_host") == 0 || strcasecmp(key, "broker") == 0 || 
                   strcasecmp(key, "mqtt_broker") == 0 || strcasecmp(key, "host") == 0) {
            size_t valLen = strlen(val);
            if (valLen > 0 && valLen < sizeof(mqttHost)) {
                snprintf(mqttHost, sizeof(mqttHost), "%s", val);
                hasMqttHost = true;
            }
        } else if (strcasecmp(key, "mqtt_port") == 0 || strcasecmp(key, "port") == 0) {
            int port = atoi(val);
            if (port > 0 && port <= 65535) {
                mqttPort = port;
                hasMqttPort = true;
            }
        } else if (strcasecmp(key, "mqtt_user") == 0 || strcasecmp(key, "username") == 0 || 
                   strcasecmp(key, "mqtt_username") == 0 || strcasecmp(key, "user") == 0) {
            size_t valLen = strlen(val);
            if (valLen < sizeof(mqttUser)) {
                snprintf(mqttUser, sizeof(mqttUser), "%s", val);
                hasMqttUser = true;
            }
        } else if (strcasecmp(key, "mqtt_password") == 0 || strcasecmp(key, "mqtt_pass") == 0) {
            size_t valLen = strlen(val);
            if (valLen < sizeof(mqttPass)) {
                snprintf(mqttPass, sizeof(mqttPass), "%s", val);
                hasMqttPass = true;
            }
        } else if (strcasecmp(key, "mqtt_device_id") == 0 || strcasecmp(key, "device_id") == 0) {
            size_t valLen = strlen(val);
            if (valLen > 0 && valLen < sizeof(mqttDeviceId)) {
                snprintf(mqttDeviceId, sizeof(mqttDeviceId), "%s", val);
                hasMqttDeviceId = true;
            }
        } else if (strcasecmp(key, "mqtt_device_name") == 0 || strcasecmp(key, "device_name") == 0) {
            size_t valLen = strlen(val);
            if (valLen > 0 && valLen < sizeof(mqttDeviceName)) {
                snprintf(mqttDeviceName, sizeof(mqttDeviceName), "%s", val);
                hasMqttDeviceName = true;
            }
        }
    }

    file.close();

    // Persist parsed configuration into NVS ("hvac-config" namespace)
    if (hasWifiSsid || hasWifiPass || hasMqttHost || hasMqttPort || 
        hasMqttUser || hasMqttPass || hasMqttDeviceId || hasMqttDeviceName) {
        Preferences preferences;
        if (preferences.begin("hvac-config", false)) {
            if (hasWifiSsid) {
                preferences.putString("wifi_ssid", wifiSsid);
            }
            if (hasWifiPass) {
                preferences.putString("wifi_pass", wifiPass);
            }
            if (hasMqttHost) {
                preferences.putString("mqtt_host", mqttHost);
            }
            if (hasMqttPort) {
                preferences.putInt("mqtt_port", mqttPort);
            }
            if (hasMqttUser) {
                preferences.putString("mqtt_user", mqttUser);
            }
            if (hasMqttPass) {
                preferences.putString("mqtt_password", mqttPass);
            }
            if (hasMqttDeviceId) {
                preferences.putString("mqtt_device_id", mqttDeviceId);
            }
            if (hasMqttDeviceName) {
                preferences.putString("mqtt_name", mqttDeviceName);
            }
            preferences.end();
        }
    }

    // Delete configuration file to prevent keeping credentials in plain text
    if (!fs.remove(foundConfigFile)) {
        char doneName[64];
        snprintf(doneName, sizeof(doneName), "%s.done", foundConfigFile);
        fs.rename(foundConfigFile, doneName);
    }

    return true;
}

bool SdUpdateManager::validateFirmware(File& file, size_t fileSize, size_t maxPartitionSize) {
    if (fileSize < MIN_FIRMWARE_SIZE || fileSize > maxPartitionSize) {
        return false;
    }

    // Read and verify the ESP32 binary image header
    esp_image_header_t header;
    if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
        return false;
    }

    // Check ESP32 magic byte (0xE9)
    if (header.magic != ESP_IMAGE_HEADER_MAGIC) {
        return false;
    }

    // Ensure the binary is compiled for ESP32
    if (header.chip_id != ESP_CHIP_ID_ESP32) {
        return false;
    }

    // Rewind file back to start for flashing
    file.seek(0);
    return true;
}

bool SdUpdateManager::checkForUpdate() {
    // Initialize dedicated SPI instance on VSPI with board-specific pins
    SPIClass sdSPI(VSPI);
    sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    // Attempt SD card mount at 20MHz, fall back to 4MHz if high frequency fails
    bool mounted = SD.begin(SD_CS_PIN, sdSPI, 20000000);
    if (!mounted) {
        mounted = SD.begin(SD_CS_PIN, sdSPI, 4000000);
    }

    if (!mounted) {
        releaseSDBus(sdSPI);
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        releaseSDBus(sdSPI);
        return false;
    }

    // 1. Check for and process config.txt (Wi-Fi and optional MQTT configuration)
    processConfigFile(SD);

    // 2. Check for firmware binary file (support case-insensitive match)
    const char* firmwareFileName = nullptr;
    if (SD.exists("/firmware.bin")) {
        firmwareFileName = "/firmware.bin";
    } else if (SD.exists("/FIRMWARE.BIN")) {
        firmwareFileName = "/FIRMWARE.BIN";
    }

    if (firmwareFileName == nullptr) {
        releaseSDBus(sdSPI);
        return false;
    }

    File file = SD.open(firmwareFileName, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        releaseSDBus(sdSPI);
        return false;
    }

    size_t fileSize = file.size();

    // Determine target OTA partition capacity
    const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(NULL);
    size_t maxPartitionSize = updatePartition ? updatePartition->size : 0x1F0000;

    // Validate image format, size, and magic bytes
    if (!validateFirmware(file, fileSize, maxPartitionSize)) {
        file.close();

        // Rename invalid file to .bad to prevent repeated failed attempts on boot
        char badFileName[32];
        snprintf(badFileName, sizeof(badFileName), "%s.bad", firmwareFileName);
        SD.rename(firmwareFileName, badFileName);

        releaseSDBus(sdSPI);
        return false;
    }

    if (!Update.begin(fileSize, U_FLASH)) {
        file.close();

        char failedFileName[32];
        snprintf(failedFileName, sizeof(failedFileName), "%s.failed", firmwareFileName);
        SD.rename(firmwareFileName, failedFileName);

        releaseSDBus(sdSPI);
        return false;
    }

    uint8_t* buffer = static_cast<uint8_t*>(malloc(FLASH_BUFFER_SIZE));
    if (!buffer) {
        Update.abort();
        file.close();
        releaseSDBus(sdSPI);
        return false;
    }

    size_t written = 0;
    bool writeFailed = false;

    while (file.available()) {
        size_t bytesRead = file.read(buffer, FLASH_BUFFER_SIZE);
        if (bytesRead == 0) {
            break;
        }

        if (Update.write(buffer, bytesRead) != bytesRead) {
            writeFailed = true;
            break;
        }

        written += bytesRead;
    }

    free(buffer);
    file.close();

    if (writeFailed || !Update.end(true) || !Update.isFinished()) {
        Update.abort();

        char failedFileName[32];
        snprintf(failedFileName, sizeof(failedFileName), "%s.failed", firmwareFileName);
        SD.rename(firmwareFileName, failedFileName);

        releaseSDBus(sdSPI);
        return false;
    }

    // Delete firmware.bin from MicroSD card upon successful flash
    bool fileCleared = false;
    if (SD.remove(firmwareFileName)) {
        fileCleared = true;
    } else {
        // Fallback: rename to .done if remove failed, to prevent repeated flashing
        char doneFileName[32];
        snprintf(doneFileName, sizeof(doneFileName), "%s.done", firmwareFileName);
        if (SD.rename(firmwareFileName, doneFileName)) {
            fileCleared = true;
        }
    }

    // Cleanly unmount SD card and shut down SPI bus
    releaseSDBus(sdSPI);

    // If the firmware file could not be deleted or renamed (e.g. write-protected/read-only SD card),
    // abort rebooting to prevent an infinite flash-and-reboot loop!
    if (!fileCleared) {
        return false;
    }

    // Reset bootloop guard trial flags and counter on fresh SD firmware flash
    BootloopGuard::onSdUpdate();

    // Give hardware bus a brief moment to settle before restarting
    delay(500);
    ESP.restart();
    return true;
}

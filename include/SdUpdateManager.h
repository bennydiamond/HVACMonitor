#pragma once

#include <Arduino.h>
#include <FS.h>

// Default SPI pin definitions for Sunton ESP32-2432S022C (CYD 2.2" TF card slot)
#ifndef SD_CS_PIN
#define SD_CS_PIN    5
#endif
#ifndef SD_SCK_PIN
#define SD_SCK_PIN   18
#endif
#ifndef SD_MISO_PIN
#define SD_MISO_PIN  19
#endif
#ifndef SD_MOSI_PIN
#define SD_MOSI_PIN  23
#endif

/**
 * @brief Failsafe MicroSD firmware recovery and configuration manager.
 * 
 * Performs an early boot-time check for:
 * 1. A configuration file (config.txt) to inject Wi-Fi and optional MQTT settings into NVS.
 * 2. A recovery firmware image (firmware.bin) to flash into the ESP32 OTA partition.
 * 
 * Injected config and flashed firmware files are automatically deleted upon processing.
 * Operates completely silently without Serial output to avoid interfering with the Arduino
 * Nano coprocessor UART, and fully shuts down the SPI bus before UI/runtime tasks initialize.
 */
class SdUpdateManager {
public:
    /**
     * @brief Checks for a MicroSD card and processes config.txt and/or firmware.bin at early boot.
     * 
     * @return true if a firmware update was executed (triggers a reboot), false otherwise.
     */
    static bool checkForUpdate();

private:
    static bool processConfigFile(FS& fs);
    static bool validateFirmware(File& file, size_t fileSize, size_t maxPartitionSize);
    static void releaseSDBus(class SPIClass& spi);
    static void trimString(char* str);
    static void stripQuotes(char* str);
};

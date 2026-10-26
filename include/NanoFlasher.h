#pragma once

#include <Arduino.h>
#include <functional>

class NanoFlasher {
public:
    // ATmega328P memory layout:
    // Total flash: 32768 bytes (32KB).
    // Bootloader section (512 bytes): 0x7E00 - 0x7FFF.
    // Application flash: 0x0000 - 0x7DFF (32256 bytes max = 252 pages of 128 bytes).
    static const size_t NANO_MAX_FLASH_SIZE = 32256;
    static const size_t NANO_PAGE_SIZE = 128;

    // Flash an in-memory binary image to ATmega328P via urboot over UART
    static bool flashImage(const uint8_t* image, size_t size, String& errorMsg,
                           std::function<void(size_t written, size_t total)> progressCallback = nullptr);

    // Check whether urboot is waiting on the UART (no valid SensorStack application).
    // The line must have been silent for more than urboot's 1 s timeout beforehand.
    // Blocks for up to ~0.3 s; takes the SerialMutex itself.
    static bool probeBootloader();

private:
    // Train urboot autobaud and wait for STK_INSYNC/STK_OK. Caller must hold the SerialMutex.
    static bool syncWithBootloader(int attempts, bool logAttempts);

public:

    // Stream-based uploader that handles both Intel HEX (.hex) and raw binary (.bin)
    class Uploader {
    public:
        Uploader();
        ~Uploader();

        bool begin(String& errorMsg);
        bool write(const uint8_t* data, size_t len, String& errorMsg);
        bool finish(String& errorMsg);

        size_t getFlashSize() const { return _flashSize; }
        const uint8_t* getFlashBuffer() const { return _flashBuf; }

    private:
        bool parseHexLine(const char* line, size_t len, String& errorMsg);

        uint8_t* _flashBuf;
        size_t _flashSize;
        bool _isHex;
        bool _formatDetermined;
        bool _hexEof;
        uint32_t _baseAddress;
        char _lineBuf[128];
        size_t _linePos;
    };
};

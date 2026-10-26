#include "NanoFlasher.h"
#include "NanoUartBridge.h"
#include "SensorStackStatusManager.h"
#include "SerialMutex.h"
#include "NanoCommands.h"
#include "WatchdogManager.h"
#include "Logger.h"

// urboot / STK500 protocol definitions
static const uint8_t STK_GET_SYNC        = 0x30;
static const uint8_t UR_PROG_PAGE_FL     = 0x02;
static const uint8_t STK_LEAVE_PROGMODE  = 0x51;
static const uint8_t STK_INSYNC          = 0x20;
static const uint8_t STK_OK              = 0x78;
static const uint8_t CRC_EOP             = 0x20;

// urboot leaves the bootloader through its 1 s watchdog timeout, which restarts on every received byte
static const uint32_t BOOTLOADER_TIMEOUT_MS = 1000;

bool NanoFlasher::syncWithBootloader(int attempts, bool logAttempts) {
    // Step A: Send single STK_GET_SYNC (0x30) byte to train urboot autobaud.
    // When urboot AUTOBAUD=1 starts, it measures the 5 low bits of 0x30 in a bit-banged loop
    // to compute UBRR0L, then drains the line for 25.6 bit times (~6.7ms at 19200 baud) before
    // enabling the hardware UART receiver. During this drain, any received byte is dropped.
    while (Serial.available()) {
        Serial.read();
    }
    Serial.write(STK_GET_SYNC);
    Serial.flush();
    vTaskDelay(pdMS_TO_TICKS(20)); // Allow drain loop to complete and UART receiver to initialize

    // Step B: Send standard STK_GET_SYNC + CRC_EOP (0x30 0x20)
    for (int attempt = 1; attempt <= attempts; attempt++) {
        WatchdogManager::reset();
        while (Serial.available()) {
            Serial.read();
        }

        uint8_t sync_cmd[2] = { STK_GET_SYNC, CRC_EOP };
        Serial.write(sync_cmd, 2);
        Serial.flush();

        uint32_t wait_start = millis();
        uint8_t resp[2] = {0, 0};
        int rlen = 0;
        while (millis() - wait_start < 120 && rlen < 2) {
            if (Serial.available()) {
                resp[rlen++] = Serial.read();
            } else {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }

        if (rlen == 2 && resp[0] == STK_INSYNC && resp[1] == STK_OK) {
            if (logAttempts) {
                logger.infof("[NanoFlasher] urboot synchronized successfully on attempt %d (19200 baud)", attempt);
            }
            return true;
        }
        if (logAttempts) {
            logger.warningf("[NanoFlasher] Sync attempt %d: rlen=%d, resp=0x%02X 0x%02X",
                            attempt, rlen, resp[0], resp[1]);
        }

        vTaskDelay(pdMS_TO_TICKS(25));
    }
    return false;
}

bool NanoFlasher::probeBootloader() {
    SerialMutex& serialMutex = SerialMutex::getInstance();
    if (!serialMutex.lock(pdMS_TO_TICKS(500))) {
        return false;
    }
    bool in_bootloader = syncWithBootloader(2, false);
    serialMutex.unlock();
    // Once in sync, urboot waits for the next command; going quiet lets it time out and restart
    // its autobaud, so the next probe or a firmware upload starts from a clean state.
    return in_bootloader;
}

bool NanoFlasher::flashImage(const uint8_t* image, size_t size, String& errorMsg,
                             std::function<void(size_t written, size_t total)> progressCallback) {
    if (!image || size == 0) {
        errorMsg = "Empty firmware image";
        return false;
    }
    if (size > NANO_MAX_FLASH_SIZE) {
        errorMsg = "Image exceeds maximum Nano application size (32256 bytes)";
        return false;
    }

    size_t alignedSize = ((size + NANO_PAGE_SIZE - 1) / NANO_PAGE_SIZE) * NANO_PAGE_SIZE;
    size_t totalPages = alignedSize / NANO_PAGE_SIZE;

    // On ATmega328P with a vector bootloader (VBL=1), urboot starts the application by jumping
    // to vector 25 (SPM_RDY, byte offset 0x0064). The fuses leave BOOTRST unprogrammed (hfuse 0xD7),
    // so every reset starts at 0x0000, and our urboot build is too large to include PROTECTRESET:
    // nothing on the Nano keeps the reset vector pointing at the bootloader. Just like
    // avrdude -c urclock, we must patch vector 25 with the application's real entry point from
    // vector 0, then point vector 0 at the bootloader.
    static const size_t VBL_VECTOR_OFFSET = 25 * 4; // 100 bytes = 0x64
    // jmp 0x7E80 (urboot start; word address 0x3F40)
    static const uint8_t JMP_BOOTLOADER[4] = { 0x0C, 0x94, 0x40, 0x3F };
    if (alignedSize < VBL_VECTOR_OFFSET + 4) {
        errorMsg = "Image is too small to contain a valid AVR interrupt vector table";
        return false;
    }

    uint8_t* mutableImage = const_cast<uint8_t*>(image);
    // AVR encodes 32-bit `jmp <addr>` as 0x0C 0x94 <addr_lo> <addr_hi>, where addr is a word address.
    // Only a jmp whose target lies inside the application area (below the bootloader at 0x7E00)
    // is a valid app entry point; a jmp into the bootloader (e.g. vector 0 of an image patched by
    // avrdude -c urclock) must never be copied to vector 25, or urboot would loop into itself.
    auto isAppJmp = [](const uint8_t* v) -> bool {
        uint32_t targetByteAddr = ((uint32_t)v[2] | ((uint32_t)v[3] << 8)) * 2;
        return v[0] == 0x0C && v[1] == 0x94 && targetByteAddr < NANO_MAX_FLASH_SIZE;
    };

    // Guard against corrupt images, flash dumps, or already-patched images:
    if (isAppJmp(mutableImage)) {
        // Standard unpatched binary: vector 0 is jmp __init. Copy it to vector 25.
        if (memcmp(mutableImage + VBL_VECTOR_OFFSET, mutableImage, 4) != 0) {
            logger.infof("[NanoFlasher] Patching VBL vector 25 (0x0064) with app entry point from vector 0: %02X %02X %02X %02X",
                         mutableImage[0], mutableImage[1], mutableImage[2], mutableImage[3]);
            memcpy(mutableImage + VBL_VECTOR_OFFSET, mutableImage, 4);
        }
    } else if (isAppJmp(mutableImage + VBL_VECTOR_OFFSET)) {
        // Image was already patched (e.g. read back from flash or processed by urclock):
        // Vector 25 already contains a valid 32-bit jmp __init. Preserve it!
        logger.info("[NanoFlasher] Image already has a valid jmp instruction at vector 25 (0x0064). Preserving it.");
    } else {
        errorMsg = "Invalid firmware image: neither vector 0 nor vector 25 is a jmp into the application area (may be corrupted or invalid architecture)";
        return false;
    }

    // Preserve the complete, final page 0 (containing the real vector 25 entry point), with the
    // reset vector redirected to the bootloader so every reset (USB DTR, WDT, power-on) goes
    // through urboot. Without this, the bootloader is never reached again after an OTA update.
    uint8_t finalPage0[NANO_PAGE_SIZE];
    memcpy(finalPage0, mutableImage, NANO_PAGE_SIZE);
    memcpy(finalPage0, JMP_BOOTLOADER, sizeof(JMP_BOOTLOADER));

    logger.infof("[NanoFlasher] Starting Nano flashing: %u bytes (%u pages)...", alignedSize, totalPages);

    NanoUartBridge* bridge = NanoUartBridge::getInstance();
    SensorStackStatusManager* statusManager = bridge ? bridge->getStatusManager() : nullptr;
    auto setFirmwareState = [&](SensorStackFirmwareState state) {
        if (statusManager) statusManager->setFirmwareState(state);
    };

    setFirmwareState(SensorStackFirmwareState::Updating);
    if (bridge) {
        bridge->setSuspended(true);
    }

    SerialMutex& serialMutex = SerialMutex::getInstance();
    if (!serialMutex.lock(pdMS_TO_TICKS(5000))) {
        if (bridge) bridge->setSuspended(false);
        setFirmwareState(SensorStackFirmwareState::Ok);
        errorMsg = "Timeout acquiring SerialMutex for flashing";
        return false;
    }

    // Set once page 0 (with vector 25 erased) may have been written: from then on, a failure
    // leaves urboot waiting for a new upload instead of starting the previous application.
    bool applicationErased = false;

    auto cleanupOnError = [&](const String& msg) -> bool {
        serialMutex.unlock();
        setFirmwareState(applicationErased ? SensorStackFirmwareState::Faulty
                                           : SensorStackFirmwareState::Ok);
        if (bridge) {
            bridge->setSuspended(false);
            bridge->restartInitSequence();
        }
        errorMsg = msg;
        logger.errorf("[NanoFlasher] %s", msg.c_str());
        return false;
    };

    // 1. Drain residual serial bytes
    while (Serial.available()) {
        Serial.read();
    }

    // 2. Request bootloader entry via CMD_BOOTLOADER and wait for ACK (<b,...>)
    char cmdBuf[2] = { CMD_BOOTLOADER, '\0' };
    uint8_t cs = NanoUartBridge::calculateChecksum(cmdBuf);
    bool bootloader_acked = false;

    for (int retry = 1; retry <= 3; retry++) {
        while (Serial.available()) {
            Serial.read();
        }

        // Clear any partial packet on Nano
        Serial.print('>');
        Serial.flush();
        vTaskDelay(pdMS_TO_TICKS(10));

        Serial.printf("<%c,%u>", CMD_BOOTLOADER, cs);
        Serial.flush();
        logger.infof("[NanoFlasher] Sent bootloader command <B,%u> (attempt %d/3). Waiting for ack...", cs, retry);

        uint32_t ack_start = millis();
        String ackLine = "";
        while (millis() - ack_start < 250) {
            if (Serial.available()) {
                char c = Serial.read();
                if (c == '<') {
                    ackLine = "";
                } else if (c == '>') {
                    if (ackLine.startsWith("b,") || ackLine.equals("b")) {
                        bootloader_acked = true;
                        logger.infof("[NanoFlasher] Received bootloader ACK: <%s>", ackLine.c_str());
                        break;
                    }
                } else if (c >= ' ') {
                    ackLine += c;
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
        if (bootloader_acked) {
            break;
        }
    }

    if (!bootloader_acked) {
        logger.warning("[NanoFlasher] No ACK received for <B>; continuing in case Nano is already entering reset...");
    }

    // 3. Wait 120ms for Nano watchdog reset (~15-30ms) and crystal stabilization delay (66ms)
    vTaskDelay(pdMS_TO_TICKS(120));

    while (Serial.available()) {
        Serial.read();
    }

    // 4. Synchronize with urboot autobaud
    if (!syncWithBootloader(10, true)) {
        return cleanupOnError("Failed to synchronize with Nano urboot bootloader");
    }

    // 5. Flash pages (128 bytes each)
    // Fail-safe strategy:
    // Page 0 is initially written with vector 25 (0x0064) set to 0xFFFFFFFF.
    // If flashing is aborted or interrupted at ANY point before completion,
    // urboot detects 0xFFFF at vector 25 on boot and stays in the bootloader.
    // Once all pages are successfully written, page 0 is rewritten with the real
    // vector 25 entry point (finalPage0) to activate the application.
    uint8_t stagedPage0[NANO_PAGE_SIZE];
    memcpy(stagedPage0, finalPage0, NANO_PAGE_SIZE);
    memset(stagedPage0 + VBL_VECTOR_OFFSET, 0xFF, 4);

    uint8_t pageReq[4 + NANO_PAGE_SIZE + 1];

    auto writePage = [&](uint16_t byteAddr, const uint8_t* pageData) -> bool {
        WatchdogManager::reset();
        pageReq[0] = UR_PROG_PAGE_FL;
        pageReq[1] = byteAddr & 0xFF;        // Addr low
        pageReq[2] = (byteAddr >> 8) & 0xFF; // Addr high
        pageReq[3] = NANO_PAGE_SIZE;         // 128 bytes
        memcpy(pageReq + 4, pageData, NANO_PAGE_SIZE);
        pageReq[4 + NANO_PAGE_SIZE] = CRC_EOP; // 0x20

        Serial.write(pageReq, sizeof(pageReq));
        Serial.flush();

        // Wait for response: 0x20 0x78 (ATmega328P erase + write takes ~8-10ms)
        uint32_t wait_start = millis();
        uint8_t resp[2] = {0, 0};
        int rlen = 0;
        while (millis() - wait_start < 400 && rlen < 2) {
            if (Serial.available()) {
                resp[rlen++] = Serial.read();
            } else {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }

        return (rlen == 2 && resp[0] == STK_INSYNC && resp[1] == STK_OK);
    };

    // Phase 1: Write page 0 (with vector 25 erased) followed by pages 1 .. totalPages-1
    for (size_t page = 0; page < totalPages; page++) {
        uint16_t byteAddr = page * NANO_PAGE_SIZE;
        const uint8_t* pData = (page == 0) ? stagedPage0 : (mutableImage + byteAddr);
        if (page == 0) {
            applicationErased = true;
        }

        if (!writePage(byteAddr, pData)) {
            char failMsg[96];
            snprintf(failMsg, sizeof(failMsg), "Flash write failed at page %u (addr 0x%04X)",
                     (unsigned)page, (unsigned)byteAddr);
            return cleanupOnError(failMsg);
        }

        if (progressCallback) {
            progressCallback((page + 1) * NANO_PAGE_SIZE, alignedSize);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // Phase 2: Commit real application vector 25 by rewriting page 0
    logger.info("[NanoFlasher] Committing final application entry vector at 0x0064...");
    if (!writePage(0x0000, finalPage0)) {
        return cleanupOnError("Flash write failed during final vector 25 commit at page 0");
    }

    logger.infof("[NanoFlasher] Flashed all %u pages (%u bytes) successfully!", totalPages, alignedSize);

    // 6. Leave programming mode (STK_LEAVE_PROGMODE + CRC_EOP -> 0x51 0x20)
    uint8_t leave_cmd[2] = { STK_LEAVE_PROGMODE, CRC_EOP };
    Serial.write(leave_cmd, 2);
    Serial.flush();

    uint32_t leave_start = millis();
    while (millis() - leave_start < 100 && Serial.available() < 2) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    while (Serial.available()) {
        Serial.read();
    }

    // 7. This urboot build has no QEXITEND, so STK_LEAVE_PROGMODE is only acknowledged and does not
    // exit. The bootloader leaves through its 1 s watchdog timeout, which restarts on every byte it
    // receives. Keep the line silent until then, or the bridge's init commands land in the
    // bootloader instead of the new application.
    for (uint32_t waited = 0; waited < BOOTLOADER_TIMEOUT_MS + 200; waited += 100) {
        WatchdogManager::reset();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    serialMutex.unlock();
    setFirmwareState(SensorStackFirmwareState::Ok);
    if (bridge) {
        bridge->setSuspended(false);
        bridge->restartInitSequence();
    }

    logger.info("[NanoFlasher] Arduino Nano firmware update complete. Re-initialized sensor hub bridge.");
    return true;
}

// =========================================================================
// Uploader helper implementation
// =========================================================================

NanoFlasher::Uploader::Uploader()
    : _flashBuf(nullptr),
      _flashSize(0),
      _isHex(false),
      _formatDetermined(false),
      _hexEof(false),
      _baseAddress(0),
      _linePos(0) {
    _lineBuf[0] = '\0';
}

NanoFlasher::Uploader::~Uploader() {
    if (_flashBuf) {
        free(_flashBuf);
        _flashBuf = nullptr;
    }
}

bool NanoFlasher::Uploader::begin(String& errorMsg) {
    if (!_flashBuf) {
        _flashBuf = (uint8_t*)malloc(NANO_MAX_FLASH_SIZE);
    }
    if (!_flashBuf) {
        errorMsg = "Out of memory allocating Nano flash staging buffer";
        return false;
    }
    memset(_flashBuf, 0xFF, NANO_MAX_FLASH_SIZE);
    _flashSize = 0;
    _isHex = false;
    _formatDetermined = false;
    _hexEof = false;
    _baseAddress = 0;
    _linePos = 0;
    return true;
}

static inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static inline int hexByte(const char* s) {
    int h1 = hexNibble(s[0]);
    int h2 = hexNibble(s[1]);
    if (h1 < 0 || h2 < 0) return -1;
    return (h1 << 4) | h2;
}

bool NanoFlasher::Uploader::parseHexLine(const char* line, size_t len, String& errorMsg) {
    while (len > 0 && (*line == ' ' || *line == '\t')) {
        line++;
        len--;
    }
    if (len == 0) return true;
    if (*line != ':') {
        errorMsg = "Invalid Intel HEX line: missing ':' start character";
        return false;
    }
    line++;
    len--;

    if (len < 10) {
        errorMsg = "Intel HEX line is too short";
        return false;
    }

    int byteCount = hexByte(line);
    if (byteCount < 0) {
        errorMsg = "Invalid byte count in Intel HEX line";
        return false;
    }

    size_t expectedHexChars = (1 + 2 + 1 + byteCount + 1) * 2;
    if (len < expectedHexChars) {
        errorMsg = "Intel HEX line truncated";
        return false;
    }

    uint8_t sum = 0;
    for (size_t i = 0; i < expectedHexChars; i += 2) {
        int b = hexByte(line + i);
        if (b < 0) {
            errorMsg = "Invalid hex character in Intel HEX line";
            return false;
        }
        sum += (uint8_t)b;
    }
    if (sum != 0) {
        errorMsg = "Intel HEX checksum mismatch";
        return false;
    }

    int addr = (hexByte(line + 2) << 8) | hexByte(line + 4);
    int type = hexByte(line + 6);

    if (type == 0x00) {
        uint32_t fullAddr = _baseAddress + addr;
        if (fullAddr + byteCount > NANO_MAX_FLASH_SIZE) {
            errorMsg = "Firmware exceeds Nano application flash boundary (32256 bytes)";
            return false;
        }
        for (int k = 0; k < byteCount; k++) {
            _flashBuf[fullAddr + k] = (uint8_t)hexByte(line + 8 + k * 2);
        }
        if (fullAddr + byteCount > _flashSize) {
            _flashSize = fullAddr + byteCount;
        }
    } else if (type == 0x01) {
        _hexEof = true;
    } else if (type == 0x02) {
        if (byteCount >= 2) {
            int seg = (hexByte(line + 8) << 8) | hexByte(line + 10);
            _baseAddress = (uint32_t)seg << 4;
        }
    } else if (type == 0x04) {
        if (byteCount >= 2) {
            int lin = (hexByte(line + 8) << 8) | hexByte(line + 10);
            _baseAddress = (uint32_t)lin << 16;
        }
    }
    return true;
}

bool NanoFlasher::Uploader::write(const uint8_t* data, size_t len, String& errorMsg) {
    if (!_flashBuf) {
        errorMsg = "Uploader buffer not allocated";
        return false;
    }
    if (len == 0) return true;

    size_t idx = 0;
    if (!_formatDetermined) {
        while (idx < len && (data[idx] == ' ' || data[idx] == '\t' || data[idx] == '\r' || data[idx] == '\n')) {
            idx++;
        }
        if (idx < len) {
            _isHex = (data[idx] == ':');
            _formatDetermined = true;
        }
    }

    if (_formatDetermined && !_isHex) {
        if (_flashSize + (len - idx) > NANO_MAX_FLASH_SIZE) {
            errorMsg = "Binary firmware exceeds Nano application flash (32256 bytes max)";
            return false;
        }
        memcpy(_flashBuf + _flashSize, data + idx, len - idx);
        _flashSize += (len - idx);
        return true;
    }

    for (size_t i = idx; i < len; i++) {
        char c = (char)data[i];
        if (c == '\r' || c == '\n') {
            if (_linePos > 0) {
                _lineBuf[_linePos] = '\0';
                if (!parseHexLine(_lineBuf, _linePos, errorMsg)) {
                    return false;
                }
                _linePos = 0;
            }
        } else {
            if (_linePos < sizeof(_lineBuf) - 1) {
                _lineBuf[_linePos++] = c;
            } else {
                errorMsg = "Intel HEX line length exceeded buffer";
                return false;
            }
        }
    }
    return true;
}

bool NanoFlasher::Uploader::finish(String& errorMsg) {
    if (_linePos > 0) {
        _lineBuf[_linePos] = '\0';
        if (!parseHexLine(_lineBuf, _linePos, errorMsg)) {
            return false;
        }
        _linePos = 0;
    }

    if (_flashSize == 0) {
        errorMsg = "No firmware code found in uploaded file";
        return false;
    }

    _flashSize = ((_flashSize + NANO_PAGE_SIZE - 1) / NANO_PAGE_SIZE) * NANO_PAGE_SIZE;
    return true;
}

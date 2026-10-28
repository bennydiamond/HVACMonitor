#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Esp32Architecture {
    // Binary upload headers, flash boundaries, and validation constraints
    constexpr size_t IMAGE_HEADER_SIZE = 0x24; // 36-byte ESP image header
    constexpr size_t APP_DESC_OFFSET = 0x20;   // Offset of app descriptor magic word
    constexpr size_t BOOTLOADER_STAGING_BUFFER_SIZE = 0x7000; // 28 KB staging buffer
    constexpr size_t BOOTLOADER_MIN_SIZE_BYTES = 4096;
    constexpr size_t BOOTLOADER_FLASH_OFFSET = 0x1000;
    constexpr size_t FLASH_SECTOR_SIZE = 4096;
    constexpr uint32_t IRAM_ENTRY_ADDR_MIN = 0x40078000;
    constexpr uint32_t IRAM_ENTRY_ADDR_MAX = 0x40085FFF;
}

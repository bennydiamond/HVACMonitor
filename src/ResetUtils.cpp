#include "ResetUtils.h"
#include <esp_system.h>

const char* get_reset_reason_string() {
    switch (esp_reset_reason()) {
        case ESP_RST_UNKNOWN:    return "Unknown";
        case ESP_RST_POWERON:    return "Power On";
        case ESP_RST_EXT:        return "External";
        case ESP_RST_SW:         return "Software";
        case ESP_RST_PANIC:      return "Panic";
        case ESP_RST_INT_WDT:    return "Interrupt WDT";
        case ESP_RST_TASK_WDT:   return "Task WDT";
        case ESP_RST_WDT:        return "Other WDT";
        case ESP_RST_DEEPSLEEP:  return "Deep Sleep";
        case ESP_RST_BROWNOUT:   return "Brownout";
        case ESP_RST_SDIO:       return "SDIO";
        default:                 return "No reason";
    }
} 
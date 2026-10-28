#pragma once

#include <Arduino.h>

class FilterPressureTracker;

class RebootManager {
public:
    // Initialize the singleton instance with reference to FilterPressureTracker
    static void init(FilterPressureTracker& filterTracker);

    // Access the singleton instance
    static RebootManager& getInstance();

    // Reboot delay constants
    static constexpr uint32_t WEB_REBOOT_DELAY_MS = 2000;
    static constexpr uint32_t OTA_REBOOT_DELAY_MS = 500;

    // Centralized method to request a reboot with logging, state flushing, and controlled delay
    void requestReboot(const char* reason, uint32_t delayMs = 500);

    // Called when an OTA update starts to ensure all pending state is persisted before flash overwrite
    void prepareForOta(const char* otaType);

private:
    explicit RebootManager(FilterPressureTracker& filterTracker);
    ~RebootManager() = default;
    RebootManager(const RebootManager&) = delete;
    RebootManager& operator=(const RebootManager&) = delete;

    static RebootManager* _instance;
    FilterPressureTracker& _filterTracker;
};

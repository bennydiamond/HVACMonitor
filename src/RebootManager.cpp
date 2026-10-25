#include "RebootManager.h"
#include "FilterPressureTracker.h"
#include "Logger.h"
#include "BootloopGuard.h"

RebootManager* RebootManager::_instance = nullptr;

void RebootManager::init(FilterPressureTracker& filterTracker) {
    if (!_instance) {
        _instance = new RebootManager(filterTracker);
        logger.info("RebootManager initialized.");
    }
}

RebootManager& RebootManager::getInstance() {
    return *_instance;
}

RebootManager::RebootManager(FilterPressureTracker& filterTracker) :
    _filterTracker(filterTracker)
{
}

void RebootManager::requestReboot(const char* reason, uint32_t delayMs) {
    logger.warningf("RebootManager: Reboot requested (reason: %s). Flushing states...", reason ? reason : "Unknown");
    
    // Flush all sensor tracking states to NVS
    _filterTracker.flush();

    // Ensure serial logs are flushed
    Serial.flush();

    // Clear RTC crash count so intentional reboots do not trigger bootloop guard
    BootloopGuard::onGracefulShutdown();

    if (delayMs > 0) {
        delay(delayMs);
    }

    logger.warning("RebootManager: Executing ESP.restart() now.");
    ESP.restart();
}

void RebootManager::prepareForOta(const char* otaType) {
    logger.infof("RebootManager: Preparing for %s OTA update. Flushing states...", otaType ? otaType : "OTA");
    
    // Flush all sensor tracking states to NVS before flash partition is written
    _filterTracker.flush();
    Serial.flush();
}

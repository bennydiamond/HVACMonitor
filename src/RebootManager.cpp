#include "RebootManager.h"
#include "FilterPressureTracker.h"
#include "Logger.h"
#include "BootloopGuard.h"
#include <esp_attr.h>

RebootManager* RebootManager::_instance = nullptr;

// Reason of the last requested reboot, kept across the restart (RTC memory is not
// cleared by esp_restart): logged at the next boot, since the request's own log
// lines may not reach syslog before the restart
static const uint32_t REBOOT_REASON_MAGIC = 0x52424F54; // "RBOT"
RTC_NOINIT_ATTR static uint32_t s_rebootReasonMagic;
RTC_NOINIT_ATTR static char s_rebootReason[48];

void RebootManager::init(FilterPressureTracker& filterTracker) {
    if (!_instance) {
        _instance = new RebootManager(filterTracker);
        logger.info("RebootManager initialized.");
        if (s_rebootReasonMagic == REBOOT_REASON_MAGIC) {
            s_rebootReason[sizeof(s_rebootReason) - 1] = '\0';
            logger.warningf("RebootManager: previous restart was requested (reason: %s).",
                            s_rebootReason);
        }
        s_rebootReasonMagic = 0;
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

    snprintf(s_rebootReason, sizeof(s_rebootReason), "%s", reason ? reason : "Unknown");
    s_rebootReasonMagic = REBOOT_REASON_MAGIC;

    // Called from the main task: keep the logger sending meanwhile, so the lines
    // above (and whoever requested the reboot) reach syslog before the restart
    unsigned long start = millis();
    while (millis() - start < delayMs) {
        logger.loop();
        delay(10);
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

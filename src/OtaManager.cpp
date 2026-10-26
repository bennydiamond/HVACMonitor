#include <ArduinoOTA.h>
#include "OtaManager.h"
#include "Logger.h"
#include "UITask.h"
#include "RebootManager.h"
#include "BootloopGuard.h"
#ifdef ENABLE_TASK_WATCHDOG
#include "WatchdogManager.h"
#endif

void OtaManager::init() {
    ArduinoOTA.setHostname("hvac-sensor-display");

    ArduinoOTA.onStart([]() {
        //UITask::getInstance().queue_ota_popup();
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        logger.infof("OTA update started for %s. Free: %lu, Min: %lu, Max: %lu bytes", type.c_str(), ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    });

    ArduinoOTA.onEnd([]() {
        logger.info("OTA update finished successfully. Flushing states before reboot...");
        BootloopGuard::markOtaTrialPending();
        RebootManager::getInstance().prepareForOta("ArduinoOTA");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // ArduinoOTA receives the whole image inside handle(): feed the watchdog meanwhile
#ifdef ENABLE_TASK_WATCHDOG
        WatchdogManager::reset();
#endif
        static uint8_t lastPercent = 0;
        uint8_t percent = progress / (total / 100);
        if (percent != lastPercent) {
            lastPercent = percent;
            logger.debugf("OTA Progress: %u%% Free: %lu, Min: %lu, Max: %lu bytes\r", percent, ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        char error_msg[32];
        if (error == OTA_AUTH_ERROR) strcpy(error_msg, "Auth Failed");
        else if (error == OTA_BEGIN_ERROR) strcpy(error_msg, "Begin Failed");
        else if (error == OTA_CONNECT_ERROR) strcpy(error_msg, "Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) strcpy(error_msg, "Receive Failed");
        else if (error == OTA_END_ERROR) strcpy(error_msg, "End Failed");
        else strcpy(error_msg, "Unknown Error");
        
        logger.errorf("OTA Error[%u]: %s Free: %lu, Min: %lu, Max: %lu bytes", error, error_msg, ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
        //ESP.restart();  // Restart the device on OTA error to recover
    });

    ArduinoOTA.begin();
    logger.info("ArduinoOTA initialized.");
}

void OtaManager::handle() {
    ArduinoOTA.handle();
}

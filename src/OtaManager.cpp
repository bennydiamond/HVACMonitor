#include <ArduinoOTA.h>
#include "OtaManager.h"
#include "Logger.h"

void OtaManager::init() {
    ArduinoOTA.setHostname("hvac-sensor-display");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        logger.infof("OTA update started for %s.", type.c_str());
    });

    ArduinoOTA.onEnd([]() {
        logger.info("OTA update finished successfully.");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        logger.debugf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        char error_msg[32];
        if (error == OTA_AUTH_ERROR) strcpy(error_msg, "Auth Failed");
        else if (error == OTA_BEGIN_ERROR) strcpy(error_msg, "Begin Failed");
        else if (error == OTA_CONNECT_ERROR) strcpy(error_msg, "Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) strcpy(error_msg, "Receive Failed");
        else if (error == OTA_END_ERROR) strcpy(error_msg, "End Failed");
        else strcpy(error_msg, "Unknown Error");
        
        logger.errorf("OTA Error[%u]: %s", error, error_msg);
    });

    ArduinoOTA.begin();
    logger.info("ArduinoOTA initialized.");
}

void OtaManager::handle() {
    ArduinoOTA.handle();
}

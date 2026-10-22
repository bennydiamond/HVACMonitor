#pragma once

#include <WebServer.h>

class WebServerManager {
public:
    static void init(const char* firmware_version);
    static void handle();

private:
    static char _firmware_version[16];
    
    static void handleRoot();
    static void handleOTA();
    static void handleApiStatus();
    static void handleConfig();
    static void handleConfigUpdate();
    static void handleUpload();
    static void handleNotFound();

    static void handleConfigGas();
    static void handleConfigPM();
    static void handleConfigHVAC();
    static void handleConfigClimate();
    static void handleConfigSystem();
    static void handleConfigMaintenance();
};
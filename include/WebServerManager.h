#pragma once

#include <WebServer.h>
#include "ConfigManager.h"

class WebServerManager {
public:
    void init(const char* firmware_version);
    void handle();

private:
    static void handleRoot();
    static void handleConfig();
    static void handleConfigUpdate();
    static void handleUpload();
    static void handleNotFound();

    static String _firmware_version;
};

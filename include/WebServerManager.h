#pragma once

#include <WebServer.h>

class WebServerManager {
public:
    // MODIFIED: Added firmware_version parameter
    void init(const char* firmware_version);
    void handle();

private:
    static void handleRoot();
    static void handleUpload();
    static void handleNotFound();

    // ADDED: Static member to store firmware version
    static String _firmware_version;
};

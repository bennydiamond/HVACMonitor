#pragma once

#include <WebServer.h>

class WebServerManager {
public:
    void init(const char* firmware_version);
    void handle();

private:
    static void handleRoot();
    static void handleUpload();
    static void handleNotFound();

    static String _firmware_version;
};

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoHA.h>

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

#include <atomic>

enum AppLogLevel {
    APP_LOG_DEBUG,
    APP_LOG_INFO,
    APP_LOG_WARNING,
    APP_LOG_ERROR
};

class Logger {
public:
    Logger();
    void init(HAMqtt* mqtt);
    void loop();

    void setLogLevel(AppLogLevel level);
    AppLogLevel getLogLevel() const;

    void log(AppLogLevel level, const char* message);

    // Helper methods
    void debug(const char* message) { log(APP_LOG_DEBUG, message); }
    void info(const char* message) { log(APP_LOG_INFO, message); }
    void warning(const char* message) { log(APP_LOG_WARNING, message); }
    void error(const char* message) { log(APP_LOG_ERROR, message); }

    void debugf(const char* format, ...);
    void infof(const char* format, ...);
    void warningf(const char* format, ...);
    void errorf(const char* format, ...);

private:
    static const size_t LOG_BUFFER_SIZE_BYTES = 4096;
    static const unsigned long PING_CHECK_INTERVAL_MS = 5000;
    static const int MAX_LOGS_PER_LOOP = 5;
    static const size_t MAX_MESSAGE_LENGTH = 255;
    static const uint16_t SYSLOG_PORT = 514;
    static constexpr const char* SYSLOG_SERVER = "192.168.0.15";
    static constexpr const char* DEVICE_HOSTNAME = "hvac-sensor-display";
    static constexpr const char* MQTT_LOG_TOPIC = "aha/hvac_diff_pressure_sensor_01/log";

#pragma pack(push, 1)
struct LogMessage {
    AppLogLevel level;
    uint8_t msgLen;
    char message[Logger::MAX_MESSAGE_LENGTH];
};
#pragma pack(pop)

    StreamBufferHandle_t _logStreamBufferHandle = nullptr;
    WiFiUDP _udpClient;
    Syslog* _syslog = nullptr;
    HAMqtt* _mqtt = nullptr;
    std::atomic<AppLogLevel> _currentLogLevel{APP_LOG_INFO};
    bool _syslogServerReachable = false;
    unsigned long _lastPingTime = 0;

    const char* _getLogLevelString(AppLogLevel level);
};

extern Logger logger;

#endif // LOGGER_H
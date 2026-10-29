#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <WiFi.h>

#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"

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
    static const int MAX_LOGS_PER_LOOP = 5;
    static const size_t MAX_MESSAGE_LENGTH = 255;
    static const uint16_t SYSLOG_PORT = 514; // rsyslog imtcp
    static const unsigned long SYSLOG_RECONNECT_INTERVAL_MS = 5000;
    static const unsigned long SYSLOG_CONNECT_TIMEOUT_MS = 3000;
    // TCP keepalive: a host that vanishes silently (no reset) is detected on an idle
    // connection after ~idle + interval * count = 25 s
    static const int SYSLOG_KEEPALIVE_IDLE_S = 10;
    static const int SYSLOG_KEEPALIVE_INTERVAL_S = 5;
    static const int SYSLOG_KEEPALIVE_COUNT = 3;
    static constexpr const char* SYSLOG_APP_NAME = "HVAC_Sensor";
    static constexpr const char* SYSLOG_SERVER = "192.168.0.15";
    static constexpr const char* DEVICE_HOSTNAME = "hvac-sensor-display";

#pragma pack(push, 1)
struct LogMessage {
    AppLogLevel level;
    uint8_t msgLen;
    char message[Logger::MAX_MESSAGE_LENGTH + 1]; // Room for the terminator added on receive
};
#pragma pack(pop)

    MessageBufferHandle_t _logStreamBufferHandle = nullptr;

    // Syslog over TCP (main loop only). A message leaves the queue only into _line, and
    // _line is only released once fully written to an established connection: while
    // rsyslog is unreachable, logs stay queued. All socket calls are non-blocking.
    int _syslogSocket = -1;
    bool _syslogConnecting = false;
    bool _syslogWasConnected = false;
    unsigned long _syslogConnectStartedAt = 0;
    char _line[MAX_MESSAGE_LENGTH + 96]; // "<PRI>1 - host app - - - message\n"
    size_t _lineLen = 0;                 // 0: no message pending
    size_t _lineSent = 0;

    bool _syslogReady();
    void _syslogClose();
    bool _syslogSendLine();
    void _formatLine(AppLogLevel level, const char* message);
    std::atomic<AppLogLevel> _currentLogLevel{APP_LOG_INFO};
    // Messages dropped because the buffer was full (e.g. a long Wi-Fi outage), any task
    std::atomic<uint32_t> _droppedMessages{0};

    const char* _getLogLevelString(AppLogLevel level);
};

extern Logger logger;

#endif // LOGGER_H

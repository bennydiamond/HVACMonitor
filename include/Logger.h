#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoHA.h>
#include <vector>
#include <string>

// Define application-specific severity levels to avoid conflicts with Syslog.h
enum AppLogLevel {
    APP_LOG_DEBUG,
    APP_LOG_INFO,
    APP_LOG_WARNING,
    APP_LOG_ERROR
};

// Struct to hold a single log entry for the queue
struct LogEntry {
    AppLogLevel level;
    std::string message;
};

// Define the maximum number of logs to queue while offline
#define MAX_LOG_QUEUE_SIZE 30
// Check syslog server reachability every 5 seconds
#define PING_CHECK_INTERVAL_MS 5000

class Logger {
public:
    Logger();
    void init(HAMqtt* mqtt);
    void loop(); // Method to be called in the main loop to handle queue flushing
    void log(AppLogLevel level, const char* message);
    void logf(AppLogLevel level, const char* format, ...);

    // Helper methods for different levels
    void debug(const char* message) { log(APP_LOG_DEBUG, message); }
    void info(const char* message) { log(APP_LOG_INFO, message); }
    void warning(const char* message) { log(APP_LOG_WARNING, message); }
    void error(const char* message) { log(APP_LOG_ERROR, message); }

    void debugf(const char* format, ...);
    void infof(const char* format, ...);
    void warningf(const char* format, ...);
    void errorf(const char* format, ...);

private:
    WiFiUDP _udpClient;
    Syslog* _syslog = nullptr;
    HAMqtt* _mqtt = nullptr;
    bool _isLogging = false; // Re-entrancy guard
    bool _syslogServerReachable = false; // Flag to indicate if the syslog server is reachable
    unsigned long _lastPingTime = 0; // Timer for ping checks

    std::vector<LogEntry> _logQueue; // Queue for offline messages

    const char* _getLogLevelString(AppLogLevel level);
    void _sendSyslog(AppLogLevel level, const char* message);
    void _queueLog(AppLogLevel level, const char* message);
    void _flushLogQueue();
};

// Declare a global logger instance
extern Logger logger;

#endif // LOGGER_H

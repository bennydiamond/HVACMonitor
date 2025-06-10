#include "Logger.h"
#include <ESP32Ping.h>

// Syslog server configuration
#define SYSLOG_SERVER "192.168.0.15"
#define SYSLOG_PORT 514
#define DEVICE_HOSTNAME "hvac-sensor-display"

// MQTT Log Topic
#define MQTT_LOG_TOPIC "aha/hvac_diff_pressure_sensor_01/log"

Logger logger; // Define the global logger instance

Logger::Logger() : _isLogging(false), _syslogServerReachable(false), _lastPingTime(0) {
    // Constructor
}

void Logger::init(HAMqtt* mqtt) {
    _mqtt = mqtt;
    _syslog = new Syslog(_udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, "HVAC_Sensor", LOG_KERN);
    _logQueue.reserve(MAX_LOG_QUEUE_SIZE);
}

void Logger::loop() {
    // Only perform network checks if WiFi is connected.
    if (WiFi.status() == WL_CONNECTED) {
        // Periodically check if the syslog server is reachable.
        if (millis() - _lastPingTime >= PING_CHECK_INTERVAL_MS) {
            _lastPingTime = millis();
            
            // Use the ESP32Ping library. The '1' is the number of pings to send.
            bool isReachable = Ping.ping(SYSLOG_SERVER, 1);

            // If the server just became reachable, update the state and flush the queue.
            if (isReachable && !_syslogServerReachable) {
                _syslogServerReachable = true;
                if (!_logQueue.empty()) {
                    infof("Syslog server is reachable. Flushing %d queued log messages.", _logQueue.size());
                    _flushLogQueue();
                } else {
                    info("Syslog server is reachable.");
                }
            } 
            // If the server just became unreachable, update the state.
            else if (!isReachable && _syslogServerReachable) {
                _syslogServerReachable = false;
                warning("Syslog server has become unreachable. Queuing logs.");
            }
        }
    } else {
        // If WiFi is disconnected, the server is definitely not reachable.
        if (_syslogServerReachable) {
            warning("WiFi disconnected. Syslog server is now unreachable.");
            _syslogServerReachable = false;
        }
    }
}

void Logger::log(AppLogLevel level, const char* message) {
    if (_isLogging) { return; }
    _isLogging = true;

    // 1. Always send to Serial
    Serial.printf("[%s] %s\n", _getLogLevelString(level), message);

    // 2. Always try to send to MQTT if connected
    if (_mqtt != nullptr && _mqtt->isConnected()) {
        char payload[512];
        snprintf(payload, sizeof(payload), "{\"level\": \"%s\", \"message\": \"%s\"}", _getLogLevelString(level), message);
        _mqtt->publish(MQTT_LOG_TOPIC, payload);
    }

    // 3. Handle Syslog: If the server is confirmed reachable, send directly. Otherwise, queue.
    if (_syslogServerReachable) {
        if (!_logQueue.empty()){
            _flushLogQueue();
        }
        _sendSyslog(level, message);
    } else {
        _queueLog(level, message);
    }

    _isLogging = false;
}

void Logger::_sendSyslog(AppLogLevel level, const char* message) {
    if (_syslog != nullptr) {
        switch (level) {
            case APP_LOG_DEBUG:   _syslog->log(LOG_DEBUG, message);   break;
            case APP_LOG_INFO:    _syslog->log(LOG_INFO, message);    break;
            case APP_LOG_WARNING: _syslog->log(LOG_WARNING, message); break;
            case APP_LOG_ERROR:   _syslog->log(LOG_ERR, message);     break;
        }
    }
}

void Logger::_queueLog(AppLogLevel level, const char* message) {
    if (_logQueue.size() < MAX_LOG_QUEUE_SIZE) {
        LogEntry entry = {level, message};
        _logQueue.push_back(entry);
    } else {
        Serial.println("[WARNING] Log queue is full. Discarding oldest message.");
        _logQueue.erase(_logQueue.begin());
        LogEntry entry = {level, message};
        _logQueue.push_back(entry);
    }
}

void Logger::_flushLogQueue() {
    for (const auto& entry : _logQueue) {
        _sendSyslog(entry.level, entry.message.c_str());
        delay(5); // Small delay to prevent overwhelming the network stack
    }
    _logQueue.clear();
}

void Logger::logf(AppLogLevel level, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(level, buffer);
}

void Logger::debugf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(APP_LOG_DEBUG, buffer);
}

void Logger::infof(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(APP_LOG_INFO, buffer);
}

void Logger::warningf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(APP_LOG_WARNING, buffer);
}

void Logger::errorf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(APP_LOG_ERROR, buffer);
}

const char* Logger::_getLogLevelString(AppLogLevel level) {
    switch (level) {
        case APP_LOG_DEBUG: return "DEBUG";
        case APP_LOG_INFO: return "INFO";
        case APP_LOG_WARNING: return "WARNING";
        case APP_LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

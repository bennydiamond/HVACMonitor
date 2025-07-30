#include "Logger.h"
#include <ESP32Ping.h>
#include <cstdarg>
#include <cstddef>
#ifdef SERIAL_OUT_DEBUG 
#include "SerialMutex.h"
#endif

Logger logger;

Logger::Logger() {
    _logStreamBufferHandle = xStreamBufferCreate(LOG_BUFFER_SIZE_BYTES, 1);
}

void Logger::init(HAMqtt* mqtt) {
    _mqtt = mqtt;
    _syslog = new Syslog(_udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, "HVAC_Sensor", LOG_KERN);
}

void Logger::setLogLevel(AppLogLevel level) {
    AppLogLevel oldLevel = _currentLogLevel.load();
    infof("Setting Log Level \"%s\" -> \"%s\"", _getLogLevelString(oldLevel), _getLogLevelString(level));
    _currentLogLevel.store(level);
}

AppLogLevel Logger::getLogLevel() const {
    return _currentLogLevel.load();
}

void Logger::log(AppLogLevel level, const char* message) {
    if (level < _currentLogLevel.load()) {
        return;
    }

    LogMessage logEntry;
    size_t msgLen = strnlen(message, MAX_MESSAGE_LENGTH);

    logEntry.level = level;
    logEntry.msgLen = static_cast<uint8_t>(msgLen);
    memcpy(logEntry.message, message, msgLen);
    
    size_t totalLen = offsetof(LogMessage, message) + msgLen;

    if (xStreamBufferSpacesAvailable(_logStreamBufferHandle) < totalLen) {
        return;
    }

    xStreamBufferSend(_logStreamBufferHandle, &logEntry, totalLen, 0);
}

void Logger::loop() {
    // 1. Periodically check network status
    if (millis() - _lastPingTime >= PING_CHECK_INTERVAL_MS) {
        _lastPingTime = millis();
        if (WiFi.status() == WL_CONNECTED) {
            bool isReachable = Ping.ping(SYSLOG_SERVER, 1);
            if (isReachable && !_syslogServerReachable) {
                _syslogServerReachable = true;
                info("Syslog server is reachable. Processing queued logs.");
            } else if (!isReachable && _syslogServerReachable) {
                _syslogServerReachable = false;
                warning("Syslog server has become unreachable. Pausing log sending.");
            }
        } else if (_syslogServerReachable) {
            _syslogServerReachable = false;
            warning("WiFi disconnected. Pausing log sending.");
        }
    }

    // 2. Only process logs if the server is reachable.
    if (_syslogServerReachable) {
        int processedCount = 0;

        while (processedCount < MAX_LOGS_PER_LOOP) {
            LogMessage receivedEntry;
            const size_t headerSize = offsetof(LogMessage, message);

            size_t headerBytesRead = xStreamBufferReceive(_logStreamBufferHandle, &receivedEntry, headerSize, 0);
            if (headerBytesRead != headerSize) {
                break;
            }

            size_t msgBytesRead = xStreamBufferReceive(_logStreamBufferHandle, receivedEntry.message, receivedEntry.msgLen, 0);
            if (msgBytesRead != receivedEntry.msgLen) {
                xStreamBufferReset(_logStreamBufferHandle);
                errorf("Log stream corrupted. Buffer has been reset.");
                break;
            }

            receivedEntry.message[msgBytesRead] = '\0';

            #ifdef SERIAL_OUT_DEBUG
            SerialMutex::getInstance().lock();
            Serial.printf("[%s] %s\n", _getLogLevelString(receivedEntry.level), receivedEntry.message);
            SerialMutex::getInstance().unlock();
            #endif

            if (_mqtt != nullptr && _mqtt->isConnected()) {
                char payload[MAX_MESSAGE_LENGTH + 128];
                snprintf(payload, sizeof(payload), "{\"level\": \"%s\", \"message\": \"%s\"}",
                         _getLogLevelString(receivedEntry.level), receivedEntry.message);
                _mqtt->publish(MQTT_LOG_TOPIC, payload);
            }

            if (_syslog != nullptr) {
                _syslog->log(receivedEntry.level, receivedEntry.message);
            }

            processedCount++;
        }
    }
}

void Logger::debugf(const char* format, ...) {
    char buffer[MAX_MESSAGE_LENGTH + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(APP_LOG_DEBUG, buffer);
}

void Logger::infof(const char* format, ...) {
    char buffer[MAX_MESSAGE_LENGTH + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(APP_LOG_INFO, buffer);
}

void Logger::warningf(const char* format, ...) {
    char buffer[MAX_MESSAGE_LENGTH + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(APP_LOG_WARNING, buffer);
}

void Logger::errorf(const char* format, ...) {
    char buffer[MAX_MESSAGE_LENGTH + 1];
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
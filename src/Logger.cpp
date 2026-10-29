#include "Logger.h"
#include <cstdarg>
#include <cstddef>
#include <errno.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#ifdef SERIAL_OUT_DEBUG 
#include "SerialMutex.h"
#endif

Logger logger;
static portMUX_TYPE criticalSectionSpinlock = portMUX_INITIALIZER_UNLOCKED;

Logger::Logger() {
    _logStreamBufferHandle = xMessageBufferCreate(LOG_BUFFER_SIZE_BYTES);
}

// Syslog severities (RFC 5424); facility 0 (kern), as the device always used
static uint8_t syslogSeverity(AppLogLevel level) {
    switch (level) {
        case APP_LOG_DEBUG:   return 7;
        case APP_LOG_INFO:    return 6;
        case APP_LOG_WARNING: return 4;
        case APP_LOG_ERROR:
        default:              return 3;
    }
}

void Logger::_formatLine(AppLogLevel level, const char* message) {
    // RFC 5424 with a nil timestamp (rsyslog stamps reception time), one line per message
    // (newline framing, accepted by rsyslog imtcp). Same header as before, without the BOM.
    int n = snprintf(_line, sizeof(_line) - 1, "<%u>1 - %s %s - - - %s",
                     (unsigned)syslogSeverity(level), DEVICE_HOSTNAME, SYSLOG_APP_NAME, message);
    if (n < 0) {
        n = 0;
    } else if ((size_t)n > sizeof(_line) - 2) {
        n = sizeof(_line) - 2; // Truncated
    }
    for (int i = 0; i < n; i++) {
        if (_line[i] == '\n' || _line[i] == '\r') _line[i] = ' '; // Would split the frame
    }
    _line[n++] = '\n';
    _lineLen = n;
    _lineSent = 0;
}

void Logger::_syslogClose() {
    if (_syslogSocket >= 0) {
        close(_syslogSocket);
        _syslogSocket = -1;
    }
    _syslogConnecting = false;
    _lineSent = 0; // A partially sent line is sent again in full on the next connection
    if (_syslogWasConnected) {
        _syslogWasConnected = false;
        warningf("Syslog connection to %s:%u lost; logs stay queued until it is back.", SYSLOG_SERVER, SYSLOG_PORT);
    }
}

bool Logger::_syslogReady() {
    if (_syslogSocket >= 0 && !_syslogConnecting) {
        // Notice a connection rsyslog closed before writing to it (data would be lost)
        char c;
        int r = recv(_syslogSocket, &c, 1, MSG_PEEK | MSG_DONTWAIT);
        if (r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            _syslogClose();
            return false;
        }
        return true;
    }

    if (_syslogSocket >= 0) {
        // Connect in progress: check its outcome without waiting
        fd_set writeFds;
        FD_ZERO(&writeFds);
        FD_SET(_syslogSocket, &writeFds);
        struct timeval noWait = {0, 0};
        int r = select(_syslogSocket + 1, nullptr, &writeFds, nullptr, &noWait);
        if (r > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(_syslogSocket, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err != 0) {
                _syslogClose();
                return false;
            }
            _syslogConnecting = false;
            _syslogWasConnected = true;
            infof("Syslog connected to %s:%u (TCP); sending queued logs.", SYSLOG_SERVER, SYSLOG_PORT);
            return true;
        }
        if (r < 0 || millis() - _syslogConnectStartedAt >= SYSLOG_CONNECT_TIMEOUT_MS) {
            _syslogClose();
        }
        return false;
    }

    if (_syslogConnectStartedAt != 0 && millis() - _syslogConnectStartedAt < SYSLOG_RECONNECT_INTERVAL_MS) {
        return false;
    }
    _syslogConnectStartedAt = millis();

    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        return false;
    }
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    // Without keepalive, a host that disappears silently leaves the socket "connected" for
    // minutes while writes fill its send buffer; with it, the next peek reports the error
    int keepAlive = 1;
    int keepIdle = SYSLOG_KEEPALIVE_IDLE_S;
    int keepInterval = SYSLOG_KEEPALIVE_INTERVAL_S;
    int keepCount = SYSLOG_KEEPALIVE_COUNT;
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
    setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
    setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
    setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SYSLOG_PORT);
    inet_pton(AF_INET, SYSLOG_SERVER, &addr.sin_addr);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0 && errno != EINPROGRESS) {
        close(s);
        return false;
    }
    _syslogSocket = s;
    _syslogConnecting = true; // Completion checked on the next calls
    return false;
}

bool Logger::_syslogSendLine() {
    ssize_t n = send(_syslogSocket, _line + _lineSent, _lineLen - _lineSent, MSG_DONTWAIT);
    if (n > 0) {
        _lineSent += n;
        if (_lineSent >= _lineLen) {
            _lineLen = 0;
            _lineSent = 0;
            return true;
        }
        return false; // Rest of the line on the next call
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return false; // Send buffer full: retry later
    }
    _syslogClose();
    return false;
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

#ifdef SERIAL_OUT_DEBUG
    SerialMutex::getInstance().lock();
    Serial.printf("[%s] %s\r\n", _getLogLevelString(level), message);
    SerialMutex::getInstance().unlock();
#endif

    // Never block the caller: a full buffer (logs queued during a Wi-Fi outage) drops the
    // message, and loop() reports how many were dropped once it can send again
    if (xMessageBufferSpacesAvailable(_logStreamBufferHandle) < totalLen) {
        _droppedMessages.fetch_add(1);
        return;
    }
    taskENTER_CRITICAL(&criticalSectionSpinlock);
    xMessageBufferSend(_logStreamBufferHandle, &logEntry, totalLen, 0);
    taskEXIT_CRITICAL(&criticalSectionSpinlock);
}

void Logger::loop() {
    // Logs leave the queue only while rsyslog accepts them over TCP: without Wi-Fi or
    // while the server is down they stay queued. Nothing here blocks the main loop.
    if (WiFi.status() != WL_CONNECTED) {
        _syslogClose();
        return;
    }
    if (!_syslogReady()) {
        return;
    }

    uint32_t dropped = _droppedMessages.exchange(0);
    if (dropped > 0) {
        warningf("Logger: %lu messages dropped while the log buffer was full.", (unsigned long)dropped);
    }

    for (int processedCount = 0; processedCount < MAX_LOGS_PER_LOOP; processedCount++) {
        if (_lineLen == 0) {
            LogMessage receivedEntry;
            const size_t headerSize = offsetof(LogMessage, message);

            size_t totalBytesRead = xMessageBufferReceive(_logStreamBufferHandle, &receivedEntry, sizeof(receivedEntry), 0);
            if (totalBytesRead < headerSize) {
                break;
            }

            // Validate message length
            if (receivedEntry.msgLen > MAX_MESSAGE_LENGTH) {
                continue; // Skip corrupted message
            }

            receivedEntry.message[receivedEntry.msgLen] = '\0';

            _formatLine(receivedEntry.level, receivedEntry.message);
        }

        if (!_syslogSendLine()) {
            break; // Socket busy or connection lost: the line stays pending
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

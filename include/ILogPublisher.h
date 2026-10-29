#pragma once

#include <stddef.h>

enum AppLogLevel {
    APP_LOG_DEBUG,
    APP_LOG_INFO,
    APP_LOG_WARNING,
    APP_LOG_ERROR
};

class ILogPublisher {
public:
    static constexpr size_t MAX_LOG_MESSAGE_LENGTH = 255;

    virtual ~ILogPublisher() = default;
    virtual bool isMqttConnected() = 0;
    virtual void queueLogMessage(AppLogLevel level, const char* message) = 0;
};

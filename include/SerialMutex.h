#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Singleton class to protect Serial access with a mutex
class SerialMutex {
public:
    static SerialMutex& getInstance() {
        static SerialMutex instance;
        return instance;
    }

    void init() {
        if (_mutex == nullptr) {
            _mutex = xSemaphoreCreateMutexStatic(&_mutexBuffer);
        }
    }

    bool lock(TickType_t timeout = portMAX_DELAY) {
        return xSemaphoreTake(_mutex, timeout) == pdTRUE;
    }

    void unlock() {
        xSemaphoreGive(_mutex);
    }

private:
    SerialMutex() : _mutex(nullptr) {}
    ~SerialMutex() {
        // No need to delete statically allocated mutex
    }

    SemaphoreHandle_t _mutex;
    StaticSemaphore_t _mutexBuffer;
};
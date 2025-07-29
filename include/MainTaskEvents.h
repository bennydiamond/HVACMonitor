#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>

class MainTaskEventNotifier {
public:
    enum EventBits : uint32_t {
        EVT_SCD30_AUTOCAL_ON   = (1UL << 0),
        EVT_SCD30_AUTOCAL_OFF  = (1UL << 1),
        EVT_SPS30_CLEAN        = (1UL << 2),
        EVT_SGP41_TEST         = (1UL << 3),
        // Add more events as needed
    };

    static MainTaskEventNotifier& getInstance();

    void setMainTaskHandle(TaskHandle_t handle);
    void sendEvent(EventBits event);
    bool receiveOneEvent(EventBits& eventOut);
    // Process one event per call, using a callback for the event bit
    void processOneEvent(const std::function<void(EventBits)>& handler);

private:
    MainTaskEventNotifier() : mainTaskHandle(nullptr), pendingEvents(0) {}
    TaskHandle_t mainTaskHandle;
    uint32_t pendingEvents;
    // Disallow copy/move
    MainTaskEventNotifier(const MainTaskEventNotifier&) = delete;
    MainTaskEventNotifier& operator=(const MainTaskEventNotifier&) = delete;
}; 
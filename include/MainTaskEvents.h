#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>

class MainTaskEventNotifier {
public:
    enum EventBits : uint32_t {
        EVT_SCD30_AUTOCAL      = (1UL << 0), // Target in SystemStatusData.scd30AutoCalTarget
        EVT_SPS30_CLEAN        = (1UL << 2),
        EVT_SGP41_TEST         = (1UL << 3),
        EVT_REBOOT_NANO        = (1UL << 4),
        EVT_REBOOT_ESP32       = (1UL << 5),
        EVT_SPS30_GET_INFO     = (1UL << 6),
        EVT_SCD30_FORCECAL     = (1UL << 7),
        EVT_SCD30_GET_INFO     = (1UL << 8),
        // Add more events as needed
    };

    static MainTaskEventNotifier& getInstance();

    void setMainTaskHandle(TaskHandle_t handle);
    void sendEvent(EventBits event);
    // Sets the requested state, then sends EVT_SCD30_AUTOCAL. A single event with a
    // target, not one event per state: pending events run in bit order, so separate
    // ON/OFF events could be sent to the Nano in the opposite order of the requests.
    void requestScd30AutoCal(bool enabled);
    // Sets the target, then sends EVT_SCD30_FORCECAL. A request still pending (not
    // yet sent to the Nano) is replaced, and logged: the latest request wins.
    void requestScd30ForceCal(uint16_t ppm);
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

void dispatch_main_task_event(MainTaskEventNotifier::EventBits event); 
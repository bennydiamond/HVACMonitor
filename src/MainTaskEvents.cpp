#include "MainTaskEvents.h"
#include "SystemStatusAccessor.h"
#include "Logger.h"

MainTaskEventNotifier& MainTaskEventNotifier::getInstance() {
    static MainTaskEventNotifier instance;
    return instance;
}

void MainTaskEventNotifier::setMainTaskHandle(TaskHandle_t handle) {
    mainTaskHandle = handle;
}

void MainTaskEventNotifier::sendEvent(EventBits event) {
    if (mainTaskHandle) {
        xTaskNotify(mainTaskHandle, event, eSetBits);
    }
}

void MainTaskEventNotifier::requestScd30AutoCal(bool enabled) {
    SystemStatusAccessor()->scd30AutoCalTarget = enabled;
    sendEvent(EVT_SCD30_AUTOCAL);
}

void MainTaskEventNotifier::requestScd30ForceCal(uint16_t ppm) {
    uint16_t pending;
    {
        SystemStatusAccessor status;
        pending = status->scd30ForceCalTarget;
        status->scd30ForceCalTarget = ppm;
    }
    if (pending != 0) {
        logger.warningf("SCD30 Force Calibration to %u ppm replaces a pending request for %u ppm.",
                        ppm, pending);
    }
    sendEvent(EVT_SCD30_FORCECAL);
}

bool MainTaskEventNotifier::receiveOneEvent(EventBits& eventOut) {
    uint32_t ulNotificationValue = 0;
    if (xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotificationValue, 0) == pdTRUE && ulNotificationValue) {
        for (uint32_t bit = 0; bit < 32; ++bit) {
            if (ulNotificationValue & (1UL << bit)) {
                eventOut = static_cast<EventBits>(1UL << bit);
                return true;
            }
        }
    }
    return false;
}

void MainTaskEventNotifier::processOneEvent(const std::function<void(EventBits)>& handler) {
    if (pendingEvents == 0) {
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &pendingEvents, 0) != pdTRUE) {
            pendingEvents = 0;
        }
    }
    if (pendingEvents) {
        for (uint32_t bit = 0; bit < 32; ++bit) {
            if (pendingEvents & (1UL << bit)) {
                handler(static_cast<EventBits>(1UL << bit));
                pendingEvents &= ~(1UL << bit); // clear the processed bit
                break; // only process one per call
            }
        }
    }
} 
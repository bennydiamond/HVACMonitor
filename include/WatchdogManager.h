#pragma once

#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifdef ENABLE_TASK_WATCHDOG
class WatchdogManager {
public:
    static const uint32_t _defaultTimeout = 10; // seconds
    
    static void init(uint32_t timeoutSeconds = 5, bool panic = false);
    static void addCurrentTask();
    static void addTask(TaskHandle_t handle);
    static void removeCurrentTask();
    static void removeTask(TaskHandle_t handle);
    static void reset();
    static void setTimeout(uint32_t timeoutSeconds);
    static void restoreDefaultTimeout();
    static void taskIsMonitored();
    
private:
    static esp_task_wdt_config_t _config;
};
#endif
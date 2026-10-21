#pragma once
#include <FreeRTOSConfig.h>

// FreeRTOS Task Priorities (0 = lowest, configMAX_PRIORITIES-1 = highest)
// In ESP-IDF:
// - lwIP TCP/IP stack runs at priority 18
// - Wi-Fi driver runs at priority 23
// Application tasks should run at low priorities (1-4), NEVER at 24.
#define TASK_PRIORITY_IDLE          0   // FreeRTOS idle task
#define TASK_PRIORITY_SENSOR        3   // Sensor reading task
#define TASK_PRIORITY_UI            4   // UI updates and LVGL (above sensor, equal to main; sleeps every loop)
#define TASK_PRIORITY_MAIN          4   // Main application task (highest application priority)

// Stack sizes in bytes (ESP-IDF FreeRTOS takes stack depth in BYTES, not words)
#define STACK_SIZE_SENSOR           3072
#define STACK_SIZE_UI               6144
#define STACK_SIZE_MAIN             ARDUINO_LOOP_STACK_SIZE


#pragma once
#include <FreeRTOSConfig.h>

// FreeRTOS Task Priorities (0 = lowest, configMAX_PRIORITIES-1 = highest)
// ESP32 default configMAX_PRIORITIES = 25

#define TASK_PRIORITY_IDLE          0   // FreeRTOS idle task
#define TASK_PRIORITY_UI            2   // UI updates and LVGL
#define TASK_PRIORITY_SENSOR        3   // Sensor reading task
#define TASK_PRIORITY_MAIN          configMAX_PRIORITIES - 1   // Main application task

// Stack sizes (in words, not bytes)
#define STACK_SIZE_SENSOR           3072
#define STACK_SIZE_UI               4096
#define STACK_SIZE_MAIN             ARDUINO_LOOP_STACK_SIZE

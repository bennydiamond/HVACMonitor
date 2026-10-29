#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <cstdint>

// Forward declarations
class LGFX;
class CST820;
class IUIManager;
class IUIUpdater;

class UITask {
public:
    UITask();
    void start();
    static UITask& getInstance();

    static void prepare_for_ota();
    static void restore_after_ota();   
    static void SetPauseState(bool paused);

    void queue_ota_popup();

private:
    static const uint32_t UITASK_SLEEP_TIME_MS = 15; // fallback sleep when UI manager absent
    static const uint32_t UITASK_SLEEP_TIME_MS_MAX = 10; // max sleep between LVGL handler calls
    static const uint32_t STACK_CHECK_INTERVAL_MS = 60000;
    static const uint8_t DEFAULT_LCD_BRIGHTNESS = 255;

    ~UITask();
    
    static void taskFunc(void* param);
    void run();

    // Owned UI objects
    LGFX* tft;
    CST820* touch;
    IUIManager* uiManager;
    IUIUpdater* uiUpdater;

    TaskHandle_t taskHandle;

    std::atomic<bool> otaPopupRequested{false};
    bool uiIsInit;

    void show_ota_popup();
};

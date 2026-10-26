#include "WatchdogManager.h"
#include "Logger.h"
#ifdef ENABLE_TASK_WATCHDOG

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// Watch the idle task of every core too (as CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPUx
// intends): it catches a task hogging a core, even one not subscribed itself
esp_task_wdt_config_t WatchdogManager::_config = {
    .timeout_ms = WatchdogManager::_defaultTimeout * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = false };

void WatchdogManager::init(uint32_t timeoutSeconds, bool panic) {
    _config.timeout_ms = timeoutSeconds * 1000;
    _config.trigger_panic = panic;
    esp_err_t err = esp_task_wdt_init(&_config);
    if (err == ESP_ERR_INVALID_STATE) {
        // Already started by ESP-IDF (CONFIG_ESP_TASK_WDT_INIT): apply our configuration
        err = esp_task_wdt_reconfigure(&_config);
    }
    if (err == ESP_OK) {
        logger.infof("Watchdog initialized: timeout=%us, panic=%s", timeoutSeconds, panic ? "true" : "false");
    } else {
        logger.errorf("Watchdog init failed: %s", esp_err_to_name(err));
    }
}
#else
bool WatchdogManager::_panic = false;
uint32_t WatchdogManager::_currentTimeout = WatchdogManager::_defaultTimeout;

void WatchdogManager::init(uint32_t timeoutSeconds, bool panic) {
    _currentTimeout = timeoutSeconds;
    _panic = panic;
    esp_err_t err = esp_task_wdt_init(timeoutSeconds, panic);
    if (err == ESP_OK) {
        logger.infof("Watchdog initialized: timeout=%us, panic=%s", timeoutSeconds, panic ? "true" : "false");
    } else {
        logger.errorf("Watchdog init failed: %s", esp_err_to_name(err));
    }
}
#endif

void WatchdogManager::addCurrentTask() {
    addTask(NULL);
}

void WatchdogManager::addTask(TaskHandle_t handle) {
    const esp_err_t status = esp_task_wdt_status(handle);
    if(status == ESP_ERR_NOT_FOUND)
    {
        esp_err_t err = esp_task_wdt_add(handle);
        if (err == ESP_OK) {
            logger.debugf("Task %s subscribed to watchdog", pcTaskGetName(handle));
        } else {
            logger.errorf("Task %s subscription failed: %s", pcTaskGetName(handle), esp_err_to_name(err));
        }
    }
    else if(status == ESP_OK)
    {
        logger.warningf("Task %s already subscribed to watchdog", pcTaskGetName(handle));
    }
    else
    {
        logger.errorf("Task %s cannot be subscribed to watchdog: %s", pcTaskGetName(handle), esp_err_to_name(status));
    }
}

void WatchdogManager::removeCurrentTask() {
    removeTask(NULL);
}

void WatchdogManager::removeTask(TaskHandle_t handle) {
    esp_err_t status = esp_task_wdt_status(handle);
    if (status == ESP_OK) {
        esp_err_t err = esp_task_wdt_delete(handle);
        if (err == ESP_OK) {
            logger.debugf("Task %s unsubscribed from watchdog", pcTaskGetName(handle));
        } else {
            logger.errorf("Task %s unsubscribe failed: %s", pcTaskGetName(handle), esp_err_to_name(err));
        }
    } else if (status == ESP_ERR_NOT_FOUND) {
        logger.debugf("Task %s not subscribed to watchdog", pcTaskGetName(handle));
    } else {
        logger.errorf("Watchdog not initialized, cannot unsubscribe task %s", pcTaskGetName(handle));
    }
}

void WatchdogManager::reset() {
    esp_err_t status = esp_task_wdt_status(NULL);
    if (status == ESP_OK) {
        esp_err_t err = esp_task_wdt_reset();
        if (err != ESP_OK) {
            logger.errorf("Watchdog reset failed: %s", esp_err_to_name(err));
        }
    } else if (status == ESP_ERR_NOT_FOUND) {
        logger.warningf("Task %s not subscribed, cannot reset watchdog", pcTaskGetName(NULL));
    }
}

void WatchdogManager::setTimeout(uint32_t timeoutSeconds) {
    esp_err_t status = esp_task_wdt_status(NULL);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (_config.timeout_ms == timeoutSeconds * 1000) {
        return;
    }
    _config.timeout_ms = timeoutSeconds * 1000;
    if (status != ESP_ERR_INVALID_STATE) {
        esp_err_t err = esp_task_wdt_reconfigure(&_config);
        if (err == ESP_OK) {
            logger.infof("Watchdog timeout changed to %us", timeoutSeconds);
        } else {
            logger.errorf("Watchdog timeout change failed: %s", esp_err_to_name(err));
        }
    } else {
        logger.errorf("Watchdog not initialized, cannot change timeout");
    }
#else
    _currentTimeout = timeoutSeconds;
    if (status != ESP_ERR_INVALID_STATE) {
        esp_err_t err = esp_task_wdt_init(timeoutSeconds, _panic);
        if (err == ESP_OK) {
            logger.debugf("Watchdog timeout changed to %us", timeoutSeconds);
        } else {
            logger.errorf("Watchdog timeout change failed: %s", esp_err_to_name(err));
        }
    } else {
        logger.errorf("Watchdog not initialized, cannot change timeout");
    }
#endif
}

void WatchdogManager::restoreDefaultTimeout() {
    setTimeout(_defaultTimeout);
}
#endif

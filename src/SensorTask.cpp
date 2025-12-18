#include "SensorTask.h"
#include "TaskPriorities.h"
#include "Logger.h"
#include "SerialMutex.h"
#include "I2CBridge.h"
#ifdef ENABLE_TASK_WATCHDOG
#include "WatchdogManager.h"
#endif
SensorTask::SensorTask() 
    : latestZMOD4510Values(),
#ifdef BMP280_ENABLED
      latestBMP280Values(),
#endif
#ifdef AHT20_ENABLED
      latestAHT20Values(),
#endif
      previousFirstTimeFlag(false),
      firstHealthCheckReceived(false),
      rebootDetected(false),
      taskHandle(nullptr),

      stateMutex(nullptr)
{
}

SensorTask::~SensorTask() {
    if (taskHandle != nullptr) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
    
    if (stateMutex != nullptr) {
        vSemaphoreDelete(stateMutex);
        stateMutex = nullptr;
    }
}

void SensorTask::init() {
    // Initialize the serial mutex if not already done
    SerialMutex::getInstance().init();
    
    // Initialize I2CBridge
    I2CBridge::getInstance().begin();
    
    // Initialize managers (simple initialization that cannot fail)
    ZMOD4510Manager::getInstance().init();
#ifdef BMP280_ENABLED
    BMP280Manager::getInstance().begin();
#endif
#ifdef AHT20_ENABLED
    AHT20Manager::getInstance().init();
#endif
    
    // Create mutex for state variables
    stateMutex = xSemaphoreCreateMutexStatic(&stateMutexBuffer);
    if (stateMutex == nullptr) {
        logger.error("Failed to create SensorTask state mutex");
        return;
    }
    
    // Create the task
    BaseType_t result = xTaskCreatePinnedToCore(
        taskFunction,
        "SensorTask",
        STACK_SIZE_SENSOR,
        this,
        TASK_PRIORITY_SENSOR,
        &taskHandle,
        xPortGetCoreID()      // Core ID (use running core to avoid executing on WIFI core)
    );
    
    if (result != pdPASS) {
        logger.errorf("Failed to create SensorTask. Free: %d, Min: %d, Max: %d bytes", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
        taskHandle = nullptr;
    } else {
        logger.infof("SensorTask created successfully on core %d", xPortGetCoreID());
    }
}


bool SensorTask::getZMOD4510Data(ZMOD4510Values& outValues) {
    bool newValuesAvailable = false;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        outValues = latestZMOD4510Values; // Copy latest values to output
        newValuesAvailable = latestZMOD4510Values.valid; // Check if values are valid
        latestZMOD4510Values.valid = false; // Reset valid flag after copying
        xSemaphoreGive(stateMutex);
    }
    return newValuesAvailable;
}

#ifdef BMP280_ENABLED
bool SensorTask::getBMP280Data(BMP280Values& outValues) {
    bool newValuesAvailable = false;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        outValues = latestBMP280Values; // Copy latest values to output
        newValuesAvailable = latestBMP280Values.valid; // Check if values are valid
        latestBMP280Values.valid = false; // Reset valid flag after copying
        xSemaphoreGive(stateMutex);
    }
    return newValuesAvailable;
}
#endif
#ifdef AHT20_ENABLED
bool SensorTask::getAHT20Data(AHT20Values& outValues) {
    bool newValuesAvailable = false;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        outValues = latestAHT20Values; // Copy latest values to output
        newValuesAvailable = latestAHT20Values.valid; // Check if values are valid
        latestAHT20Values.valid = false; // Reset valid flag after copying
        xSemaphoreGive(stateMutex);
    }
    return newValuesAvailable;
}
#endif
void SensorTask::setEnvironmentalData(float temperature_degc, float humidity_pct) {
    ZMOD4510Manager::getInstance().setEnvironmentalData(temperature_degc, humidity_pct);
}

void SensorTask::notifyConnectionStatus(bool connected, bool firstTimeFlag) {
    if (taskHandle == nullptr) return;
    
    uint32_t notifyBits = connected ? NOTIFY_SENSORSTACK_CONNECTED : NOTIFY_SENSORSTACK_OFFLINE;
    
    // Set reboot flag when first time flag transitions from true to false (reboot detected)
    if (firstTimeFlag && connected) {
        notifyBits |= NOTIFY_SENSORSTACK_REBOOTED;
    }
    
    xTaskNotify(taskHandle, notifyBits, eSetValueWithOverwrite);
}

void SensorTask::taskFunction(void* parameter) {
    SensorTask* task = static_cast<SensorTask*>(parameter);
    task->taskLoop();
}

void SensorTask::taskLoop() {
#ifdef ENABLE_TASK_WATCHDOG
    WatchdogManager::addCurrentTask();
#endif
    logger.info("SensorTask started");

    // Variables for periodic stack size logging
    unsigned long lastStackCheckTime = 0;
    const unsigned long STACK_CHECK_INTERVAL_MS = 60000;
    bool connected = false;
    bool sensorStackRebooted = false;
    
    while (true) {
        // Periodically log the remaining stack size for this task
        if (millis() - lastStackCheckTime > STACK_CHECK_INTERVAL_MS) {
            lastStackCheckTime = millis();
            UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL);
            logger.debugf("SensorTask remaining stack: %u bytes", remaining_stack);
        }

        uint32_t notificationValue = 0;
        
        // Handle sensor initialization
        bool shouldInitSensors = false;

        // Wait for notification with timeout
        if (xTaskNotifyWait(0, UINT32_MAX, &notificationValue, pdMS_TO_TICKS(0)) == pdTRUE) {
            connected = (notificationValue & NOTIFY_SENSORSTACK_CONNECTED) != 0;
            sensorStackRebooted = (notificationValue & NOTIFY_SENSORSTACK_REBOOTED) != 0;
            logger.debugf("SensorTask notification: connected=%d, rebooted=%d", connected, sensorStackRebooted);

            if (!firstHealthCheckReceived && connected) {
                // First connection - initialize sensors
                logger.info("First connection to Nano established, initializing sensors");
                firstHealthCheckReceived = true;
                shouldInitSensors = true;
            } else if (connected && sensorStackRebooted) {
                // Sensor stack rebooted - re-initialize sensors
                logger.warning("Sensor stack reboot detected, re-initializing sensors");
                shouldInitSensors = true;
            }
        }
        
        if (shouldInitSensors) {
            ZMOD4510Manager::getInstance().onNanoReboot();
#ifdef BMP280_ENABLED
            BMP280Manager::getInstance().onNanoReboot();
#endif
#ifdef AHT20_ENABLED
            AHT20Manager::getInstance().onNanoReboot();
#endif
            logger.info("Sensor initialization completed");
        }
        
        // Signal managers about connection status
        ZMOD4510Manager::getInstance().onConnectionStatusChanged(connected);
#ifdef BMP280_ENABLED
        BMP280Manager::getInstance().onConnectionStatusChanged(connected);
#endif
#ifdef AHT20_ENABLED
        AHT20Manager::getInstance().onConnectionStatusChanged(connected);
#endif
        
        // Only process sensors if Nano is connected and alive
        if (connected) {
            // Process BMP280 sensor
#ifdef BMP280_ENABLED
            BMP280Manager::getInstance().process();
            
            // Check for new BMP280 data
            if (BMP280Manager::getInstance().hasNewData()) {
                BMP280Sensor::SensorData bmp280Data = BMP280Manager::getInstance().getData();
                
                if (bmp280Data.valid) {
                    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
                        latestBMP280Values.pressure_pa = bmp280Data.pressure_pa;
                        latestBMP280Values.temperature_degc = bmp280Data.temperature_degc;
                        latestBMP280Values.valid = true;
                        xSemaphoreGive(stateMutex);
                    }
                }
            }
#endif
            
            // Process AHT20 sensor
#ifdef AHT20_ENABLED
            AHT20Manager::getInstance().process();
            
            // Check for new AHT20 data 
            if (AHT20Manager::getInstance().hasNewData()) {
                AHT20Manager::Values aht20Data = AHT20Manager::getInstance().getData();
                
                if (aht20Data.valid) {
                    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
                        latestAHT20Values.temperature_degc = aht20Data.temperature_degc;
                        latestAHT20Values.humidity_pct = aht20Data.humidity_pct;
                        latestAHT20Values.valid = true;
                        xSemaphoreGive(stateMutex);
                    }
                }
            }
#endif
            
            // Process ZMOD4510 sensor
            ZMOD4510Manager::getInstance().process();
            
            // Check for new ZMOD4510 data
            if (ZMOD4510Manager::getInstance().hasNewData()) {
                ZMOD4510Manager::Values zmod4510Data = ZMOD4510Manager::getInstance().getData();
                
                if (zmod4510Data.valid) {
                    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
                        latestZMOD4510Values.o3_conc_ppb = zmod4510Data.o3_conc_ppb;
                        latestZMOD4510Values.no2_conc_ppb = zmod4510Data.no2_conc_ppb;
                        latestZMOD4510Values.fast_aqi = zmod4510Data.fast_aqi;
                        latestZMOD4510Values.epa_aqi = zmod4510Data.epa_aqi;
                        latestZMOD4510Values.valid = true;
                        xSemaphoreGive(stateMutex);
                    }
                } else {
                    logger.warning("ZMOD4510: Invalid measurement results");
                }
            }
        }
#ifdef ENABLE_TASK_WATCHDOG
        WatchdogManager::reset();
#endif
        vTaskDelay(pdMS_TO_TICKS(20));
    }
} 
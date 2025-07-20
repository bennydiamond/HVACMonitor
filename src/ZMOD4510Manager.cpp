#include "ZMOD4510Manager.h"
#include "Logger.h"
#include "SerialMutex.h"

ZMOD4510Manager::ZMOD4510Manager() 
    : state(STATE_WAITING_FOR_NANO),
      latestValues(),
      previousFirstTimeFlag(false),
      taskHandle(nullptr),
      sensorStackConnected(false),
      firstTimeFlag(false),
      stateMutex(nullptr)
{
}

ZMOD4510Manager::~ZMOD4510Manager() {
    if (taskHandle != nullptr) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
    
    if (stateMutex != nullptr) {
        vSemaphoreDelete(stateMutex);
        stateMutex = nullptr;
    }
}

void ZMOD4510Manager::init() {
    // Initialize the serial mutex if not already done
    SerialMutex::getInstance().init();
    
    // Initialize HAL and data structures early
    if (!sensor.initHAL()) {
        logger.error("Failed to initialize ZMOD4510 HAL");
        return;
    }
    
    // Create mutex for state variables
    stateMutex = xSemaphoreCreateMutexStatic(&stateMutexBuffer);
    if (stateMutex == nullptr) {
        logger.error("Failed to create ZMOD4510Manager state mutex");
        return;
    }
    
    // Create the task
    BaseType_t result = xTaskCreatePinnedToCore(
        taskFunction,
        "ZMOD4510Task",
        3072,
        this,
        tskIDLE_PRIORITY + 1,
        &taskHandle,
        xPortGetCoreID()      // Core ID (use running core to avoid executing on WIFI core)
    );
    
    if (result != pdPASS) {
        logger.error("Failed to create ZMOD4510Manager task");
        taskHandle = nullptr;
    } else {
        logger.info("ZMOD4510Manager task created successfully on core 1");
    }
}

bool ZMOD4510Manager::process(bool connected, bool flag, Values& outValues) {
    bool newValuesAvailable = false;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        sensorStackConnected = connected;
        firstTimeFlag = flag;
        outValues = latestValues; // Copy latest values to output
        newValuesAvailable = latestValues.valid; // Check if values are valid
        latestValues.valid = false; // Reset valid flag after copying
        xSemaphoreGive(stateMutex);
    }

    return newValuesAvailable;
}

void ZMOD4510Manager::setEnvironmentalData(float temperature_degc, float humidity_pct) {
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        sensor.setEnvironmentalData(temperature_degc, humidity_pct);
        xSemaphoreGive(stateMutex);
    }
}

void ZMOD4510Manager::taskFunction(void* parameter) {
    ZMOD4510Manager* manager = static_cast<ZMOD4510Manager*>(parameter);
    
    manager->taskLoop();
}

void ZMOD4510Manager::taskLoop() {
    logger.info("ZMOD4510Manager task started");
    
    unsigned long lastInitAttemptTime = 0;
    const unsigned long INIT_RETRY_INTERVAL_MS = 5000;

    // Variables for periodic stack size logging
    unsigned long lastStackCheckTime = 0;
    const unsigned long STACK_CHECK_INTERVAL_MS = 60000;
    
    while (true) {
        // Periodically log the remaining stack size for this task
        if (millis() - lastStackCheckTime > STACK_CHECK_INTERVAL_MS) {
            lastStackCheckTime = millis();
            UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL);
            logger.debugf("ZMOD4510 task remaining stack: %u bytes", remaining_stack);
        }

        bool connected = false;
        bool flag = false;
        
        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            connected = sensorStackConnected;
            flag = firstTimeFlag;
            xSemaphoreGive(stateMutex);
        }
        
        // Detect SensorStack reboot (firstTimeFlag transition from 0 to 1)
        bool nanoRebootDetected = !previousFirstTimeFlag && flag;
        
        // If SensorStack reboot detected, reset to initialization state
        if (nanoRebootDetected && state != STATE_WAITING_FOR_NANO) {
            logger.warning("Nano reboot detected, reinitializing ZMOD4510 sensor");
            state = STATE_INITIALIZING;
        }
        
        switch (state) {
            case STATE_WAITING_FOR_NANO:
                // Wait for SensorStack to connect
                if (connected) {
                    logger.info("Nano sensor hub connected, initializing ZMOD4510 sensor");
                    state = STATE_INITIALIZING;
                    lastInitAttemptTime = millis();
                }
                break;
                
            case STATE_INITIALIZING:{
                // Only attempt initialization if the retry interval has passed
                unsigned long currentTime = millis();
                unsigned long timeSinceLastAttempt = currentTime - lastInitAttemptTime;
                
                if (timeSinceLastAttempt > INIT_RETRY_INTERVAL_MS) {
                    logger.debugf("ZMOD4510: Time since last init attempt: %lu ms (waiting for %lu ms)", 
                                  timeSinceLastAttempt, INIT_RETRY_INTERVAL_MS);
                    lastInitAttemptTime = currentTime;
                    
                    if (sensor.init()) {
                        state = STATE_READY;
                    } else {
                        logger.error("Failed to initialize ZMOD4510 sensor");
                        // If SensorStack is disconnected, go back to waiting state
                        if (!connected) {
                            logger.warning("Nano disconnected during initialization, waiting for reconnection");
                            state = STATE_WAITING_FOR_NANO;
                        }
                        // Otherwise stay in initializing state and retry after interval
                    }
                }
                break;
            }
                
            case STATE_READY:
                // Sensor is initialized and ready for use
                sensor.process();

                if(sensor.hasNewData()) {
                    ZMOD4510Sensor::Results results = sensor.getResults();
                    
                    if (results.valid) {
                        latestValues.o3_conc_ppb = results.o3_conc_ppb;
                        latestValues.no2_conc_ppb = results.no2_conc_ppb;
                        latestValues.fast_aqi = results.fast_aqi;
                        latestValues.epa_aqi = results.epa_aqi;
                        latestValues.valid = true;
                        
                    } else {
                        logger.warning("ZMOD4510: Invalid measurement results");
                    }
                }
                break;
        }
                
        previousFirstTimeFlag = flag;
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
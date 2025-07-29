#include "SensorTask.h"
#include "Logger.h"
#include "SerialMutex.h"
#include "I2CBridge.h"

SensorTask::SensorTask() 
    : latestZMOD4510Values(),
      latestBMP280Values(),
#ifdef AHT20_ENABLED
      latestAHT20Values(),
#endif
      previousFirstTimeFlag(false),
      firstTimeFlag(false),
      firstHealthCheckReceived(false),
      rebootDetected(false),
      taskHandle(nullptr),
      sensorStackConnected(false),
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
    BMP280Manager::getInstance().begin();
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
        3072,
        this,
        tskIDLE_PRIORITY + 2,
        &taskHandle,
        xPortGetCoreID()      // Core ID (use running core to avoid executing on WIFI core)
    );
    
    if (result != pdPASS) {
        logger.error("Failed to create SensorTask");
        taskHandle = nullptr;
    } else {
        logger.info("SensorTask created successfully on core 1");
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

void SensorTask::updateConnectionStatus(bool connected, bool flag) {
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        sensorStackConnected = connected;
        firstTimeFlag = flag;
        
        // Mark that we've received at least one health check
        if (connected) {
            firstHealthCheckReceived = true;
        }
        
        xSemaphoreGive(stateMutex);
    }
}

void SensorTask::taskFunction(void* parameter) {
    SensorTask* task = static_cast<SensorTask*>(parameter);
    task->taskLoop();
}

void SensorTask::taskLoop() {
    logger.info("SensorTask started");

    // Variables for periodic stack size logging
    unsigned long lastStackCheckTime = 0;
    const unsigned long STACK_CHECK_INTERVAL_MS = 60000;
    
    while (true) {
        // Periodically log the remaining stack size for this task
        if (millis() - lastStackCheckTime > STACK_CHECK_INTERVAL_MS) {
            lastStackCheckTime = millis();
            UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL);
            logger.debugf("SensorTask remaining stack: %u bytes", remaining_stack);
        }

        bool connected = false;
        bool flag = false;
        
        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            connected = sensorStackConnected;
            flag = firstTimeFlag;
            xSemaphoreGive(stateMutex);
        }
        
        // Detect SensorStack reboot and handle sensor re-initialization
        // Protocol: 
        // 1. Nano reports FirstTimeFlag=0 when it boots/reboots
        // 2. ESP32 sends ACK, then Nano reports FirstTimeFlag=1 until next reboot
        // 3. Sensors should init when FirstTimeFlag=1 is seen for the first time
        // 4. Future reboots: FirstTimeFlag=0 -> ACK -> FirstTimeFlag=1 -> Re-init sensors
        
        // Detect reboot: transition from true to false (FirstTimeFlag=0)
        if (firstHealthCheckReceived && previousFirstTimeFlag && !flag) {
            rebootDetected = true;
            logger.warning("Nano reboot detected, waiting for ACK response");
        }
        
        // Initialize sensors on first connection (FirstTimeFlag=1 seen for first time)
        bool firstConnection = !firstHealthCheckReceived && flag;
        
        // Re-initialize sensors: when flag=true and we've detected a reboot
        bool nanoRebootDetected = rebootDetected && flag;
        
        // Signal managers about connection status and reboot detection
        if (firstConnection) {
            logger.info("First connection to Nano established, initializing sensors");
            // Don't call onNanoReboot() for first connection, let managers initialize normally
        } else if (nanoRebootDetected) {
            logger.warning("Nano reboot detected, notifying managers");
            ZMOD4510Manager::getInstance().onNanoReboot();
            BMP280Manager::getInstance().onNanoReboot();
#ifdef AHT20_ENABLED
            AHT20Manager::getInstance().onNanoReboot();
#endif
            rebootDetected = false; // Reset after handling
            logger.info("Sensor re-initialization completed after Nano reboot");
        }
        
        // Signal managers about connection status
        ZMOD4510Manager::getInstance().onConnectionStatusChanged(connected);
        BMP280Manager::getInstance().onConnectionStatusChanged(connected);
#ifdef AHT20_ENABLED
        AHT20Manager::getInstance().onConnectionStatusChanged(connected);
#endif
        
        // Only process sensors if Nano is connected and alive
        if (connected) {
            // Process BMP280 sensor
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
        } else {
            // Nano is not connected - skip all sensor processing
            // Managers will handle their own state when they receive the disconnected signal
        }
                
        previousFirstTimeFlag = flag;
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
} 
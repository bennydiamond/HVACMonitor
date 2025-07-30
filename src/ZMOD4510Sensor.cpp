#include "ZMOD4510Sensor.h"
#include "Logger.h"

ZMOD4510Sensor::ZMOD4510Sensor() 
    : state(STATE_IDLE), 
      new_data_available(false),
      temperature_degc(-300), // Default to use on-chip temperature sensor
      humidity_pct(50),       // Default to 50% humidity
      measurement_start_time(0)
{
    memset(&latest_results, 0, sizeof(Results));
    latest_results.valid = false;
}

ZMOD4510Sensor::~ZMOD4510Sensor() {
    // Nothing to clean up
}

bool ZMOD4510Sensor::initHAL() {
    logger.info("ZMOD4510: Initializing  HAL and data structures");
    
    int ret = HAL_Init(&hal);
    if (ret) {
        logger.errorf("ZMOD4510: HAL initialization failed with error %d", ret);
        return false;
    }
    
    dev.i2c_addr = ZMOD4510_I2C_ADDR;
    dev.pid = ZMOD4510_PID;
    dev.init_conf = &zmod_no2_o3_sensor_cfg[INIT];
    dev.meas_conf = &zmod_no2_o3_sensor_cfg[MEASUREMENT];
    dev.prod_data = prod_data;
    
    logger.info("ZMOD4510: HAL and data structures initialized successfully");
    return true;
}

bool ZMOD4510Sensor::init() {
    logger.info("ZMOD4510: Initializing sensor");
    
    int ret = detect_and_configure();
    if (ret) {
        logger.errorf("ZMOD4510: Sensor configuration failed with error %d", ret);
        return false;
    }
    
    ret = init_no2_o3(&algo_handle);
    if (ret) {
        logger.errorf("ZMOD4510: Algorithm initialization failed with error %d", ret);
        return false;
    }
    
    logger.info("ZMOD4510: sensor initialized successfully");
    return true;
}

void ZMOD4510Sensor::startMeasurement() {
    if (state != STATE_IDLE) {
        logger.warning("ZMOD4510: Measurement already in progress");
        return;
    }
    
    int ret = zmod4xxx_start_measurement(&dev);
    if (ret) {
        logger.errorf("ZMOD4510: Starting measurement failed with error %d", ret);
        return;
    }
    
    logger.debug("ZMOD4510: Measurement started");
}

void ZMOD4510Sensor::process() {
    switch (state) {
        case STATE_IDLE:
            startMeasurement();
            state = STATE_MEASURING;
            measurement_start_time = millis();
            break;
            
        case STATE_MEASURING:
            // Check if measurement time has elapsed
            if (millis() - measurement_start_time >= ZMOD4510_NO2_O3_SAMPLE_TIME) {
                logger.debug("ZMOD4510: Measurement complete");
                state = STATE_READING_RESULTS;
            }
            else {
                delay(10); // Allow for context switch
            }
            break;
            
        case STATE_READING_RESULTS:
            read_and_verify();
            delay(1);  // Allow for context switch
            processResults();
            
            new_data_available = true;
            state = STATE_DATA_READY;
            break;
            
        case STATE_DATA_READY:
            // Wait for data to be consumed
            break;
    }
}

bool ZMOD4510Sensor::hasNewData() const {
    return new_data_available;
}

ZMOD4510Sensor::Results ZMOD4510Sensor::getResults() {
    new_data_available = false;
    
    state = STATE_IDLE;
    
    return latest_results;
}

void ZMOD4510Sensor::setEnvironmentalData(float temp_degc, float hum_pct) {
    temperature_degc = temp_degc;
    humidity_pct = hum_pct;
}

int ZMOD4510Sensor::detect_and_configure() {
    uint8_t track_number[ZMOD4XXX_LEN_TRACKING];
    int ret;
    
    ret = zmod4xxx_init(&dev, &hal);
    if (ret) {
        logger.errorf("ZMOD4510: Sensor initialization failed with error %d", ret);
        return ret;
    }
    
    ret = zmod4xxx_read_sensor_info(&dev);
    if (ret) {
        logger.errorf("ZMOD4510: Reading sensor info failed with error %d", ret);
        return ret;
    }
    
    ret = zmod4xxx_read_tracking_number(&dev, track_number);
    if (ret) {
        logger.warningf("ZMOD4510: Reading tracking number failed with error %d", ret);
    } else {
        String tracking;
        tracking = "x0000";
        for (int i = 0; i < sizeof(track_number); i++) {
            char hex[3];
            sprintf(hex, "%02X", track_number[i]);
            tracking += hex;
        }
        logger.infof("ZMOD4510: Sensor tracking number: %s", tracking.c_str());
    }
    
    String trimming = "ZMOD4510: Sensor trimming data:";
    for (int i = 0; i < ZMOD4510_PROD_DATA_LEN; i++) {
        char tmp[8];
        sprintf(tmp, " %d", prod_data[i]);
        trimming += tmp;
    }
    logger.debug(trimming.c_str());
    
    ret = zmod4xxx_prepare_sensor(&dev);
    if (ret) {
        logger.errorf("ZMOD4510: Sensor preparation failed with error %d", ret);
        return ret;
    }
    
    return 0;
}

void ZMOD4510Sensor::read_and_verify() {
    int ret;
    
    ret = zmod4xxx_read_status(&dev, &zmod4xxx_status);
    if (ret) {
        logger.errorf("ZMOD4510: Reading sensor status failed with error %d", ret);
        return;
    }
    
    if (zmod4xxx_status & STATUS_SEQUENCER_RUNNING_MASK) {
        ret = zmod4xxx_check_error_event(&dev);
        switch (ret) {
            case ERROR_POR_EVENT:
                logger.error("ZMOD4510: Unexpected sensor reset during measurement");
                break;
            case ZMOD4XXX_OK:
                logger.error("ZMOD4510: Wrong sensor setup");
                break;
            default:
                logger.errorf("ZMOD4510: Unknown error %d during measurement", ret);
                break;
        }
        return;
    }
    
    ret = zmod4xxx_read_adc_result(&dev, adc_result);
    if (ret) {
        logger.errorf("ZMOD4510: Reading ADC results failed with error %d", ret);
        return;
    }
    
    ret = zmod4xxx_check_error_event(&dev);
    if (ret) {
        logger.errorf("ZMOD4510: Error event detected after reading ADC: %d", ret);
        return;
    }
    
    logger.debug("ZMOD4510: ADC results read successfully");
}

void ZMOD4510Sensor::processResults() {
    algo_input.adc_result = adc_result;
    algo_input.humidity_pct = humidity_pct;
    algo_input.temperature_degc = temperature_degc;
    
    int ret = calc_no2_o3(&algo_handle, &dev, &algo_input, &algo_results);
    
    for (int i = 0; i < 4; i++) {
        latest_results.rmox[i] = algo_results.rmox[i] / 1e3; // Convert to kOhm
    }
    // As per datasheet (Table 7), O3 and NO2 concentrations are in ppb with range of 0-500 ppb
    latest_results.o3_conc_ppb = static_cast<uint16_t>(algo_results.O3_conc_ppb);
    latest_results.no2_conc_ppb = static_cast<uint16_t>(algo_results.NO2_conc_ppb);
    latest_results.fast_aqi = algo_results.FAST_AQI;
    latest_results.epa_aqi = algo_results.EPA_AQI;
    
    switch (ret) {
        case NO2_O3_STABILIZATION:
            logger.debug("ZMOD4510: Sensor in warm-up period, results not valid yet");
            latest_results.valid = false;
            break;
            
        case NO2_O3_OK:
            latest_results.valid = true;
            logger.debugf("ZMOD4510: O3=%u ppb, NO2=%u ppb, FAST_AQI=%u, EPA_AQI=%u", 
                         latest_results.o3_conc_ppb, latest_results.no2_conc_ppb,
                         latest_results.fast_aqi, latest_results.epa_aqi);
            break;
            
        case NO2_O3_DAMAGE:
            logger.warning("ZMOD4510: Sensor probably damaged. Algorithm results may be incorrect");
            latest_results.valid = false;
            break;
            
        default:
            logger.errorf("ZMOD4510: Algorithm calculation failed with error %d", ret);
            latest_results.valid = false;
            break;
    }
    
}
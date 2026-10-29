#pragma once

#include "BaseAccessor.h"
#include "SensorDefines.h"
#include "FanStatus.h"
#include <cstdint>
#include <cmath>

struct SensorStateData {
    // Environmental & Electrical Sensor Telemetry
    float pressure = NAN;
    float filter_pressure = 0.0f;
    int cpm = -1;
    float geiger_usvh = NAN;
    float temp = NAN;
    float humi = NAN;
    float co2 = NAN;
    int32_t voc_index = 0;
    int32_t nox_index = 0;
    float fan_amps = NAN;
    float compressor_amps = NAN;
    float geothermal_pump_amps = NAN;
    bool liquid_level_sensor_state = false;
    float co_ppm = NAN;
    float pm1 = NAN;
    float pm25 = NAN;
    float pm4 = NAN;
    float pm10 = NAN;
    bool high_pressure = false;
    FanStatus fan_status = FanStatus::Off;

    // ZMOD4510 Gas & AQI
    float o3_conc_ug_per_m3 = -1.0f;
    float no2_conc_ug_per_m3 = -1.0f;
    uint16_t fast_aqi = 0xFFFF;
    uint16_t epa_aqi = 0xFFFF;

#ifdef BMP280_ENABLED
    float bmp_pressure_pa = NAN;
    float bmp_temp_c = NAN;
#endif

#ifdef AHT20_ENABLED
    float aht_temp_c = NAN;
    float aht_humi_pct = NAN;
#endif
};

/**
 * @brief Thread-safe accessor inheriting from BaseAccessor, providing centralized
 *        mutex-protected read/write access to sensor telemetry state across all tasks.
 */
class SensorDataAccessor : public BaseAccessor {
public:
    SensorDataAccessor();
    ~SensorDataAccessor() override = default;

    SensorStateData* operator->() { return &_state; }
    SensorStateData& operator*() { return _state; }
    const SensorStateData* operator->() const { return &_state; }
    const SensorStateData& operator*() const { return _state; }

    // Static initialization of the underlying mutex
    static void init();

private:
    static SemaphoreHandle_t _sharedMutex;
    static StaticSemaphore_t _mutexBuffer;
    static SensorStateData _state;
};


#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include "NanoSensorPacket.h"
#include "LiveDashboardData.h"
#include "RollingAverage.h"
#include "VOCGasIndexAlgorithm.h"
#include "NOxGasIndexAlgorithm.h"
#include "GeigerCounter.h"
#include "ui/ui_common.h"

// Forward declarations
class UITask;
class HomeAssistantManager;
class FilterPressureTracker;
class SensorTask;

class SensorDataProcessor {
public:
    SensorDataProcessor();
    ~SensorDataProcessor() = default;

    void begin(UITask* ui,
               HomeAssistantManager* ha,
               FilterPressureTracker* filterTracker,
               SensorTask* sensorTask);

    // Parses raw payload string and processes sensor data
    void processPayload(char* payload);

    // Processes parsed sensor telemetry
    void processPacket(const NanoSensorPacket& pkt);

    // Populates sensor telemetry into LiveDashboardData
    void populateDashboardData(LiveDashboardData& data) const;

    // Accessors for processed values
    float getCo2Ppm() const { return _co2_avg.getAverage(); }
    float getVocIndex() const { return _voc_avg.getAverage(); }
    float getNoxIndex() const { return _nox_avg.getAverage(); }
    float getPm1() const { return _pm1_avg.getAverage(); }
    float getPm25() const { return _pm25_avg.getAverage(); }
    float getPm4() const { return _pm4_avg.getAverage(); }
    float getPm10() const { return _pm10_avg.getAverage(); }
    float getCoPpm() const { return _co_avg.getAverage(); }
    float getDiffPressurePa() const { return _diff_pressure_avg.getAverage(); }
    float getCompressorAmps() const { return _compressor_amps_avg.getAverage(); }
    float getPumpAmps() const { return _geothermal_pump_amps_avg.getAverage(); }

    float getTemperatureC() const { return _live_temp_c; }
    float getHumidityPct() const { return _live_humi_pct; }
    float getFanAmps() const { return _live_fan_amps; }
    FanStatus getFanStatus() const { return _live_fan_status; }
    bool isPressureHigh() const { return _live_is_pressure_high; }
    bool isWaterDetected() const { return _live_water_detected; }

private:
    UITask* _ui;
    HomeAssistantManager* _ha;
    FilterPressureTracker* _filterTracker;
    SensorTask* _sensorTask;

    unsigned long _last_received_timestamp;

    // Algorithms
    VOCGasIndexAlgorithm _voc_algorithm;
    NOxGasIndexAlgorithm _nox_algorithm;
    GeigerCounter _geigerCounter;

    // Rolling Averages
    RollingAverage<uint16_t> _co2_avg;
    RollingAverage<uint16_t> _co_avg;
    RollingAverage<uint16_t> _voc_avg;
    RollingAverage<uint16_t> _nox_avg;
    RollingAverage<float> _diff_pressure_avg;
    RollingAverage<float> _pm1_avg;
    RollingAverage<float> _pm25_avg;
    RollingAverage<float> _pm4_avg;
    RollingAverage<float> _pm10_avg;
    RollingAverage<float> _compressor_amps_avg;
    RollingAverage<float> _geothermal_pump_amps_avg;

    // Latest live cached states
    float _live_temp_c;
    float _live_humi_pct;
    float _live_fan_amps;
    FanStatus _live_fan_status;
    bool _live_is_pressure_high;
    bool _live_water_detected;
};

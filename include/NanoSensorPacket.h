#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "SensorDefines.h"

class NanoSensorPacket {
public:
    NanoSensorPacket();
    explicit NanoSensorPacket(char* payload);

    bool parse(char* payload);
    bool isValid() const { return _valid; }

    bool isSgp41Conditioning() const { return _sgp41_conditioning; }

    unsigned long getTimestamp() const { return _timestamp; }
    uint16_t getPressureAdcRaw() const { return _pressure_adc_raw; }
    uint16_t getPulseCount() const { return _pulse_count; }
    float getTemperatureC() const { return _temperature_c; }
    float getHumidityPct() const { return _humidity_pct; }
    float getCo2Ppm() const { return _co2_ppm; }
    uint16_t getVocRaw() const { return _voc_raw; }
    uint16_t getNoxRaw() const { return _nox_raw; }
    float getFanAmps() const { return _fan_amps; }
    float getPm1() const { return _pm1; }
    float getPm25() const { return _pm25; }
    float getPm4() const { return _pm4; }
    float getPm10() const { return _pm10; }
    float getCompressorAmps() const { return _compressor_amps; }
    float getPumpAmps() const { return _pump_amps; }
    bool isWaterDetected() const { return _water_detected; }
    uint16_t getCoAdcRaw() const { return _co_adc_raw; }

private:
    bool _valid;
    unsigned long _timestamp;
    uint16_t _pressure_adc_raw;
    uint16_t _pulse_count;
    float _temperature_c;
    float _humidity_pct;
    float _co2_ppm;
    uint16_t _voc_raw;
    uint16_t _nox_raw;
    float _fan_amps;
    float _pm1;
    float _pm25;
    float _pm4;
    float _pm10;
    float _compressor_amps;
    float _pump_amps;
    bool _water_detected;
    uint16_t _co_adc_raw;
    bool _sgp41_conditioning;
};

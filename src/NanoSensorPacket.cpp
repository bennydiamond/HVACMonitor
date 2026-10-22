#include "NanoSensorPacket.h"
#include <string.h>
#include <stdlib.h>

NanoSensorPacket::NanoSensorPacket()
    : _valid(false),
      _timestamp(0),
      _pressure_adc_raw(0),
      _pulse_count(0),
      _temperature_c(0.0f),
      _humidity_pct(0.0f),
      _co2_ppm(0.0f),
      _voc_raw(0),
      _nox_raw(0),
      _fan_amps(0.0f),
      _pm1(0.0f),
      _pm25(0.0f),
      _pm4(0.0f),
      _pm10(0.0f),
      _compressor_amps(0.0f),
      _pump_amps(0.0f),
      _water_detected(false),
      _co_adc_raw(0) {}

NanoSensorPacket::NanoSensorPacket(char* payload)
    : NanoSensorPacket() {
    _valid = parse(payload);
}

bool NanoSensorPacket::parse(char* payload) {
    if (!payload) return false;

    char *token = strtok(payload, ",");
    if (!token) return false;
    _timestamp = strtoul(token, nullptr, 10);

    token = strtok(NULL, ",");
    if (!token) return false;
    _pressure_adc_raw = atoi(token);

    token = strtok(NULL, ",");
    if (!token) return false;
    _pulse_count = atoi(token);

    token = strtok(NULL, ",");
    if (!token) return false;
    _temperature_c = atof(token) / 10.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _humidity_pct = atof(token) / 10.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _co2_ppm = atof(token);

    token = strtok(NULL, ",");
    if (!token) return false;
    _voc_raw = static_cast<uint16_t>(strtoul(token, nullptr, 10));

    token = strtok(NULL, ",");
    if (!token) return false;
    _nox_raw = static_cast<uint16_t>(strtoul(token, nullptr, 10));

    token = strtok(NULL, ",");
    if (!token) return false;
    _fan_amps = atof(token) / 100.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _pm1 = atof(token) / 10.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _pm25 = atof(token) / 10.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _pm4 = atof(token) / 10.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _pm10 = atof(token) / 10.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _compressor_amps = atof(token) / 100.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _pump_amps = atof(token) / 100.0f;

    token = strtok(NULL, ",");
    if (!token) return false;
    _water_detected = (atoi(token) == 0); // Active low GPIO on SensorStack

    token = strtok(NULL, ",");
    if (!token) return false;
    _co_adc_raw = atoi(token);

    return true;
}

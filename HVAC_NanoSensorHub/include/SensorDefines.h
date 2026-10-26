#pragma once

#include <stdint.h>
#include <math.h>

// ==============================================================================
// Sensor Startup & Timing Constants
// ==============================================================================
// SCD30 typical warmup / first measurement period after power-on is ~2-5s.
// SGP41 holds off publishing readings during this window unless SCD30 responds earlier.
constexpr unsigned long SCD30_WARMUP_TIMEOUT_MS = 10000;

// ==============================================================================
// SGP41 VOC & NOx Sensor Validity Ranges
// ==============================================================================
// SGP41 MOx sensor raw ticks are strictly > 0 under normal operation (typically 15000 - 45000).
// A value of 0 indicates the sensor is uninitialized, conditioning, in SCD30 warmup hold-off, or has an I2C fault.
// Sensirion's Gas Index Algorithm considers any raw reading outside [1, 64999] invalid (e.g. electrical short or open).
constexpr uint16_t SGP41_RAW_MIN_VALID = 1;
constexpr uint16_t SGP41_RAW_MAX_VALID = 64999;

// Sensirion Gas Index output range:
// An index of 0 represents the initial blackout period (~45 seconds) while the estimator initializes,
// or an uninitialized/invalid raw input. Valid Gas Index outputs are strictly within 1..500
// (typical clean indoor air baseline: VOC Index = 100, NOx Index = 1).
constexpr int32_t GAS_INDEX_MIN_VALID = 1;
constexpr int32_t GAS_INDEX_MAX_VALID = 500;

// Sensirion SGP41 uncompensated reference defaults per datasheet:
// 0x8000 (50.0% RH) and 0x6666 (25.0 degC) serve as hardware bypass values that leave
// internal MOx humidity/temperature compensation in neutral mode when SCD30 is warming up.
constexpr uint16_t SGP41_DEFAULT_RH_TICKS   = 0x8000;
constexpr uint16_t SGP41_DEFAULT_TEMP_TICKS = 0x6666;

// ==============================================================================
// SCD30 CO2, Temperature & Humidity Validity Ranges
// ==============================================================================
// Valid CO2 measurement range per SCD30 datasheet is 0 - 40000 ppm.
// Values <= 0 or NaN indicate sensor warmup, communication failure, or invalid data.
constexpr float SCD30_CO2_MIN_VALID_PPM = 1.0f;
constexpr float SCD30_CO2_MAX_VALID_PPM = 40000.0f;

// Valid temperature range for SCD30: -40.0°C to +70.0°C
constexpr float SCD30_TEMP_MIN_VALID_C = -40.0f;
constexpr float SCD30_TEMP_MAX_VALID_C = 70.0f;

// Valid relative humidity range for SCD30: 0.0% to 100.0%
constexpr float SCD30_HUMI_MIN_VALID_PCT = 0.0f;
constexpr float SCD30_HUMI_MAX_VALID_PCT = 100.0f;

// ==============================================================================
// SensorStack Sensor Faults
// ==============================================================================
// Bits of the fault mask the SensorStack reports in its health response. A sensor that
// failed to initialize, or stopped answering while running, is skipped by the SensorStack
// (and retried periodically), and its values must be ignored.
constexpr uint8_t SENSOR_FAULT_SPS30 = 1 << 0;
constexpr uint8_t SENSOR_FAULT_SCD30 = 1 << 1;
constexpr uint8_t SENSOR_FAULT_SGP41 = 1 << 2;

// ==============================================================================
// Helper Validity Functions
// ==============================================================================
inline bool isValidVocRaw(uint16_t raw) {
    return raw >= SGP41_RAW_MIN_VALID && raw <= SGP41_RAW_MAX_VALID;
}

inline bool isValidNoxRaw(uint16_t raw) {
    return raw >= SGP41_RAW_MIN_VALID && raw <= SGP41_RAW_MAX_VALID;
}

inline bool isValidGasIndex(int32_t index) {
    return index >= GAS_INDEX_MIN_VALID && index <= GAS_INDEX_MAX_VALID;
}

inline bool isValidCO2(float co2) {
    return !isnan(co2) && !isinf(co2) && co2 >= SCD30_CO2_MIN_VALID_PPM && co2 <= SCD30_CO2_MAX_VALID_PPM;
}

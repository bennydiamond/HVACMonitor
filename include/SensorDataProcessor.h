#pragma once

#include "GeigerCounter.h"
#include "LiveDashboardData.h"
#include "NOxGasIndexAlgorithm.h"
#include "NanoSensorPacket.h"
#include "RollingAverage.h"
#include "VOCGasIndexAlgorithm.h"
#include "ui/ui_common.h"
#include <Arduino.h>
#include <stdbool.h>
#include <stdint.h>

// Forward declarations
class UITask;
class HomeAssistantManager;
class FilterPressureTracker;
class SensorTask;

class SensorDataProcessor {
public:
  static constexpr float SENSOR_SAMPLING_INTERVAL_S = 2.0f;
  static constexpr uint32_t SENSOR_QUERY_INTERVAL_MS =
      static_cast<uint32_t>(SENSOR_SAMPLING_INTERVAL_S * 1000.0f);

  SensorDataProcessor();
  ~SensorDataProcessor() = default;

  void begin(UITask *ui, HomeAssistantManager *ha,
             FilterPressureTracker *filterTracker, SensorTask *sensorTask);

  // Parses raw payload string and processes sensor data
  void processPayload(char *payload);

  // Processes parsed sensor telemetry
  void processPacket(const NanoSensorPacket &pkt);

  // SENSOR_FAULT_* bits reported by the SensorStack: values of those sensors
  // are ignored
  void setSensorFaultMask(uint8_t mask);

  // Populates sensor telemetry into LiveDashboardData
  void populateDashboardData(LiveDashboardData &data) const;

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
  UITask *_ui;
  HomeAssistantManager *_ha;
  FilterPressureTracker *_filterTracker;
  SensorTask *_sensorTask;

  unsigned long _last_received_timestamp;
  uint8_t _sensor_fault_mask;

  // Algorithms
  // Sensirion Gas Index algorithms assume a 1.0s sample interval by default.
  // Because Nano sensor packets arrive every 2.0s (SENSOR_QUERY_INTERVAL_MS),
  // we must supply 2.0f so the adaptive baseline estimators and low-pass
  // filters use the correct time constants rather than suffering a 2x
  // time-dilation bias.
  //
  // Why we cannot use the standard constructor for NOx:
  // NOxGasIndexAlgorithm inherits from SensirionGasIndexAlgorithm, but
  // Sensirion's wrapper header only provides a parameterless constructor that
  // hardcodes the 1.0s default. The base class SensirionGasIndexAlgorithm
  // explicitly provides:
  //   SensirionGasIndexAlgorithm(int32_t algorithm_type, float
  //   sampling_interval);
  // We therefore instantiate SensirionGasIndexAlgorithm directly with
  // ALGORITHM_TYPE_NOX and SENSOR_SAMPLING_INTERVAL_S to eliminate the 1s
  // default bias.
  VOCGasIndexAlgorithm _voc_algorithm{SENSOR_SAMPLING_INTERVAL_S};
  SensirionGasIndexAlgorithm _nox_algorithm{
      SensirionGasIndexAlgorithm::ALGORITHM_TYPE_NOX,
      SENSOR_SAMPLING_INTERVAL_S};
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

#pragma once

#include <Arduino.h>

struct LiveDashboardData {
  // Indoor Air Quality & Climate
  float temperature_c = -999.0f;
  float humidity_pct = -1.0f;
  float co2_ppm = -1.0f;
  int32_t voc_index = 0;
  int32_t nox_index = 0;
  float pm1_ug = -1.0f;
  float pm25_ug = -1.0f;
  float pm4_ug = -1.0f;
  float pm10_ug = -1.0f;
  int co_ppm = -1;
  float o3_ug = -1.0f;
  float no2_ug = -1.0f;
  int fast_aqi = -1;
  int epa_aqi = -1;
  int geiger_cpm = -1;
  float geiger_usv_h = -1.0f;
  float baro_pressure_hpa = -1.0f;

  // HVAC Equipment Operations
  float diff_pressure_pa = 0.0f;
  float filter_pressure_pa = 0.0f;
  bool is_pressure_high = false;
  float fan_amps = 0.0f;
  uint8_t fan_status = 0; // 0=Off, 1=Alert, 2=Normal
  float compressor_amps = 0.0f;
  float pump_amps = 0.0f;
  bool water_detected = false;

  // System & Network (Integrator)
  bool wifi_connected = false;
  int8_t wifi_rssi = 0;
  char wifi_ssid[33] = {0};
  char wifi_ip[16] = {0};
  char wifi_mac[18] = {0};
  bool mqtt_connected = false;
  char mqtt_host[64] = {0};
  int mqtt_port = 1883;

  // Sensor Hub (Nano Co-processor)
  bool sensor_module_connected = false;
  uint8_t sensorstack_firmware_state = 0; // SensorStackFirmwareState: 0 OK, 1 updating, 2 faulty
  uint32_t seconds_since_packet = 0;
  uint32_t nano_uptime_seconds = 0;
  uint16_t nano_free_ram = 0;
  char nano_version[12] = {0};
  char nano_reset_cause[16] = {0};

  // Diagnostics & ESP32 Health (Developer)
  uint32_t esp32_free_heap = 0;
  uint32_t esp32_min_free_heap = 0;
  uint32_t esp32_max_alloc_heap = 0;
  uint32_t esp32_uptime_seconds = 0;
  char esp32_reset_reason[32] = {0};
  char firmware_version[16] = {0};
};

void get_live_dashboard_data(LiveDashboardData &data);

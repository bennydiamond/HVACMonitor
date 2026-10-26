#pragma once

// --- Protocol Definitions ---
#define CMD_GET_SENSORS        'S' // Request sensor data (changed from CMD_BROADCAST_SENSORS)
#define RSP_SENSORS            's' // Response with sensor data
#define CMD_GET_VERSION       'V'
#define CMD_GET_HEALTH        'H'
#define RSP_HEALTH            'h' // Overall health status response
#define CMD_ACK_HEALTH        'A' // Acknowledge Health Status
#define RSP_VERSION           'v'
#define CMD_REBOOT            'R' // Request Nano Reboot
#define CMD_BOOTLOADER        'B' // Request Nano Enter Bootloader
#define RSP_BOOTLOADER        'b' // Response/ACK for Nano Enter Bootloader
#define CMD_GET_SPS30_INFO    'P' // Request SPS30 info
#define RSP_SPS30_INFO        'p' // Response with SPS30 info
#define CMD_SPS30_CLEAN       'C' // Request manual SPS30 fan cleaning
#define RSP_SPS30_CLEAN       'c' // ACK for fan cleaning command
#define CMD_SGP41_TEST        'G' // Request SGP41 test
#define RSP_SGP41_TEST        'g' // Response with SGP41 test result
#define CMD_GET_SCD30_INFO    'D' // Request SCD30 info
#define RSP_SCD30_INFO        'd' // Response with SCD30 info
#define CMD_SET_SCD30_AUTOCAL 'T' // Set SCD30 AutoCalibration
#define RSP_SCD30_AUTOCAL     't' // Response with SCD30 AutoCalibration result
#define CMD_SET_SCD30_FORCECAL 'F' // Set SCD30 Forced Recalibration
#define RSP_SCD30_FORCECAL     'f' // Response with SCD30 Forced Recalibration result

// --- I2C Bridge Commands ---
#define CMD_I2C_READ          'I' // Request I2C read operation
#define RSP_I2C_READ          'i' // Response with I2C read result
#define CMD_I2C_WRITE         'W' // Request I2C write operation
#define RSP_I2C_WRITE         'w' // Response with I2C write result

// I2C Error Codes
#define I2C_ERROR_NONE        0x00 // No error
#define I2C_ERROR_ADDR_NACK   0x01 // Address not acknowledged
#define I2C_ERROR_DATA_NACK   0x02 // Data not acknowledged
#define I2C_ERROR_OTHER       0x03 // Other error
#define I2C_ERROR_TIMEOUT     0x04 // Timeout
#define I2C_ERROR_BUF_LEN     0x05 // Buffer length exceeded
#define I2C_ERROR_PENDING     0x06 // Operation pending

// Functions to send commands to the Nano, defined in main.cpp
void send_command_to_nano(char cmd);
void send_command_to_nano_with_param(char cmd, char param);
void send_command_to_nano_str(const char *data_str);

struct SPS30DeviceInfo {
  bool valid = false;
  uint8_t fw_major = 0;
  uint8_t fw_minor = 0;
  uint32_t fan_interval = 0;
  uint32_t fan_days = 0;
  uint32_t status_reg = 0;
};

struct SGP41TestResult {
  bool valid = false;
  int ret_status = -1;
  uint16_t raw_value = 0;
};

const SPS30DeviceInfo& get_sps30_device_info();
const SGP41TestResult& get_sgp41_test_result();
bool query_sps30_info_blocking(uint32_t timeout_ms = 3000);
bool run_sgp41_test_blocking(uint32_t timeout_ms = 4000);
/*
 * ======================================================================
 * PLATFORMIO PROJECT FOR ARDUINO NANO (ATmega168 or ATmega328)
 * Reads Pressure, Geiger, SPS30, SCD30 and SGP41 sensors.
 * ======================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <sps30.h>
#include <SensirionI2cScd30.h>
#include <SensirionI2CSgp41.h>

// ======================================================================
//  CONFIGURATION & DEFINITIONS
// ======================================================================

// --- Firmware & Protocol ---
#define NANO_FIRMWARE_VERSION "1.2.0"
#define CMD_GET_SENSORS        'S' // Request sensor data (changed from CMD_BROADCAST_SENSORS)
#define RSP_SENSORS            's' // Response with sensor data
#define CMD_GET_VERSION       'V'
#define CMD_GET_HEALTH        'H'
#define RSP_HEALTH            'h' // Overall health status response
#define CMD_ACK_HEALTH        'A' // Acknowledge Health Status
#define RSP_VERSION           'v'
#define CMD_REBOOT            'R' // Request Nano Reboot
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

// --- Pin Definitions ---
#define PRESSURE_SENSOR_PIN A0
#define GEIGER_PIN          2
#define FAN_CT_CLAMP_PIN    A1
#define DEBUG_LED_PIN       13
#define LIQUID_LEVEL_SENSOR_PIN 8
#define COMPRESSOR_CT_CLAMP_PIN A3
#define GEOTHERMAL_PUMP_CT_CLAMP_PIN A2

// --- Timing Constants ---
const unsigned long COMMAND_TIMEOUT_MS = 250;
const unsigned long SCD30_INVALIDATE_TIMEOUT_MS = 60000;

// --- Sensor Calculation Constants ---
#define SHUNT_RESISTOR 150.0f
#define NUM_SAMPLES   1000
#define FAN_CT_VOLTS_PER_AMP        (1.0f / 10.0f)   // 0.1 V/A, e.g. 1V at 10A
#define COMPRESSOR_CT_VOLTS_PER_AMP (1.0f / 30.0f)   // 0.0333 V/A, SCT-013-030: 1V at 30A
#define GEOTHERMAL_PUMP_CT_VOLTS_PER_AMP (1.0f / 5.0f)   // 0.2 V/A, 5A CT clamp: 1V at 5A

// --- Buffer Sizes ---
#define MAX_COMMAND_LEN 150
#define MAX_RESPONSE_LEN 200

// --- Hardware & Behavior Constants ---
#define SERIAL_BAUD_RATE            19200
#define I2C_PAYLOAD_BUFFER_SIZE     40    // Max bytes for an I2C data payload
#define I2C_WIRE_LIB_MAX_READ       32    // Max bytes the AVR Wire library can read at once
#define I2C_CMD_MAX_WRITE_IN_READ   16    // Max write bytes within a read-write command
#define I2C_WRITE_READ_DELAY_MS     2     // Brief delay between an I2C write and read
#define STARTUP_BLINK_DELAY_MS      500   // Delay for the startup LED blink

// ======================================================================
//  GLOBAL VARIABLES & OBJECTS
// ======================================================================

bool first_health_status_sent = true;
volatile uint16_t geiger_pulse_count = 0;
float    current_pressure_pa = 0.0;
float    fan_amps            = 0.0;
float    compressor_amps     = 0.0;
float    geothermal_pump_amps = 0.0;
float    current_co2         = 0.0;
float    current_temp_c      = 0.0;
float    current_humi        = 0.0;
uint16_t current_voc_raw     = 0;
uint16_t current_nox_raw     = 0;
struct   sps30_measurement current_sps_data = {0};
unsigned long last_scd30_update = 0;
SensirionI2cScd30 scd30_sensor;
SensirionI2CSgp41 sgp41_sensor;
uint8_t conditioning_s = 10;
char tx_command_buffer[MAX_RESPONSE_LEN];

// --- Forward Declarations ---
unsigned long read_all_sensors();
void send_data_packet(unsigned long timestamp);
void process_command(const char* buffer);
int freeRam();
void recoverI2Cbus();
bool checkAndRecoverI2C();

// ======================================================================

void on_geiger_pulse() {
  geiger_pulse_count++;
}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

uint8_t calculate_checksum(const char* data_str) {
  uint8_t crc = 0x00;
  uint8_t polynomial = 0x07;
  size_t length = strlen(data_str);
  for (size_t i = 0; i < length; i++) {
    crc ^= data_str[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ polynomial;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

float readCurrentRMS_Generic(uint8_t analog_pin, float volts_per_amp) {
  long sum_of_squares = 0;
  long sum_of_samples = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
      int reading = analogRead(analog_pin);
      sum_of_samples += reading;
      sum_of_squares += (long)reading * reading;
  }
  float mean_of_samples = (float)sum_of_samples / NUM_SAMPLES;
  float mean_of_squares = (float)sum_of_squares / NUM_SAMPLES;
  float variance = mean_of_squares - (mean_of_samples * mean_of_samples);
  float rms_adc = sqrt(variance);
  float voltage = rms_adc * (5.0 / 1023.0);
  float current = voltage / volts_per_amp;
  return current;
}

void blink_error_code(uint8_t blinks) {
  for (uint8_t i = 0; i < blinks; ++i) {
    digitalWrite(DEBUG_LED_PIN, HIGH);
    delay(250);
    digitalWrite(DEBUG_LED_PIN, LOW);
    delay(250);
  }
}

// ======================================================================
//  SETUP
// ======================================================================

volatile uint8_t urboot_reset_flags __attribute__((section(".noinit")));
uint8_t last_reset_cause = 0;

void capture_r2(void) __attribute__((naked, section(".init0"), used));
void capture_r2(void) {
    asm volatile ("sts urboot_reset_flags, r2\n");
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);

    if (urboot_reset_flags & (1 << 3))      last_reset_cause = 4; // Watchdog
    else if (urboot_reset_flags & (1 << 2)) last_reset_cause = 3; // Brown-Out
    else if (urboot_reset_flags & (1 << 1)) last_reset_cause = 2; // External
    else if (urboot_reset_flags & (1 << 0)) last_reset_cause = 1; // Power-On
    else                                    last_reset_cause = 0; // Unknown

    pinMode(DEBUG_LED_PIN, OUTPUT);
    digitalWrite(DEBUG_LED_PIN, HIGH);
/*
    delay(STARTUP_BLINK_DELAY_MS);
    digitalWrite(DEBUG_LED_PIN, LOW);
    delay(STARTUP_BLINK_DELAY_MS);
*/

    // Check and recover I2C bus before initializing sensors
    checkAndRecoverI2C();

    sensirion_i2c_init();

    while (sps30_probe() != 0) {
      blink_error_code(3);
      delay(1500);
      // Check I2C bus before retrying
      checkAndRecoverI2C();
    }

    if (sps30_start_measurement() < 0) {
      while (1) { blink_error_code(4); delay(1500); }
    }

    scd30_sensor.begin(Wire, SCD30_I2C_ADDR_61);
    if (scd30_sensor.startPeriodicMeasurement(0)) {
      while (1) { blink_error_code(5); delay(1500); }
    }

    sgp41_sensor.begin(Wire);

    pinMode(LIQUID_LEVEL_SENSOR_PIN, INPUT_PULLUP);

    pinMode(GEIGER_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), on_geiger_pulse, RISING);

    pinMode(PRESSURE_SENSOR_PIN, INPUT);
    pinMode(FAN_CT_CLAMP_PIN, INPUT); 
    pinMode(COMPRESSOR_CT_CLAMP_PIN, INPUT);
    pinMode(GEOTHERMAL_PUMP_CT_CLAMP_PIN, INPUT);
}

// ======================================================================
//  MAIN LOOP
// ======================================================================
void loop() {
  wdt_reset();

  static char command_buffer[MAX_COMMAND_LEN];
  static uint8_t command_len = 0;
  static bool in_command = false;
  static unsigned long command_start_time = 0;
  static unsigned long last_i2c_check_time = 0;

  unsigned long current_time = millis();

  // Periodic I2C bus health check (every 10 seconds)
  if (current_time - last_i2c_check_time >= 10000) {
    last_i2c_check_time = current_time;
    checkAndRecoverI2C();
  }

  if (in_command && (current_time - command_start_time > COMMAND_TIMEOUT_MS)) {
    in_command = false;
    command_len = 0;
  }

  if (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '<') {
      in_command = true;
      command_len = 0;
      command_start_time = current_time;
    } else if (c == '>') {
      if (in_command) {
        command_buffer[command_len] = '\0';
        process_command(command_buffer);
      }
      in_command = false;
    } else if (in_command) {
      if (command_len < (sizeof(command_buffer) - 1)) {
        command_buffer[command_len++] = c;
      } else {
        in_command = false;
        command_len = 0;
      }
    }
  }
}

// ======================================================================
//  SENSOR & DATA FUNCTIONS
// ======================================================================

unsigned long read_all_sensors() {
  int temp_val;
  uint16_t temp_uval;
  digitalWrite(DEBUG_LED_PIN, !digitalRead(DEBUG_LED_PIN));
  
  
  // Check and recover I2C bus before sensor readings
  checkAndRecoverI2C();
  
  // ADC read 1, space out ADC reads to avoid noise
  temp_val = analogRead(PRESSURE_SENSOR_PIN);
  float voltage = (temp_val / 1023.0) * 5.0; 
  float current_ma = (voltage / SHUNT_RESISTOR) * 1000.0;
  current_pressure_pa = (current_ma > 4.0) ? (current_ma - 4.0) * (300.0 / 16.0) : 0.0;
  
  // Check I2C bus before SPS30 operations
  checkAndRecoverI2C();
  int16_t ret_sps_read = sps30_read_data_ready(&temp_uval);
  if (ret_sps_read != 0) {
    Serial.println(F("<E,SPS30_DATA_READY_ERROR,1>"));
  }
  if (ret_sps_read == 0 && temp_uval) {
    int16_t ret_sps_measurement = sps30_read_measurement(&current_sps_data);
    if (ret_sps_measurement != 0) {
      Serial.println(F("<E,SPS30_MEASUREMENT_ERROR,1>"));
    }
  }
  
  // ADC read 2
  fan_amps = readCurrentRMS_Generic(FAN_CT_CLAMP_PIN, FAN_CT_VOLTS_PER_AMP);

  // Check I2C bus before SCD30 operations
  checkAndRecoverI2C();
  temp_val = scd30_sensor.getDataReady(temp_uval);
  if (temp_val != 0) {
    Serial.println(F("<E,SCD30_DATA_READY_ERROR,1>"));
  }
  if (temp_val == 0 && temp_uval) {
    temp_val = scd30_sensor.readMeasurementData(current_co2, current_temp_c, current_humi);
    if (temp_val != 0) {
      Serial.println(F("<E,SCD30_MEASUREMENT_ERROR,1>"));
    }
    if (temp_val == 0) {
      last_scd30_update = millis();
    }
  }
  
  if (millis() - last_scd30_update > SCD30_INVALIDATE_TIMEOUT_MS) {
    current_co2 = NAN;
    current_temp_c = NAN;
    current_humi = NAN;
  }
  
  uint16_t rh = static_cast<uint16_t>(current_humi * 65535.0f / 100.0f);
  uint16_t temp = static_cast<uint16_t>((current_temp_c + 45.0f) * 65535.0f / 175.0f);
  
  // ADC read 3
  compressor_amps = readCurrentRMS_Generic(COMPRESSOR_CT_CLAMP_PIN, COMPRESSOR_CT_VOLTS_PER_AMP);

  // Check I2C bus before SGP41 operations
  checkAndRecoverI2C();
  if (conditioning_s > 0) {
    temp_uval = sgp41_sensor.executeConditioning(rh, temp, current_voc_raw);
    if (temp_uval != 0) {
      Serial.println(F("<E,SGP41_CONDITIONING_ERROR,1>"));
    }
    conditioning_s--;
  } else {
    temp_uval = sgp41_sensor.measureRawSignals(rh, temp, current_voc_raw, current_nox_raw);
    if (temp_uval != 0) {
      Serial.println(F("<E,SGP41_MEASUREMENT_ERROR,1>"));
    }
  }
  
  if (temp_uval) {
    current_voc_raw = 0;
    current_nox_raw = 0;
  }

  // ADC read 4
  geothermal_pump_amps = readCurrentRMS_Generic(GEOTHERMAL_PUMP_CT_CLAMP_PIN, GEOTHERMAL_PUMP_CT_VOLTS_PER_AMP);
  return millis(); // Return the timestamp
}

void send_data_packet(unsigned long timestamp) {
  digitalWrite(DEBUG_LED_PIN, !digitalRead(DEBUG_LED_PIN));

  uint16_t pulse_count;
  noInterrupts();
  pulse_count = geiger_pulse_count;
  geiger_pulse_count = 0;
  interrupts();

  bool liquid_level_sensor_state = digitalRead(LIQUID_LEVEL_SENSOR_PIN);
  
  long p_val = (long)(current_pressure_pa * 10);
  long t_val = (long)(current_temp_c * 10);
  long h_val = (long)(current_humi * 10);
  long co2_val = (long)current_co2;
  long amps_val = (long)(fan_amps * 100);
  long pm1_val = (long)(current_sps_data.mc_1p0 * 10);
  long pm25_val = (long)(current_sps_data.mc_2p5 * 10);
  long pm4_val = (long)(current_sps_data.mc_4p0 * 10);
  long pm10_val = (long)(current_sps_data.mc_10p0 * 10);
  
  sprintf(tx_command_buffer, "%c%lu,%ld,%u,%ld,%ld,%ld,%u,%u,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%u",
          RSP_SENSORS, timestamp, p_val, pulse_count, t_val, h_val, co2_val, current_voc_raw, current_nox_raw,
          amps_val, pm1_val, pm25_val, pm4_val, pm10_val,
          (long)(compressor_amps * 100), (long)(geothermal_pump_amps * 100), liquid_level_sensor_state
          );

  uint8_t checksum = calculate_checksum(tx_command_buffer);

  Serial.print('<');
  Serial.print(tx_command_buffer);
  Serial.print(',');
  Serial.print(checksum);
  Serial.println('>');
}

void process_command(const char* buffer) {
  const char* comma = strrchr(buffer, ',');
  if (!comma) return;
  int data_len = comma - buffer;
  if (data_len <= 0 || data_len >= MAX_COMMAND_LEN) return;
  uint8_t received_checksum = atoi(comma + 1);
  char temp_char = buffer[data_len];
  const_cast<char*>(buffer)[data_len] = '\0';
  uint8_t calculated_checksum = calculate_checksum(buffer);
  const_cast<char*>(buffer)[data_len] = temp_char;
  if (received_checksum != calculated_checksum) return;

  char command = buffer[0];
  uint8_t checksum;
  uint32_t uint32_val = 0;
  uint8_t uint8_val1 = 0, uint8_val2 = 0;
  int16_t int_val = 0;
  uint16_t uint_val = 0, uint_val2 = 0;
  
  uint8_t i2c_address = 0;
  uint8_t i2c_num_bytes = 0;
  uint8_t i2c_data[I2C_PAYLOAD_BUFFER_SIZE];
  uint8_t i2c_status = I2C_ERROR_NONE;

  switch (command) {
    case CMD_GET_VERSION:
      sprintf(tx_command_buffer, "%c%s", RSP_VERSION, NANO_FIRMWARE_VERSION);
      checksum = calculate_checksum(tx_command_buffer);
      Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;

    case CMD_GET_HEALTH:
      sprintf(tx_command_buffer, "%c%d,%d,%d", RSP_HEALTH, first_health_status_sent ? 0 : 1, freeRam(), last_reset_cause);
      checksum = calculate_checksum(tx_command_buffer);
      Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;

    case CMD_ACK_HEALTH:
      first_health_status_sent = false;
      break;

    case CMD_REBOOT:
      wdt_enable(WDTO_15MS);
      while(1);
      break;

    case CMD_GET_SENSORS: {
      unsigned long timestamp = read_all_sensors();
      send_data_packet(timestamp);
      break;
    }

    case CMD_GET_SPS30_INFO: {
      strcpy(tx_command_buffer, "p");
      char* pos = tx_command_buffer + 1;
      int_val = sps30_read_firmware_version(&uint8_val1, &uint8_val2);
      pos += sprintf(pos, "%d,%u,%u,", int_val, uint8_val1, uint8_val2);
      int_val = sps30_get_fan_auto_cleaning_interval(&uint32_val);
      pos += sprintf(pos, "%d,%lu,", int_val, (unsigned long)uint32_val);
      int_val = sps30_get_fan_auto_cleaning_interval_days(&uint8_val1);
      pos += sprintf(pos, "%d,%lu,", int_val, (unsigned long)uint8_val1);
      int_val = sps30_read_device_status_register(&uint32_val);
      sprintf(pos, "%d,%lu", int_val, (unsigned long)uint32_val);
      checksum = calculate_checksum(tx_command_buffer);
      Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;
    }
    case CMD_SPS30_CLEAN: {
      int_val = sps30_start_manual_fan_cleaning();
      sprintf(tx_command_buffer, "%c%d", RSP_SPS30_CLEAN, int_val);
      checksum = calculate_checksum(tx_command_buffer);
      Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;
    }
    case CMD_SGP41_TEST: {
      uint16_t sgp41_ret = sgp41_sensor.executeSelfTest(uint_val);
      sprintf(tx_command_buffer, "%c%d,0x%4X", RSP_SGP41_TEST, sgp41_ret, uint_val);
      checksum = calculate_checksum(tx_command_buffer);
      Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;
    }
    case CMD_GET_SCD30_INFO: {
      strcpy(tx_command_buffer, "d");
      char* pos = tx_command_buffer + 1;
      int_val = scd30_sensor.getMeasurementInterval(uint_val);
      pos += sprintf(pos, "%X,%X,", int_val, uint_val);
      int_val = scd30_sensor.getAutoCalibrationStatus(uint_val);
      pos += sprintf(pos, "%X,%X,", int_val, uint_val);
      int_val = scd30_sensor.getForceRecalibrationStatus(uint_val);
      pos += sprintf(pos, "%X,%X,", int_val, uint_val);
      int_val = scd30_sensor.getTemperatureOffset(uint_val);
      pos += sprintf(pos, "%X,%X,", int_val, uint_val);
      int_val = scd30_sensor.getAltitudeCompensation(uint_val);
      pos += sprintf(pos, "%X,%X,", int_val, uint_val);
      int_val = scd30_sensor.readFirmwareVersion(uint8_val1, uint8_val2);
      sprintf(pos, ",%X,%X,%X", int_val, uint8_val1, uint8_val2);
      checksum = calculate_checksum(tx_command_buffer);
      Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;
    }
    case CMD_SET_SCD30_AUTOCAL: {
      uint_val = (data_len > 1 && buffer[1] == '1');
      int_val = scd30_sensor.activateAutoCalibration(uint_val);
      uint_val2 = scd30_sensor.getAutoCalibrationStatus(uint_val);
      sprintf(tx_command_buffer, "%c%X,%X,%X", RSP_SCD30_AUTOCAL, int_val, uint_val2, uint_val);
      checksum = calculate_checksum(tx_command_buffer);
      Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;
    }
    case CMD_SET_SCD30_FORCECAL: {
      uint_val = 0;
      if (data_len > 1) {
          uint_val = atoi(&buffer[1]);
      }
      int_val = scd30_sensor.forceRecalibration(uint_val);
      uint_val2 = scd30_sensor.getForceRecalibrationStatus(uint_val);
      sprintf(tx_command_buffer, "%c%X,%X,%X", RSP_SCD30_FORCECAL, int_val, uint_val2, uint_val);
      checksum = calculate_checksum(tx_command_buffer);
      Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;
    }
    
    case CMD_I2C_READ: {
        // Check and recover I2C bus before I2C operations
        checkAndRecoverI2C();
        
        char* p = const_cast<char*>(buffer + 1);
        char* endptr;

        i2c_address = strtol(p, &endptr, 16);
        if (p == endptr) { i2c_status = I2C_ERROR_OTHER; break; }
        p = endptr; if (*p == ',') p++;
        
        uint8_t write_len = strtol(p, &endptr, 16);
        if (p == endptr) { i2c_status = I2C_ERROR_OTHER; break; }
        p = endptr; if (*p == ',') p++;
        
        i2c_num_bytes = strtol(p, &endptr, 16);
        if (p == endptr) { i2c_status = I2C_ERROR_OTHER; break; }
        p = endptr;

        if (i2c_num_bytes > I2C_WIRE_LIB_MAX_READ) i2c_num_bytes = I2C_WIRE_LIB_MAX_READ;
        if (write_len > I2C_CMD_MAX_WRITE_IN_READ) { i2c_status = I2C_ERROR_BUF_LEN; break; }

        uint8_t write_data[I2C_CMD_MAX_WRITE_IN_READ];
        if (write_len > 0) {
            for (uint8_t i = 0; i < write_len; i++) {
                if (*p == ',') p++;
                write_data[i] = strtol(p, &endptr, 16);
                if (p == endptr) { i2c_status = I2C_ERROR_OTHER; break; }
                p = endptr;
            }
            if (i2c_status != I2C_ERROR_NONE) break;
        }

        if (write_len > 0) {
            Wire.beginTransmission(i2c_address);
            Wire.write(write_data, write_len);
            uint8_t i2c_result = Wire.endTransmission(false);

            if (i2c_result != 0) {
                i2c_status = (i2c_result == 2) ? I2C_ERROR_ADDR_NACK : I2C_ERROR_OTHER;
                // Trigger I2C recovery on write failure
                recoverI2Cbus();
            } else {
                delay(I2C_WRITE_READ_DELAY_MS);
            }
        }

        if (i2c_status == I2C_ERROR_NONE && i2c_num_bytes > 0) {
            uint8_t bytes_read = Wire.requestFrom(i2c_address, i2c_num_bytes);
            if (bytes_read != i2c_num_bytes) {
                i2c_status = I2C_ERROR_DATA_NACK;
                i2c_num_bytes = bytes_read;
                // Trigger I2C recovery on read failure
                recoverI2Cbus();
            }
            for (uint8_t i = 0; i < bytes_read; i++) {
                i2c_data[i] = Wire.read();
            }
        }
        
        sprintf(tx_command_buffer, "%c%02X,%02X", RSP_I2C_READ, i2c_status, i2c_num_bytes);
        if (i2c_status == I2C_ERROR_NONE && i2c_num_bytes > 0) {
            char* pos = tx_command_buffer + strlen(tx_command_buffer);
            for (uint8_t i = 0; i < i2c_num_bytes; i++) {
                pos += sprintf(pos, ",%02X", i2c_data[i]);
            }
        }
        
        checksum = calculate_checksum(tx_command_buffer);
        Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
        break;
    }
    
    case CMD_I2C_WRITE: {
        // Check and recover I2C bus before I2C operations
        checkAndRecoverI2C();
        
        char* p = const_cast<char*>(buffer + 1);
        char* endptr;

        i2c_address = strtol(p, &endptr, 16);
        if (p == endptr) { i2c_status = I2C_ERROR_OTHER; break; }
        p = endptr; if (*p == ',') p++;

        i2c_num_bytes = strtol(p, &endptr, 16);
        if (p == endptr) { i2c_status = I2C_ERROR_OTHER; break; }
        p = endptr;

        if (i2c_num_bytes > sizeof(i2c_data)) {
            i2c_status = I2C_ERROR_BUF_LEN;
            break;
        }

        for (uint8_t i = 0; i < i2c_num_bytes; i++) {
            if (*p == ',') p++;
            i2c_data[i] = strtol(p, &endptr, 16);
            if (p == endptr) {
                i2c_status = I2C_ERROR_OTHER;
                break;
            }
            p = endptr;
        }
        if (i2c_status != I2C_ERROR_NONE) break;

        Wire.beginTransmission(i2c_address);
        Wire.write(i2c_data, i2c_num_bytes);
        uint8_t i2c_result = Wire.endTransmission();
        
        switch (i2c_result) {
            case 0: i2c_status = I2C_ERROR_NONE; break;
            case 1: i2c_status = I2C_ERROR_DATA_NACK; break;
            case 2: i2c_status = I2C_ERROR_ADDR_NACK; break;
            case 3: i2c_status = I2C_ERROR_OTHER; break;
            case 4: i2c_status = I2C_ERROR_TIMEOUT; break;
            default: i2c_status = I2C_ERROR_OTHER; break;
        }
        
        // Trigger I2C recovery on any write failure
        if (i2c_status != I2C_ERROR_NONE) {
            recoverI2Cbus();
        }
        
        sprintf(tx_command_buffer, "%c%02X", RSP_I2C_WRITE, i2c_status);
        
        checksum = calculate_checksum(tx_command_buffer);
        Serial.print('<'); Serial.print(tx_command_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
        break;
    }

    default:
      break;
  }
}

// ======================================================================
//  I2C RECOVERY
// ======================================================================

/**
 * @brief Attempts to recover the I2C bus if a slave device is holding SDA low.
 * * This function manually toggles the SCL line up to 9 times to persuade a
 * stuck slave to release the SDA line. It then issues a manual STOP condition
 * before re-initializing the Wire library.
 */
void recoverI2Cbus() {
    Serial.println(F("<E,I2C_RECOVER,1>")); // Log recovery attempt

    // The standard I2C pins for Arduino Nano are A4 (SDA) and A5 (SCL).
    uint8_t sda_pin = A4;
    uint8_t scl_pin = A5;

    // Release the I2C hardware peripheral
    TWCR = 0; 
    
    // Set pins to OUTPUT to drive them manually
    pinMode(sda_pin, OUTPUT);
    pinMode(scl_pin, OUTPUT);
    
    digitalWrite(sda_pin, HIGH); // Try to release SDA
    digitalWrite(scl_pin, HIGH); // Release SCL
    
    // Check if the bus is already free
    pinMode(sda_pin, INPUT_PULLUP);
    if (digitalRead(sda_pin) == HIGH) {
        // Bus is free, re-initialize and exit
        Wire.begin(); 
        return;
    }

    // Slave is holding SDA low. Generate up to 9 clock pulses.
    for (int i = 0; i < 9; i++) {
        pinMode(scl_pin, OUTPUT);
        digitalWrite(scl_pin, LOW);
        delayMicroseconds(5);
        digitalWrite(scl_pin, HIGH);
        delayMicroseconds(5);
        
        // Check if slave has released SDA
        pinMode(sda_pin, INPUT_PULLUP); 
        if (digitalRead(sda_pin) == HIGH) {
            break; // Bus is free
        }
    }
    
    // Generate a STOP condition: SDA goes from LOW to HIGH while SCL is HIGH
    pinMode(scl_pin, OUTPUT);
    pinMode(sda_pin, OUTPUT);
    
    digitalWrite(scl_pin, LOW);
    delayMicroseconds(5);
    digitalWrite(sda_pin, LOW);
    delayMicroseconds(5);
    digitalWrite(scl_pin, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda_pin, HIGH);
    delayMicroseconds(5);

    // Re-initialize the I2C peripheral
    Wire.begin(); 
    delay(10);
    Serial.println(F("<E,I2C_RECOVER,0>")); // Log recovery finished
}

/**
 * @brief Checks if the I2C bus is stuck and calls recovery function if needed.
 * * @return true if the bus was stuck and a recovery was attempted.
 * @return false if the bus is OK.
 */
bool checkAndRecoverI2C() {
    pinMode(SDA, INPUT_PULLUP);
    if (digitalRead(SDA) == LOW) { // If SDA is stuck low
        recoverI2Cbus();
        // Re-check after recovery attempt
        return (digitalRead(SDA) == LOW); // Returns true if still stuck
    }
    return false; // Bus was not stuck
}
/*
 * ======================================================================
 * PLATFORMIO PROJECT FOR ARDUINO NANO (ATmega168 or ATmega328)
 * Reads Pressure, Geiger, SPS30, SCD30, SGP41, and TGS5042 sensors.
 * ======================================================================
 */
#include "SensorDefines.h"
#include <Arduino.h>
#include <SensirionI2CSgp41.h>
#include <SensirionI2cScd30.h>
#include <Wire.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <math.h>
#include <sps30.h>

#ifndef MINICORE
#error "This project requires the Minicore AVR core for Arduino."
#endif

// ======================================================================
//  CONFIGURATION & DEFINITIONS
// ======================================================================

// --- Firmware & Protocol ---
const char NANO_FIRMWARE_VERSION[] PROGMEM = "1.4.9";
#define CMD_GET_SENSORS 'S' // Request sensor data
#define RSP_SENSORS 's'     // Response with sensor data
#define CMD_GET_VERSION 'V'
#define CMD_GET_HEALTH 'H'
#define RSP_HEALTH 'h'     // Overall health status response
#define CMD_ACK_HEALTH 'A' // Acknowledge Health Status
#define RSP_VERSION 'v'
#define CMD_REBOOT 'R'     // Request Nano Reboot
#define CMD_BOOTLOADER 'B' // Request Nano Enter Bootloader
#define RSP_BOOTLOADER 'b' // Response/ACK for Nano Enter Bootloader

#define BOOT_KEY_ADDR ((volatile uint16_t *)(RAMEND - 3))
#define BOOT_INV_ADDR ((volatile uint16_t *)(RAMEND - 1))
#define BOOT_KEY_MAGIC ((uint16_t)0xB007)
#define BOOT_KEY_INVERSE ((uint16_t)0x4FF8) // ~0xB007
#define CMD_GET_SPS30_INFO 'P'              // Request SPS30 info
#define RSP_SPS30_INFO 'p'                  // Response with SPS30 info
#define CMD_SPS30_CLEAN 'C'                 // Request manual SPS30 fan cleaning
#define RSP_SPS30_CLEAN 'c'                 // ACK for fan cleaning command
#define CMD_SGP41_TEST 'G'                  // Request SGP41 test
#define RSP_SGP41_TEST 'g'                  // Response with SGP41 test result
#define CMD_GET_SCD30_INFO 'D'              // Request SCD30 info
#define RSP_SCD30_INFO 'd'                  // Response with SCD30 info
#define CMD_SET_SCD30_AUTOCAL 'T'           // Set SCD30 AutoCalibration
#define RSP_SCD30_AUTOCAL 't'      // Response with SCD30 AutoCalibration result
#define CMD_SET_SCD30_FORCECAL 'F' // Set SCD30 Forced Recalibration
#define RSP_SCD30_FORCECAL                                                     \
  'f' // Response with SCD30 Forced Recalibration result

// --- I2C Bridge Commands ---
#define CMD_I2C_READ 'I'  // Request I2C read operation
#define RSP_I2C_READ 'i'  // Response with I2C read result
#define CMD_I2C_WRITE 'W' // Request I2C write operation
#define RSP_I2C_WRITE 'w' // Response with I2C write result

// I2C Error Codes
#define I2C_ERROR_NONE 0x00      // No error
#define I2C_ERROR_ADDR_NACK 0x01 // Address not acknowledged
#define I2C_ERROR_DATA_NACK 0x02 // Data not acknowledged
#define I2C_ERROR_OTHER 0x03     // Other error
#define I2C_ERROR_TIMEOUT 0x04   // Timeout
#define I2C_ERROR_BUF_LEN 0x05   // Buffer length exceeded

// --- PROGMEM Error Strings ---
const char E_I2C_RECOVER_START_1[] PROGMEM = "E,I2C_RECOVER,1";
const char E_I2C_RECOVER_DONE_1[] PROGMEM = "E,I2C_RECOVER,0";
const char E_I2C_RECOVER_STUCK_2[] PROGMEM = "E,I2C_RECOVER,2";
const char E_I2C_BUS_STUCK_0[] PROGMEM = "E,I2C_RECOVER,BUS_STUCK,0";
const char E_I2C_BUS_STUCK_1[] PROGMEM = "E,I2C_RECOVER,BUS_STUCK,1";
const char E_I2C_RECOVER_FAIL_SDA_LOW_1[] PROGMEM =
    "E,I2C_RECOVER,FAIL_SDA_LOW";
const char E_I2C_RECOVER_FAIL_BUS_BUSY_1[] PROGMEM =
    "E,I2C_RECOVER,FAIL_BUS_BUSY";
const char E_I2C_TIMEOUT_1[] PROGMEM = "E,I2C_RECOVER,TIMEOUT";
const char E_SPS30_DATA_READY_ERROR_1[] PROGMEM = "E,SPS30_DATA_READY_ERROR,1";
const char E_SPS30_MEASUREMENT_ERROR_1[] PROGMEM =
    "E,SPS30_MEASUREMENT_ERROR,1";
const char E_SCD30_DATA_READY_ERROR_1[] PROGMEM = "E,SCD30_DATA_READY_ERROR,1";
const char E_SCD30_MEASUREMENT_ERROR_1[] PROGMEM =
    "E,SCD30_MEASUREMENT_ERROR,1";
const char E_SGP41_CONDITIONING_ERROR_1[] PROGMEM =
    "E,SGP41_CONDITIONING_ERROR,1";
const char E_SGP41_MEASUREMENT_ERROR_1[] PROGMEM =
    "E,SGP41_MEASUREMENT_ERROR,1";

// --- PROGMEM Format Strings ---
const char FMT_DATA_PACKET[] PROGMEM =
    "%c%lu,%u,%u,%ld,%ld,%ld,%s,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%u,%u";
const char FMT_GET_VERSION[] PROGMEM = "%c%s";
const char FMT_GET_HEALTH[] PROGMEM = "%c%d,%d,%d,%d";
const char FMT_SPS30_FW[] PROGMEM = "%d,%u,%u,";
const char FMT_SPS30_INTERVAL[] PROGMEM = "%d,%lu,";
const char FMT_SPS30_INTERVAL_DAYS[] PROGMEM = "%d,%u,";
const char FMT_SPS30_STATUS[] PROGMEM = "%d,%lu";
const char FMT_SPS30_CLEAN[] PROGMEM = "%c%d";
const char FMT_SGP41_TEST[] PROGMEM = "%c%d,0x%04X";
const char FMT_SCD30_READ[] PROGMEM = "%X,%X,";
const char FMT_SCD30_FW[] PROGMEM = ",%X,%X,%X";
const char FMT_SCD30_AUTOCAL[] PROGMEM = "%c%X,%X,%X";
const char FMT_SCD30_FORCECAL[] PROGMEM = "%c%X,%X,%X";
const char FMT_I2C_READ[] PROGMEM = "%c%02X,%02X";
const char FMT_I2C_READ_DATA[] PROGMEM = ",%02X";
const char FMT_I2C_WRITE[] PROGMEM = "%c%02X";

#define I2C_TIMEOUT_US 30000    // 30ms timeout for I2C operations
#define I2C_NORMAL_SPEED 100000 // Normal I2C speed (100 kHz)
#define I2C_FAST_SPEED 400000   // Fast I2C speed (400 kHz)

// --- Pin Definitions ---
#define PRESSURE_SENSOR_PIN A0
#define GEIGER_PIN 2
#define FAN_CT_CLAMP_PIN A2
#define DEBUG_LED_PIN 13
#define LIQUID_LEVEL_SENSOR_PIN 8
#define COMPRESSOR_CT_CLAMP_PIN A3
#define GEOTHERMAL_PUMP_CT_CLAMP_PIN A1
#define CO_SENSOR_PIN A6 // MCP616 output connected to A6

// --- Timing Constants ---
const unsigned long COMMAND_TIMEOUT_MS = 100;
const unsigned long SCD30_INVALIDATE_TIMEOUT_MS = 60000;
// SPS30 may not answer right after power-up: probe a few times before
// flagging it as faulty
const uint8_t SPS30_PROBE_ATTEMPTS = 5;
const unsigned long SPS30_PROBE_RETRY_DELAY_MS = 500;
// How often sensors flagged faulty are initialized again (e.g. reconnected)
const unsigned long SENSOR_RETRY_INTERVAL_MS = 10000;
// Failed reads in a row (one per sensor query, ~2 s apart) before a running
// sensor is flagged faulty
const uint8_t SENSOR_MAX_CONSECUTIVE_ERRORS = 3;

// --- Sensor Calculation Constants ---
#define SHUNT_RESISTOR 150.0f
#define AC_MAINS_FREQ_HZ 60 // 60 Hz mains frequency (North America)
#define FAN_CT_VOLTS_PER_AMP (1.0f / 10.0f) // 0.1 V/A, e.g. 1V at 10A
#define COMPRESSOR_CT_VOLTS_PER_AMP                                            \
  (1.0f / 30.0f) // 0.0333 V/A, SCT-013-030: 1V at 30A
#define GEOTHERMAL_PUMP_CT_VOLTS_PER_AMP                                       \
  (1.0f / 5.0f) // 0.2 V/A, 5A CT clamp: 1V at 5A
const uint8_t PRESSURE_ADC_NUM_SAMPLES = 4;

// --- Buffer Sizes ---
#define MAX_COMMAND_LEN 150
#define MAX_RESPONSE_LEN 200

// --- Hardware & Behavior Constants ---
#define SERIAL_BAUD_RATE 19200
#define I2C_PAYLOAD_BUFFER_SIZE 40 // Max bytes for an I2C data payload
#define I2C_WIRE_LIB_MAX_READ                                                  \
  32 // Max bytes the AVR Wire library can read at once
#define I2C_CMD_MAX_WRITE_IN_READ                                              \
  16                              // Max write bytes within a read-write command
#define I2C_WRITE_READ_DELAY_MS 2 // Brief delay between an I2C write and read
#define STARTUP_BLINK_DELAY_MS 500 // Delay for the startup LED blink

// ======================================================================
//  GLOBAL VARIABLES & OBJECTS
// ======================================================================

bool first_health_status_sent = true;
volatile uint16_t geiger_pulse_count = 0;
uint16_t pressure_adc_raw = 0;
float fan_amps = 0.0;
float compressor_amps = 0.0;
float geothermal_pump_amps = 0.0;
float current_co2 = NAN;
float current_temp_c = NAN;
float current_humi = NAN;
uint16_t co_adc_raw = 0;
uint16_t current_voc_raw = 0;
uint16_t current_nox_raw = 0;
struct sps30_measurement current_sps_data = {0};
unsigned long last_scd30_update = 0;
SensirionI2cScd30 scd30_sensor;
SensirionI2CSgp41 sgp41_sensor;
// SGP41 conditioning heats the NOx pixel hotplate to prepare it from cold boot.
// Sensirion datasheet specifies: "It is recommended to execute the conditioning
// for 10 s, but 10 s must not be exceeded to avoid damage to the sensing
// material." Autonomous conditioning runs at 1 Hz in loop() for 10 steps =
// exactly 10 seconds.
const uint8_t SGP41_CONDITIONING_STEPS = 10;
const unsigned long SGP41_CONDITIONING_PERIOD_MS = 1000;
uint8_t sgp41_conditioning_remaining = 0;
unsigned long last_sgp41_conditioning_tick = 0;
uint8_t sensor_fault_mask = 0; // SENSOR_FAULT_* bits
// Consecutive failed reads per sensor (see track_sensor_read())
uint8_t sps30_error_count = 0;
uint8_t scd30_error_count = 0;
uint8_t sgp41_error_count = 0;
char tx_command_buffer[MAX_RESPONSE_LEN];

// --- Round-robin ADC reading variables ---
enum ADC_CHANNEL {
  ADC_PRESSURE = 0,
  ADC_FAN_CT,
  ADC_COMPRESSOR_CT,
  ADC_GEOTHERMAL_PUMP_CT,
  ADC_CO_SENSOR,
  ADC_CHANNEL_COUNT
};

uint8_t current_adc_channel = ADC_PRESSURE;
unsigned long last_adc_read_time = 0;
const unsigned long ADC_READ_INTERVAL_MS = 10; // 10ms between ADC reads

// --- CT Clamp Burst Sample Duration ---
const unsigned long CT_BURST_DURATION_US =
    (2000000UL / AC_MAINS_FREQ_HZ); // 2 full cycles (33,333 us for 60 Hz)

// --- Rolling average variables for CO sensor ---
const float CO_EMA_ALPHA =
    0.3f; // Smoothing factor (0.3 = 30% new data, 70% old average)
float co_adc_ema = 0.0f; // Exponential moving average
bool co_adc_ema_initialized = false;

// --- Geiger stale timeout variables ---
const unsigned long GEIGER_STALE_TIMEOUT_MS = 4500;
unsigned long last_geiger_poll_time = 0;

// --- Forward Declarations ---
unsigned long read_all_sensors();
void send_data_packet(unsigned long timestamp);
void process_command(const char *buffer);
int freeRam();
bool recoverI2Cbus();
bool checkAndRecoverI2C();
void checkAndReportI2cTimeout();
void send_error_response(const char *error_msg PROGMEM);
void read_single_adc_channel();
float read_ct_clamp_rms(uint8_t pin, float volts_per_amp);

// ======================================================================

void on_geiger_pulse() { geiger_pulse_count++; }

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

uint8_t calculate_checksum(const char *data_str) {
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

// ======================================================================
//  ROUND-ROBIN ADC READING
// ======================================================================

float read_ct_clamp_rms(uint8_t pin, float volts_per_amp) {
  // Settle ADC multiplexer after channel switch
  analogRead(pin);
  delayMicroseconds(20);

  unsigned long start_time = micros();
  long sum_samples = 0;
  long sum_squares = 0;
  uint16_t sample_count = 0;

  // Sample densely across 2 full AC cycles (33.33 ms for 60 Hz) for true
  // cycle-integrated RMS
  while (micros() - start_time < CT_BURST_DURATION_US) {
    int reading = analogRead(pin);
    sum_samples += reading;
    sum_squares += (long)reading * reading;
    sample_count++;
  }

  if (sample_count == 0)
    return 0.0f;

  float mean_of_samples = (float)sum_samples / sample_count;
  float mean_of_squares = (float)sum_squares / sample_count;
  float variance = mean_of_squares - (mean_of_samples * mean_of_samples);
  if (variance < 0.0f) {
    variance = 0.0f;
  }
  float rms_adc = sqrt(variance);
  float voltage = rms_adc * (5.0f / 1023.0f);
  return (voltage / volts_per_amp);
}

void read_single_adc_channel() {
  unsigned long current_time = millis();

  // Only read ADC if enough time has passed
  if (current_time - last_adc_read_time < ADC_READ_INTERVAL_MS) {
    return;
  }

  last_adc_read_time = current_time;

  switch (current_adc_channel) {
  case ADC_PRESSURE: {
    // Discard first reading after switching to settle S/H capacitor, then
    // average readings
    analogRead(PRESSURE_SENSOR_PIN);
    delayMicroseconds(20);
    uint16_t sum = 0;
    for (uint8_t i = 0; i < PRESSURE_ADC_NUM_SAMPLES; i++) {
      sum += analogRead(PRESSURE_SENSOR_PIN);
    }
    // Integer division rounding: adding half the divisor (N/2) rounds to
    // nearest integer
    pressure_adc_raw =
        (sum + (PRESSURE_ADC_NUM_SAMPLES / 2)) / PRESSURE_ADC_NUM_SAMPLES;
    break;
  }

  case ADC_FAN_CT:
    fan_amps = read_ct_clamp_rms(FAN_CT_CLAMP_PIN, FAN_CT_VOLTS_PER_AMP);
    break;

  case ADC_COMPRESSOR_CT:
    compressor_amps =
        read_ct_clamp_rms(COMPRESSOR_CT_CLAMP_PIN, COMPRESSOR_CT_VOLTS_PER_AMP);
    break;

  case ADC_GEOTHERMAL_PUMP_CT:
    geothermal_pump_amps = read_ct_clamp_rms(GEOTHERMAL_PUMP_CT_CLAMP_PIN,
                                             GEOTHERMAL_PUMP_CT_VOLTS_PER_AMP);
    break;

  case ADC_CO_SENSOR: {
    // Discard first reading after switching multiplexer to high-impedance CO
    // op-amp channel to allow S/H capacitor to settle
    analogRead(CO_SENSOR_PIN);
    delayMicroseconds(50);
    uint16_t reading = analogRead(CO_SENSOR_PIN);

    // Update EMA (floating-point version)
    if (!co_adc_ema_initialized) {
      co_adc_ema = (float)reading;
      co_adc_ema_initialized = true;
    } else {
      co_adc_ema =
          (float)reading * CO_EMA_ALPHA + co_adc_ema * (1.0f - CO_EMA_ALPHA);
    }
    co_adc_raw = (uint16_t)co_adc_ema;

    break;
  }
  }

  // Move to next channel
  current_adc_channel = (current_adc_channel + 1) % ADC_CHANNEL_COUNT;
}

// ======================================================================
//  SETUP
// ======================================================================

// ======================================================================
//  SENSOR INITIALIZATION
// ======================================================================
// Each returns true when the sensor answered and is measuring. A sensor that
// fails is flagged in sensor_fault_mask by setup() and retried from loop().

bool init_sps30(uint8_t attempts) {
  bool ok = false;
  for (uint8_t attempt = 0; attempt < attempts && !ok; attempt++) {
    if (attempt > 0) {
      delay(SPS30_PROBE_RETRY_DELAY_MS);
      checkAndRecoverI2C();
    }
    ok = (sps30_probe() == 0);
  }
  return ok && sps30_start_measurement() >= 0;
}

bool init_scd30() {
  scd30_sensor.begin(Wire, SCD30_I2C_ADDR_61);
  return scd30_sensor.startPeriodicMeasurement(0) == 0;
}

bool init_sgp41(bool force_conditioning = false) {
  Wire.setClock(I2C_FAST_SPEED);
  sgp41_sensor.begin(Wire);
  // begin() does not talk to the sensor: read its serial number to check it
  uint16_t serial[3];
  bool ok = (sgp41_sensor.getSerialNumber(serial) == 0);
  Wire.setClock(I2C_NORMAL_SPEED);
  if (ok && force_conditioning) {
    sgp41_conditioning_remaining = SGP41_CONDITIONING_STEPS;
    last_sgp41_conditioning_tick = 0;
  }
  return ok;
}

// Counts the result of a sensor read in read_all_sensors(): after too many
// failures in a row (sensor pulled or broken while running), the sensor is
// flagged faulty like at boot, then skipped and retried from loop().
void track_sensor_read(uint8_t fault_bit, uint8_t &error_count, bool ok) {
  if (ok) {
    error_count = 0;
  } else if (++error_count >= SENSOR_MAX_CONSECUTIVE_ERRORS) {
    error_count = 0;
    sensor_fault_mask |= fault_bit;
  }
}

// One quick attempt per faulty sensor: an absent sensor NACKs right away
void retry_faulty_sensors() {
  if ((sensor_fault_mask & SENSOR_FAULT_SPS30) && init_sps30(1))
    sensor_fault_mask &= ~SENSOR_FAULT_SPS30;
  if ((sensor_fault_mask & SENSOR_FAULT_SCD30) && init_scd30())
    sensor_fault_mask &= ~SENSOR_FAULT_SCD30;
  if ((sensor_fault_mask & SENSOR_FAULT_SGP41) && init_sgp41(true))
    sensor_fault_mask &= ~SENSOR_FAULT_SGP41;
}

volatile uint8_t urboot_reset_flags __attribute__((section(".noinit")));
uint8_t last_reset_cause = 0;

void capture_r2(void) __attribute__((naked, section(".init0"), used));
void capture_r2(void) { asm volatile("sts urboot_reset_flags, r2\n"); }

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  if (urboot_reset_flags & (1 << 3))
    last_reset_cause = RESET_CAUSE_WATCHDOG;
  else if (urboot_reset_flags & (1 << 2))
    last_reset_cause = RESET_CAUSE_BROWN_OUT;
  else if (urboot_reset_flags & (1 << 1))
    last_reset_cause = RESET_CAUSE_EXTERNAL;
  else if (urboot_reset_flags & (1 << 0))
    last_reset_cause = RESET_CAUSE_POWER_ON;
  else
    last_reset_cause = RESET_CAUSE_UNKNOWN;

  pinMode(DEBUG_LED_PIN, OUTPUT);
  digitalWrite(DEBUG_LED_PIN, HIGH);

  // Check and recover I2C bus before initializing sensors
  checkAndRecoverI2C();

  sensirion_i2c_init();
  Wire.setWireTimeout(I2C_TIMEOUT_US, true); // reset on timeout
  Wire.setClock(I2C_NORMAL_SPEED);

  // A sensor that fails to initialize is flagged and skipped, never waited on:
  // setup() must always reach loop() so the ESP32 link (and OTA) keeps working.
  if (!init_sps30(SPS30_PROBE_ATTEMPTS))
    sensor_fault_mask |= SENSOR_FAULT_SPS30;
  if (!init_scd30())
    sensor_fault_mask |= SENSOR_FAULT_SCD30;
  // SGP41 conditioning only runs on true cold boot (Power-On Reset)
  if (!init_sgp41(last_reset_cause == RESET_CAUSE_POWER_ON))
    sensor_fault_mask |= SENSOR_FAULT_SGP41;

  pinMode(LIQUID_LEVEL_SENSOR_PIN, INPUT_PULLUP);

  pinMode(GEIGER_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), on_geiger_pulse, RISING);

  pinMode(PRESSURE_SENSOR_PIN, INPUT);
  pinMode(FAN_CT_CLAMP_PIN, INPUT);
  pinMode(COMPRESSOR_CT_CLAMP_PIN, INPUT);
  pinMode(GEOTHERMAL_PUMP_CT_CLAMP_PIN, INPUT);
  pinMode(CO_SENSOR_PIN, INPUT);
  last_geiger_poll_time = millis();
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

  // Reset geiger pulse count if no query received for 4.5 seconds (e.g. ESP32
  // rebooting or disconnected)
  if (current_time - last_geiger_poll_time >= GEIGER_STALE_TIMEOUT_MS) {
    noInterrupts();
    geiger_pulse_count = 0;
    interrupts();
    last_geiger_poll_time = current_time;
  }

  // Periodic I2C bus health check (every 10 seconds)
  if (current_time - last_i2c_check_time >= 10000) {
    last_i2c_check_time = current_time;
    if (checkAndRecoverI2C()) {
      send_error_response(E_I2C_BUS_STUCK_0);
    }
  }

  // Retry sensors flagged faulty; the ESP32 sees the change in the next
  // health response. Not while a command is being received.
  static unsigned long last_sensor_retry_time = 0;
  if (sensor_fault_mask && !in_command &&
      current_time - last_sensor_retry_time >= SENSOR_RETRY_INTERVAL_MS) {
    last_sensor_retry_time = current_time;
    retry_faulty_sensors();
  }

  // Timeout check for incomplete incoming commands before reading new serial
  // bytes
  if (in_command && (current_time - command_start_time > COMMAND_TIMEOUT_MS)) {
    in_command = false;
    command_len = 0;
  }

  // Process incoming serial data with priority until end-of-frame '>'
  while (Serial.available() > 0) {
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
      command_len = 0;
      break; // End of frame received: exit loop so serial chatter does not
             // starve main loop
    } else if (in_command) {
      if (command_len < (sizeof(command_buffer) - 1)) {
        command_buffer[command_len++] = c;
      } else {
        in_command = false;
        command_len = 0;
      }
    }
  }

  // Read one ADC channel per round-robin tick only when not assembling a serial
  // command
  if (!in_command) {
    read_single_adc_channel();
  }

  // Autonomous 1 Hz SGP41 conditioning (10 steps = exactly 10 seconds)
  // following cold boot or reconnect
  if (sgp41_conditioning_remaining > 0 && !in_command) {
    if (sensor_fault_mask & SENSOR_FAULT_SGP41) {
      sgp41_conditioning_remaining = 0;
    } else if (current_time - last_sgp41_conditioning_tick >=
               SGP41_CONDITIONING_PERIOD_MS) {
      last_sgp41_conditioning_tick = current_time;
      Wire.setClock(I2C_FAST_SPEED);
      uint16_t dummy_voc = 0;
      uint16_t err = sgp41_sensor.executeConditioning(
          SGP41_DEFAULT_RH_TICKS, SGP41_DEFAULT_TEMP_TICKS, dummy_voc);
      Wire.setClock(I2C_NORMAL_SPEED);
      if (err != 0) {
        send_error_response(E_SGP41_CONDITIONING_ERROR_1);
      }
      sgp41_conditioning_remaining--;
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

  // If the bus is stuck (and recovery fails), abort the entire sensor read
  // cycle.
  if (checkAndRecoverI2C()) {
    send_error_response(E_I2C_BUS_STUCK_1);
    return millis(); // Return early
  }

  Wire.setClock(I2C_NORMAL_SPEED);
  // Faulty sensors are skipped: the ESP32 ignores their values
  if (!(sensor_fault_mask & SENSOR_FAULT_SPS30)) {
    int16_t ret_sps_read = sps30_read_data_ready(&temp_uval);
    bool sps30_ok = (ret_sps_read == 0);
    if (ret_sps_read != 0) {
      send_error_response(E_SPS30_DATA_READY_ERROR_1);
    }
    if (ret_sps_read == 0 && temp_uval) {
      int16_t ret_sps_measurement = sps30_read_measurement(&current_sps_data);
      if (ret_sps_measurement != 0) {
        send_error_response(E_SPS30_MEASUREMENT_ERROR_1);
        sps30_ok = false;
      }
    }
    track_sensor_read(SENSOR_FAULT_SPS30, sps30_error_count, sps30_ok);
  }

  if (!(sensor_fault_mask & SENSOR_FAULT_SCD30)) {
    temp_val = scd30_sensor.getDataReady(temp_uval);
    bool scd30_ok = (temp_val == 0);
    if (temp_val != 0) {
      send_error_response(E_SCD30_DATA_READY_ERROR_1);
    }
    if (temp_val == 0 && temp_uval) {
      temp_val = scd30_sensor.readMeasurementData(current_co2, current_temp_c,
                                                  current_humi);
      if (temp_val != 0) {
        send_error_response(E_SCD30_MEASUREMENT_ERROR_1);
        scd30_ok = false;
      }
      if (temp_val == 0) {
        last_scd30_update = millis();
      }
    }
    track_sensor_read(SENSOR_FAULT_SCD30, scd30_error_count, scd30_ok);
  }

  if (millis() - last_scd30_update > SCD30_INVALIDATE_TIMEOUT_MS) {
    current_co2 = NAN;
    current_temp_c = NAN;
    current_humi = NAN;
  }

  bool scd30_has_data =
      (last_scd30_update > 0 && !isnan(current_temp_c) && !isnan(current_humi));
  bool scd30_warmup_expired = (millis() >= SCD30_WARMUP_TIMEOUT_MS);

  uint16_t rh = SGP41_DEFAULT_RH_TICKS;
  uint16_t temp = SGP41_DEFAULT_TEMP_TICKS;

  if (scd30_has_data && current_humi >= SCD30_HUMI_MIN_VALID_PCT &&
      current_humi <= SCD30_HUMI_MAX_VALID_PCT) {
    rh = static_cast<uint16_t>(current_humi * 65535.0f / 100.0f);
  }
  if (scd30_has_data && current_temp_c >= SCD30_TEMP_MIN_VALID_C &&
      current_temp_c <= SCD30_TEMP_MAX_VALID_C) {
    temp = static_cast<uint16_t>((current_temp_c + 45.0f) * 65535.0f / 175.0f);
  }

  if (sensor_fault_mask & SENSOR_FAULT_SGP41) {
    current_voc_raw = 0;
    current_nox_raw = 0;
    checkAndReportI2cTimeout();
    return millis();
  }

  if (sgp41_conditioning_remaining > 0) {
    current_voc_raw = 0;
    current_nox_raw = 0;
  } else {
    Wire.setClock(I2C_FAST_SPEED);
    temp_uval = sgp41_sensor.measureRawSignals(rh, temp, current_voc_raw,
                                               current_nox_raw);
    if (temp_uval != 0) {
      send_error_response(E_SGP41_MEASUREMENT_ERROR_1);
    }
    Wire.setClock(I2C_NORMAL_SPEED);
    track_sensor_read(SENSOR_FAULT_SGP41, sgp41_error_count, temp_uval == 0);

    if (temp_uval != 0 || (!scd30_has_data && !scd30_warmup_expired)) {
      current_voc_raw = 0;
      current_nox_raw = 0;
    }
  }

  checkAndReportI2cTimeout();

  return millis(); // Return the timestamp
}

void send_data_packet(unsigned long timestamp) {
  digitalWrite(DEBUG_LED_PIN, !digitalRead(DEBUG_LED_PIN));

  uint16_t pulse_count;
  noInterrupts();
  pulse_count = geiger_pulse_count;
  geiger_pulse_count = 0;
  interrupts();
  last_geiger_poll_time = millis();

  bool liquid_level_sensor_state = digitalRead(LIQUID_LEVEL_SENSOR_PIN);

  long t_val = isnan(current_temp_c) ? 0 : (long)(current_temp_c * 10);
  long h_val = isnan(current_humi) ? 0 : (long)(current_humi * 10);
  long co2_val = isnan(current_co2) ? 0 : (long)current_co2;
  long amps_val = (long)(fan_amps * 100);
  long pm1_val = (long)(current_sps_data.mc_1p0 * 10);
  long pm25_val = (long)(current_sps_data.mc_2p5 * 10);
  long pm4_val = (long)(current_sps_data.mc_4p0 * 10);
  long pm10_val = (long)(current_sps_data.mc_10p0 * 10);

  char sgp41_str[16];
  if (sgp41_conditioning_remaining > 0 &&
      !(sensor_fault_mask & SENSOR_FAULT_SGP41)) {
    strcpy_P(sgp41_str, PSTR("c,c"));
  } else {
    snprintf_P(sgp41_str, sizeof(sgp41_str), PSTR("%u,%u"), current_voc_raw,
               current_nox_raw);
  }

  snprintf_P(tx_command_buffer, sizeof(tx_command_buffer), FMT_DATA_PACKET,
             RSP_SENSORS, timestamp, pressure_adc_raw, pulse_count, t_val,
             h_val, co2_val, sgp41_str, amps_val, pm1_val, pm25_val, pm4_val,
             pm10_val, (long)(compressor_amps * 100),
             (long)(geothermal_pump_amps * 100), liquid_level_sensor_state,
             co_adc_raw);

  const uint8_t checksum = calculate_checksum(tx_command_buffer);

  Serial.print('<');
  Serial.print(tx_command_buffer);
  Serial.print(',');
  Serial.print(checksum);
  Serial.println('>');
}

// Parses the next comma-separated hex field of an I2C bridge command.
// Returns false when no field is left.
static bool next_hex_field(char **p, uint8_t *out) {
  if (**p == ',')
    (*p)++;
  char *endptr;
  long value = strtol(*p, &endptr, 16);
  if (endptr == *p)
    return false;
  *p = endptr;
  *out = (uint8_t)value;
  return true;
}

// Takes the sequence tag (always the last field of an I2C bridge command) off
// the end, so the fields before it parse on their own and even a malformed
// command gets a tagged reply.
static uint8_t take_seq_field(char *fields) {
  char *last = strrchr(fields, ',');
  if (!last)
    return 0;
  *last = '\0';
  return (uint8_t)strtol(last + 1, nullptr, 16);
}

// Sends the single response every I2C bridge command gets, success or not.
// The request's sequence tag is echoed as the last field so the ESP32 can
// discard replies to requests it already gave up on.
static void send_i2c_response(char rsp, uint8_t status, const uint8_t *data,
                              uint8_t len, uint8_t seq) {
  char *pos = tx_command_buffer;
  const char *end = tx_command_buffer + sizeof(tx_command_buffer);
  if (rsp == RSP_I2C_READ) {
    pos += snprintf_P(pos, end - pos, FMT_I2C_READ, rsp, status, len);
    for (uint8_t i = 0; i < len; i++)
      pos += snprintf_P(pos, end - pos, FMT_I2C_READ_DATA, data[i]);
  } else {
    pos += snprintf_P(pos, end - pos, FMT_I2C_WRITE, rsp, status);
  }
  snprintf_P(pos, end - pos, FMT_I2C_READ_DATA, seq);

  const uint8_t checksum = calculate_checksum(tx_command_buffer);
  Serial.print('<');
  Serial.print(tx_command_buffer);
  Serial.print(',');
  Serial.print(checksum);
  Serial.println('>');
}

void process_command(const char *buffer) {
  const char *comma = strrchr(buffer, ',');
  if (!comma)
    return;
  int data_len = comma - buffer;
  if (data_len <= 0 || data_len >= MAX_COMMAND_LEN)
    return;
  const uint8_t received_checksum = atoi(comma + 1);
  // Terminate at the checksum comma (kept that way): command parsers must
  // never read the checksum digits as data
  const_cast<char *>(buffer)[data_len] = '\0';
  const uint8_t calculated_checksum = calculate_checksum(buffer);
  if (received_checksum != calculated_checksum)
    return;

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
  case CMD_GET_VERSION: {
    char temp_version[10];
    strncpy_P(temp_version, NANO_FIRMWARE_VERSION, 10);
    snprintf_P(tx_command_buffer, sizeof(tx_command_buffer), FMT_GET_VERSION,
               RSP_VERSION, temp_version);
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    break;
  }

  case CMD_GET_HEALTH:
    snprintf_P(tx_command_buffer, sizeof(tx_command_buffer), FMT_GET_HEALTH,
               RSP_HEALTH, first_health_status_sent ? 0 : 1, freeRam(),
               last_reset_cause, sensor_fault_mask);
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    break;

  case CMD_ACK_HEALTH:
    first_health_status_sent = false;
    break;

  case CMD_BOOTLOADER: {
    tx_command_buffer[0] = RSP_BOOTLOADER;
    tx_command_buffer[1] = '\0';
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    Serial.flush();

    MCUSR = 0;
    *BOOT_KEY_ADDR = BOOT_KEY_MAGIC;
    *BOOT_INV_ADDR = BOOT_KEY_INVERSE;
    wdt_enable(WDTO_15MS);
    while (1)
      ;
    break;
  }

  case CMD_REBOOT:
    wdt_enable(WDTO_15MS);
    while (1)
      ;
    break;

  case CMD_GET_SENSORS: {
    unsigned long timestamp = read_all_sensors();
    send_data_packet(timestamp);
    break;
  }

  case CMD_GET_SPS30_INFO: {
    tx_command_buffer[0] = RSP_SPS30_INFO;
    tx_command_buffer[1] = '\0';
    char *pos = tx_command_buffer + 1;
    Wire.setClock(I2C_NORMAL_SPEED);
    int_val = sps30_read_firmware_version(&uint8_val1, &uint8_val2);
    pos +=
        snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
                   FMT_SPS30_FW, int_val, uint8_val1, uint8_val2);
    int_val = sps30_get_fan_auto_cleaning_interval(&uint32_val);
    pos +=
        snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
                   FMT_SPS30_INTERVAL, int_val, uint32_val);
    int_val = sps30_get_fan_auto_cleaning_interval_days(&uint8_val1);
    pos +=
        snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
                   FMT_SPS30_INTERVAL_DAYS, int_val, uint8_val1);
    int_val = sps30_read_device_status_register(&uint32_val);
    snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
               FMT_SPS30_STATUS, int_val, uint32_val);
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    break;
  }
  case CMD_SPS30_CLEAN: {
    Wire.setClock(I2C_NORMAL_SPEED);
    int_val = sps30_start_manual_fan_cleaning();
    snprintf_P(tx_command_buffer, sizeof(tx_command_buffer), FMT_SPS30_CLEAN,
               RSP_SPS30_CLEAN, int_val);
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    break;
  }
  case CMD_SGP41_TEST: {
    Wire.setClock(I2C_FAST_SPEED);
    uint16_t sgp41_ret = sgp41_sensor.executeSelfTest(uint_val);
    Wire.setClock(I2C_NORMAL_SPEED);
    snprintf_P(tx_command_buffer, sizeof(tx_command_buffer), FMT_SGP41_TEST,
               RSP_SGP41_TEST, sgp41_ret, uint_val);
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    break;
  }
  case CMD_GET_SCD30_INFO: {
    tx_command_buffer[0] = RSP_SCD30_INFO;
    tx_command_buffer[1] = '\0';
    char *pos = tx_command_buffer + 1;
    Wire.setClock(I2C_NORMAL_SPEED);
    int_val = scd30_sensor.getMeasurementInterval(uint_val);
    pos +=
        snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
                   FMT_SCD30_READ, int_val, uint_val);
    int_val = scd30_sensor.getAutoCalibrationStatus(uint_val);
    pos +=
        snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
                   FMT_SCD30_READ, int_val, uint_val);
    int_val = scd30_sensor.getForceRecalibrationStatus(uint_val);
    pos +=
        snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
                   FMT_SCD30_READ, int_val, uint_val);
    int_val = scd30_sensor.getTemperatureOffset(uint_val);
    pos +=
        snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
                   FMT_SCD30_READ, int_val, uint_val);
    int_val = scd30_sensor.getAltitudeCompensation(uint_val);
    pos +=
        snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
                   FMT_SCD30_READ, int_val, uint_val);
    int_val = scd30_sensor.readFirmwareVersion(uint8_val1, uint8_val2);
    snprintf_P(pos, sizeof(tx_command_buffer) - (pos - tx_command_buffer),
               FMT_SCD30_FW, int_val, uint8_val1, uint8_val2);
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    break;
  }
  case CMD_SET_SCD30_AUTOCAL: {
    uint_val = (data_len > 1 && buffer[1] == '1');
    Wire.setClock(I2C_NORMAL_SPEED);
    int_val = scd30_sensor.activateAutoCalibration(uint_val);
    uint_val2 = scd30_sensor.getAutoCalibrationStatus(uint_val);
    snprintf_P(tx_command_buffer, sizeof(tx_command_buffer), FMT_SCD30_AUTOCAL,
               RSP_SCD30_AUTOCAL, int_val, uint_val2, uint_val);
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    break;
  }
  case CMD_SET_SCD30_FORCECAL: {
    uint_val = 0;
    if (data_len > 1) {
      uint_val = atoi(&buffer[1]);
    }
    if (isValidForceCal(uint_val)) {
      Wire.setClock(I2C_NORMAL_SPEED);
      int_val = scd30_sensor.forceRecalibration(uint_val);
      uint_val2 = scd30_sensor.getForceRecalibrationStatus(uint_val);
    } else {
      int_val = -1;
      uint_val2 = 0;
      uint_val = 0;
    }
    snprintf_P(tx_command_buffer, sizeof(tx_command_buffer), FMT_SCD30_FORCECAL,
               RSP_SCD30_FORCECAL, int_val, uint_val2, uint_val);
    checksum = calculate_checksum(tx_command_buffer);
    Serial.print('<');
    Serial.print(tx_command_buffer);
    Serial.print(',');
    Serial.print(checksum);
    Serial.println('>');
    break;
  }

  case CMD_I2C_READ: {
    // I<addr>,<write_len>,<read_len>[,<write bytes>...],<seq>
    checkAndRecoverI2C();

    char *p = const_cast<char *>(buffer + 1);
    const uint8_t seq = take_seq_field(p);
    uint8_t write_len = 0;
    uint8_t write_data[I2C_CMD_MAX_WRITE_IN_READ];

    if (!next_hex_field(&p, &i2c_address) || !next_hex_field(&p, &write_len) ||
        !next_hex_field(&p, &i2c_num_bytes)) {
      i2c_status = I2C_ERROR_OTHER;
    } else if (write_len > I2C_CMD_MAX_WRITE_IN_READ ||
               i2c_num_bytes > I2C_WIRE_LIB_MAX_READ) {
      i2c_status = I2C_ERROR_BUF_LEN;
    } else {
      for (uint8_t i = 0; i < write_len; i++) {
        if (!next_hex_field(&p, &write_data[i])) {
          i2c_status = I2C_ERROR_OTHER;
          break;
        }
      }
    }

    uint8_t bytes_read = 0;
    if (i2c_status == I2C_ERROR_NONE) {
      Wire.setClock(I2C_FAST_SPEED);
      if (write_len > 0) {
        Wire.beginTransmission(i2c_address);
        Wire.write(write_data, write_len);
        uint8_t i2c_result = Wire.endTransmission(false);

        if (i2c_result != 0) {
          i2c_status =
              (i2c_result == 2) ? I2C_ERROR_ADDR_NACK : I2C_ERROR_OTHER;
          recoverI2Cbus();
        } else {
          delay(I2C_WRITE_READ_DELAY_MS);
        }
      }

      if (i2c_status == I2C_ERROR_NONE && i2c_num_bytes > 0) {
        bytes_read = Wire.requestFrom(i2c_address, i2c_num_bytes);
        for (uint8_t i = 0; i < bytes_read; i++) {
          i2c_data[i] = Wire.read();
        }
        if (bytes_read != i2c_num_bytes) {
          i2c_status = I2C_ERROR_DATA_NACK;
          recoverI2Cbus();
        }
      }
      Wire.setClock(I2C_NORMAL_SPEED);

      checkAndReportI2cTimeout();
    }

    send_i2c_response(RSP_I2C_READ, i2c_status, i2c_data,
                      i2c_status == I2C_ERROR_NONE ? bytes_read : 0, seq);
    break;
  }

  case CMD_I2C_WRITE: {
    // W<addr>,<len>[,<bytes>...],<seq>
    checkAndRecoverI2C();

    char *p = const_cast<char *>(buffer + 1);
    const uint8_t seq = take_seq_field(p);

    if (!next_hex_field(&p, &i2c_address) ||
        !next_hex_field(&p, &i2c_num_bytes)) {
      i2c_status = I2C_ERROR_OTHER;
    } else if (i2c_num_bytes > sizeof(i2c_data) ||
               i2c_num_bytes > TWI_BUFFER_SIZE) {
      // Wire silently drops bytes past its TX buffer and still reports
      // success: never send a truncated write as if it were complete
      i2c_status = I2C_ERROR_BUF_LEN;
    } else {
      for (uint8_t i = 0; i < i2c_num_bytes; i++) {
        if (!next_hex_field(&p, &i2c_data[i])) {
          i2c_status = I2C_ERROR_OTHER;
          break;
        }
      }
    }

    if (i2c_status == I2C_ERROR_NONE) {
      Wire.setClock(I2C_FAST_SPEED);
      Wire.beginTransmission(i2c_address);
      Wire.write(i2c_data, i2c_num_bytes);
      uint8_t i2c_result = Wire.endTransmission();
      if (i2c_result != 0) {
        i2c_status = (i2c_result == 2) ? I2C_ERROR_ADDR_NACK : I2C_ERROR_OTHER;
        recoverI2Cbus();
      }
      Wire.setClock(I2C_NORMAL_SPEED);

      checkAndReportI2cTimeout();
    }

    send_i2c_response(RSP_I2C_WRITE, i2c_status, nullptr, 0, seq);
    break;
  }

  default:
    break;
  }
}

// ======================================================================
//  I2C RECOVERY
// ======================================================================

bool recoverI2Cbus() {
  send_error_response(E_I2C_RECOVER_START_1);

  const uint8_t sda_pin = SDA;
  const uint8_t scl_pin = SCL;

  TWCR = 0;

  pinMode(sda_pin, INPUT);
  pinMode(scl_pin, INPUT);
  delay(1);

  if (digitalRead(sda_pin) == HIGH && digitalRead(scl_pin) == HIGH) {
    Wire.begin();
    Wire.setWireTimeout(I2C_TIMEOUT_US, true);
    Wire.setClock(I2C_NORMAL_SPEED);
    send_error_response(E_I2C_RECOVER_DONE_1);
    return true;
  }

  send_error_response(E_I2C_RECOVER_STUCK_2);

  pinMode(scl_pin, OUTPUT);
  pinMode(sda_pin, INPUT);

  const uint8_t max_pulses = 20;
  for (uint8_t i = 0; i < max_pulses; i++) {
    if (digitalRead(sda_pin) == HIGH)
      break;
    digitalWrite(scl_pin, LOW);
    delayMicroseconds(100);
    digitalWrite(scl_pin, HIGH);
    delayMicroseconds(100);
  }

  if (digitalRead(sda_pin) == LOW) {
    send_error_response(E_I2C_RECOVER_FAIL_SDA_LOW_1);
    return false;
  }

  pinMode(sda_pin, OUTPUT);
  digitalWrite(sda_pin, LOW);
  delayMicroseconds(5);
  digitalWrite(scl_pin, HIGH);
  delayMicroseconds(100);
  digitalWrite(sda_pin, HIGH);
  delayMicroseconds(20);

  pinMode(sda_pin, INPUT);
  pinMode(scl_pin, INPUT);
  delay(1);

  if (digitalRead(sda_pin) == LOW || digitalRead(scl_pin) == LOW) {
    send_error_response(E_I2C_RECOVER_FAIL_BUS_BUSY_1);
    return false;
  }

  Wire.begin();
  Wire.setWireTimeout(I2C_TIMEOUT_US, true);
  Wire.setClock(I2C_NORMAL_SPEED);
  delay(1);

  send_error_response(E_I2C_RECOVER_DONE_1);
  return true;
}

bool checkAndRecoverI2C() {
  pinMode(SDA, INPUT);
  pinMode(SCL, INPUT);
  delayMicroseconds(10);

  if (digitalRead(SDA) == HIGH && digitalRead(SCL) == HIGH) {
    return false;
  }

  return !recoverI2Cbus();
}

void checkAndReportI2cTimeout() {
  if (Wire.getWireTimeoutFlag()) {
    send_error_response(E_I2C_TIMEOUT_1);
    Wire.clearWireTimeoutFlag();
    recoverI2Cbus();
  }
}

void send_error_response(const char *error_msg PROGMEM) {
  char szBuf[40];
  strncpy_P(szBuf, error_msg, sizeof(szBuf));
  szBuf[sizeof(szBuf) - 1] = '\0';
  const uint8_t cs = calculate_checksum(szBuf);
  Serial.print('<');
  Serial.print(szBuf);
  Serial.print(',');
  Serial.print(cs);
  Serial.println('>');
}
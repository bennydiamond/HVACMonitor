/*
 * ======================================================================
 * PLATFORMIO PROJECT FOR ARDUINO NANO (ATmega168 or ATmega328)
 * Reads Pressure, Geiger, SPS30, SCD30 and SGP40 sensors.
 * ======================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <sps30.h>
#include <SensirionI2cScd30.h>
#include <SensirionI2CSgp40.h>

// ======================================================================
//  CONFIGURATION & DEFINITIONS
// ======================================================================

// --- Firmware & Protocol ---
#define NANO_FIRMWARE_VERSION "1.1.0"
#define CMD_BROADCAST_SENSORS 'S'
#define CMD_GET_VERSION       'V'
#define CMD_GET_HEALTH        'H'
#define RSP_VERSION           'v'
#define CMD_ACK_HEALTH        'A' // Acknowledge Health Status
#define CMD_REBOOT            'R' // Request Nano Reboot
#define RSP_HEALTH            'h'
#define CMD_GET_SPS30_INFO    'P' // Request SPS30 info
#define RSP_SPS30_INFO        'p' // Response with SPS30 info
#define CMD_SGP40_TEST        'G' // Request SGP40 test
#define RSP_SGP40_TEST        'g' // Response with SGP40 test result

// --- Pin Definitions ---
#define PRESSURE_SENSOR_PIN A0
#define GEIGER_PIN          2
#define CT_CLAMP_PIN        A1
#define DEBUG_LED_PIN       13

// --- Timing Constants ---
const int SENSOR_READ_INTERVAL_MS = 2000;
const unsigned long COMMAND_TIMEOUT_MS = 250; // Timeout for incomplete commands
const unsigned long SCD30_INVALIDATE_TIMEOUT_MS = 60000; // Invalidate SCD30 after 60 seconds

// --- Sensor Calculation Constants ---
// Pressure Sensor
#define SHUNT_RESISTOR 150.0f
// CT Clamp Sensor
#define NUM_SAMPLES   1000
#define VOLTS_PER_AMP 0.1f  // For SCT-013-010: 1V at 10A -> 0.1 V/A
// Geiger Counter - A larger buffer provides a more statistically stable CPM reading
#define GEIGER_PULSE_BUFFER_SIZE 36 // Number of pulses to average for CPM calculation

// --- Buffer Sizes ---
#define MAX_COMMAND_LEN 20
#define MAX_RESPONSE_LEN SPS30_MAX_SERIAL_LEN // 32 bytes
#define MAX_SENSOR_DATA_PACKET_LEN 64

// ======================================================================
//  GLOBAL VARIABLES & OBJECTS
// ======================================================================

// --- State Variables ---
unsigned long last_sensor_read_time = 0;
// Geiger counter state using a circular buffer for timestamps
bool first_health_status_sent = true; // Flag to detect first health status request after boot/reboot
volatile unsigned long geiger_pulse_timestamps[GEIGER_PULSE_BUFFER_SIZE] = {0};
volatile uint8_t geiger_pulse_index = 0;
volatile uint8_t geiger_pulses_recorded = 0;

// --- Sensor Data Holders ---
float    current_pressure_pa = 0.0;
int      current_cpm         = 0;
float    current_amps        = 0.0;
float    current_co2         = 0.0;
float    current_temp_c      = 0.0;
float    current_humi        = 0.0;
uint16_t current_voc_raw     = 0;
struct   sps30_measurement current_sps_data = {0}; // Zero-initialize struct
unsigned long last_scd30_update = 0; // Track last update time for SCD30

// --- Sensor Objects ---
SensirionI2cScd30 scd30_sensor;
SensirionI2CSgp40 sgp40_sensor;

// --- Forward Declarations ---
void read_all_sensors();
void send_data_packet();
void process_command(const char* buffer);
int calculate_cpm();
int freeRam();

// ======================================================================
// Interrupt Service Routine for counting Geiger pulses
void on_geiger_pulse() {

  // Store the timestamp of the current pulse in a circular buffer
  geiger_pulse_timestamps[geiger_pulse_index] = millis();
  geiger_pulse_index = (geiger_pulse_index + 1) % GEIGER_PULSE_BUFFER_SIZE;
  if (geiger_pulses_recorded < GEIGER_PULSE_BUFFER_SIZE) {
    geiger_pulses_recorded++;
  }
}

// Helper to get free RAM on AVR
int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

// Function to calculate the 8-bit checksum for a data string
// This is a bit-wise implementation of CRC-8 using polynomial 0x07.
// While a lookup table would be faster, this method is chosen to conserve
// flash memory, which is critical on constrained devices.
uint8_t calculate_checksum(const char* data_str) {
  uint8_t crc = 0x00; // Initial CRC value
  uint8_t polynomial = 0x07; // CRC-8 polynomial
  size_t length = strlen(data_str);

  for (size_t i = 0; i < length; i++) {
      crc ^= data_str[i]; // XOR current byte with CRC

      for (int j = 0; j < 8; j++) {
          if (crc & 0x80) { // If MSB is 1
              crc = (crc << 1) ^ polynomial;
          } else {
              crc <<= 1;
          }
      }
  }
  return crc;
}

// Function to read RMS current from a CT clamp
float readCurrentRMS() {
  long sum_of_squares = 0;
  long sum_of_samples = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    int reading = analogRead(CT_CLAMP_PIN);
    sum_of_samples += reading;
    sum_of_squares += (long)reading * reading;
  }

  // Calculate the mean of the samples to find the DC offset
  float mean_of_samples = (float)sum_of_samples / NUM_SAMPLES;

  // Calculate the mean of the squares of the samples
  float mean_of_squares = (float)sum_of_squares / NUM_SAMPLES;

  // The variance is the square of the RMS value of the AC component.
  // It's calculated as: Mean of the squares - (Square of the mean)
  float variance = mean_of_squares - (mean_of_samples * mean_of_samples);

  // The RMS value in ADC units is the square root of the variance.
  float rms_adc = sqrt(variance);

  // Convert the ADC RMS value to voltage and then to current
  float voltage = rms_adc * (5.0 / 1023.0);
  float current = voltage / VOLTS_PER_AMP;
  return current;
}

// Calculates CPM based on a rolling average of the last N pulses.
// This provides a more stable and responsive reading than batch counting.
int calculate_cpm() {
  uint8_t local_pulses_recorded;
  unsigned long oldest_ts;
  unsigned long newest_ts;

  // Atomically copy the shared volatile data to local variables to ensure
  // the ISR doesn't change them during the calculation.
  noInterrupts();
  local_pulses_recorded = geiger_pulses_recorded;
  if (local_pulses_recorded >= GEIGER_PULSE_BUFFER_SIZE) {
    uint8_t local_pulse_index = geiger_pulse_index;
    // The newest timestamp is one position behind the current index
    uint8_t newest_index = (local_pulse_index == 0) ? (GEIGER_PULSE_BUFFER_SIZE - 1) : (local_pulse_index - 1);
    // The oldest timestamp is at the current index (which will be overwritten next)
    oldest_ts = geiger_pulse_timestamps[local_pulse_index];
    newest_ts = geiger_pulse_timestamps[newest_index];
  }
  interrupts();

  if (local_pulses_recorded < GEIGER_PULSE_BUFFER_SIZE) {
    // Not enough data for a rolling average yet. Return 0 until the buffer is full.
    return 0;
  }

  unsigned long time_span = newest_ts - oldest_ts;

  if (time_span == 0) {
    return 0; // Avoid division by zero if all pulses occurred in the same millisecond.
  }

  // Calculate CPM: (number of intervals / time elapsed in ms) * 60,000 ms/min
  float cpm = (float)(GEIGER_PULSE_BUFFER_SIZE - 1) * 60000.0 / time_span;
  return (int)cpm;
}

// Function to blink the debug LED for error codes
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
    asm volatile (
        "sts urboot_reset_flags, r2\n"
    );
}

void setup() {
    Serial.begin(9600);

    // Populate last_reset_cause for ESP32 interpretation:
    // 1 = Power-On, 2 = External, 3 = Brown-Out, 4 = Watchdog, 0 = Unknown
    if (urboot_reset_flags & (1 << 3))      last_reset_cause = 4; // Watchdog
    else if (urboot_reset_flags & (1 << 2)) last_reset_cause = 3; // Brown-Out
    else if (urboot_reset_flags & (1 << 1)) last_reset_cause = 2; // External
    else if (urboot_reset_flags & (1 << 0)) last_reset_cause = 1; // Power-On
    else                                    last_reset_cause = 0; // Unknown

    pinMode(DEBUG_LED_PIN, OUTPUT);
    // Initial blink to indicate startup
    digitalWrite(DEBUG_LED_PIN, HIGH);
    delay(500);
    digitalWrite(DEBUG_LED_PIN, LOW);
    delay(500);

    // --- Initialize SPS30 (Particulate Matter) using sensirion/sensirion-sps library ---
    sensirion_i2c_init();

    int16_t sps_ret;
    while (sps30_probe() != 0) {
      blink_error_code(3); // 3 blinks: SPS30 probe failure
      delay(1500);
    }

    // Start measurements
    sps_ret = sps30_start_measurement();
    if (sps_ret < 0) {
      while (1) { blink_error_code(4); delay(1500); } // 4 blinks: SPS30 start failure
    }

    // --- Initialize SCD30 (CO2, Temp, Humidity) ---
    scd30_sensor.begin(Wire, SCD30_I2C_ADDR_61);
    uint16_t scd30_error = scd30_sensor.startPeriodicMeasurement(0);
    if (scd30_error) {
      while (1) { blink_error_code(5); delay(1500); } // 5 blinks: SCD30 start failure
    }

    // --- Initialize SGP40 (VOC) ---
    sgp40_sensor.begin(Wire);

    // Initialize Geiger Counter
    pinMode(GEIGER_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), on_geiger_pulse, RISING);
}

// ======================================================================
//  MAIN LOOP
// ======================================================================
void loop() {
  wdt_reset(); // Reset watchdog timer

  // Command parsing state variables
  static char command_buffer[MAX_COMMAND_LEN];
  static uint8_t command_len = 0;
  static bool in_command = false;
  static unsigned long command_start_time = 0;

  unsigned long current_time = millis();

  // --- Periodically read all sensors and send data packet ---
  if (current_time - last_sensor_read_time >= SENSOR_READ_INTERVAL_MS) {
    last_sensor_read_time = current_time;
    read_all_sensors();
    send_data_packet(); // Send data immediately after reading
  }

  // --- Check for command timeout ---
  // If we are in the middle of a command but haven't received the end
  // marker in time, reset the parser state to avoid getting stuck.
  if (in_command && (current_time - command_start_time > COMMAND_TIMEOUT_MS)) {
    in_command = false;
    command_len = 0;
  }

  // --- Process incoming commands from ESP32 ---
  if (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '<') {
      in_command = true;
      command_len = 0;
      command_start_time = current_time;
    } else if (c == '>') {
      if (in_command) {
        command_buffer[command_len] = '\0'; // Null-terminate the string
        process_command(command_buffer);
      }
      in_command = false; // Reset state regardless of whether we were in a command
    } else if (in_command) {
      if (command_len < (sizeof(command_buffer) - 1)) {
        command_buffer[command_len++] = c; // Add character to buffer
      } else {
        // Buffer overflow, reset state to prevent getting stuck
        in_command = false;
        command_len = 0;
      }
    }
  }
}

// ======================================================================
//  SENSOR & DATA FUNCTIONS
// ======================================================================

void read_all_sensors() {
  static bool led_state = LOW;
  led_state = !led_state;
  digitalWrite(DEBUG_LED_PIN, led_state);

  // Read Pressure Sensor
  int raw_adc = analogRead(PRESSURE_SENSOR_PIN);
  float voltage = (raw_adc / 1023.0) * 5.0; 
  float current_ma = (voltage / SHUNT_RESISTOR) * 1000.0;
  current_pressure_pa = (current_ma > 4.0) ? (current_ma - 4.0) * (300.0 / 16.0) : 0.0;

  // Read Current Sensor
  current_amps = readCurrentRMS();

  // Read SPS30 Particulate Matter Sensor
  int16_t ret_sps_read;
  uint16_t data_ready_sps;
  ret_sps_read = sps30_read_data_ready(&data_ready_sps);
  if (ret_sps_read == 0 && data_ready_sps) {
    sps30_read_measurement(&current_sps_data);
  }

  // --- Non-blocking SCD30 (CO2, Temp, Humidity) ---
  uint16_t scd30_data_ready = 0;
  uint16_t scd30_error = scd30_sensor.getDataReady(scd30_data_ready);
  if (scd30_error == 0 && scd30_data_ready) {
      // Data ready, read it
      uint16_t read_error = scd30_sensor.readMeasurementData(current_co2, current_temp_c, current_humi);
      if (read_error == 0) {
          last_scd30_update = millis();
      }
  }
  // Invalidate after 1 minute if not updated
  if (millis() - last_scd30_update > SCD30_INVALIDATE_TIMEOUT_MS) {
      current_co2 = NAN;
      current_temp_c = NAN;
      current_humi = NAN;
  }

  // Read SGP40 (VOC Raw Signal)
  uint16_t rh_ticks = static_cast<uint16_t>(current_humi * 65535.0f / 100.0f);
  uint16_t t_ticks = static_cast<uint16_t>((current_temp_c + 45.0f) * 65535.0f / 175.0f);
  uint16_t sgp40_read_error = sgp40_sensor.measureRawSignal(rh_ticks, t_ticks, current_voc_raw);
  if (sgp40_read_error) {
      current_voc_raw = 0; // Set to 0 on error
  }
}

void send_data_packet() {
  static bool led_state = LOW;
  led_state = !led_state;
  digitalWrite(DEBUG_LED_PIN, led_state);

  // Calculate CPM using the rolling average algorithm
  current_cpm = calculate_cpm();
  
  // Convert floats to scaled integers to avoid linking float-to-string conversion code.
  long p_val = (long)(current_pressure_pa * 10);
  long t_val = (long)(current_temp_c * 10);
  long h_val = (long)(current_humi * 10);
  long co2_val = (long)current_co2;
  long amps_val = (long)(current_amps * 100);
  long pm1_val = (long)(current_sps_data.mc_1p0 * 10);
  long pm25_val = (long)(current_sps_data.mc_2p5 * 10);
  long pm4_val = (long)(current_sps_data.mc_4p0 * 10);
  long pm10_val = (long)(current_sps_data.mc_10p0 * 10);
  
  char data_buffer[MAX_SENSOR_DATA_PACKET_LEN];
  sprintf(data_buffer, "%c%ld,%d,%ld,%ld,%ld,%u,%ld,%ld,%ld,%ld,%ld",
          CMD_BROADCAST_SENSORS, p_val, current_cpm, t_val, h_val, co2_val, current_voc_raw,
          amps_val, pm1_val, pm25_val, pm4_val, pm10_val
          );

  uint8_t checksum = calculate_checksum(data_buffer);

  Serial.print('<');
  Serial.print(data_buffer);
  Serial.print(',');
  Serial.print(checksum);
  Serial.println('>');
}

void process_command(const char* buffer) {
  // Packet format: DATA,CRC
  const char* comma = strrchr(buffer, ',');
  if (!comma) return; // Malformed packet, no checksum found

  int data_len = comma - buffer;
  // Check for valid data length to avoid buffer overflows and empty data parts.
  if (data_len <= 0 || data_len >= MAX_COMMAND_LEN) return;

  // Use a fixed-size buffer instead of a Variable Length Array (VLA)
  // to prevent stack corruption on this memory-constrained device.
  char data_part[MAX_COMMAND_LEN];
  strncpy(data_part, buffer, data_len);
  data_part[data_len] = '\0';

  uint8_t received_checksum = atoi(comma + 1);
  uint8_t calculated_checksum = calculate_checksum(data_part);

  if (received_checksum != calculated_checksum) {
    return; // Checksum mismatch
  }

  char command = data_part[0];
  // const char* payload = data_part + 1; // Payload would be here if needed

  char response_buffer[MAX_RESPONSE_LEN];
  uint8_t checksum;

  switch (command) {
    case CMD_GET_VERSION:
      // Response: v<NANO_FIRMWARE_VERSION>
      sprintf(response_buffer, "%c%s", RSP_VERSION, NANO_FIRMWARE_VERSION);
      checksum = calculate_checksum(response_buffer);
      Serial.print('<'); Serial.print(response_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;

    case CMD_GET_HEALTH:
      // Response: h<first_time_flag>,<free_ram>,<reset_cause>
      // first_time_flag: 0 if this is the first health request since boot, 1 otherwise.
      sprintf(response_buffer, "%c%d,%d,%d", RSP_HEALTH, first_health_status_sent ? 0 : 1, freeRam(), last_reset_cause);
      checksum = calculate_checksum(response_buffer);
      Serial.print('<'); Serial.print(response_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
      break;

    case CMD_ACK_HEALTH:
      // Acknowledge receipt of first health status, set flag to false (not first time anymore)
      first_health_status_sent = false;
      // No response needed for ACK
      break;

    case CMD_REBOOT:
      // Trigger a watchdog reset to reboot the Nano
      wdt_enable(WDTO_15MS); // Enable watchdog with a very short timeout
      while(1); // Loop indefinitely until watchdog resets
      break;

    case CMD_GET_SPS30_INFO: {
        // Query SPS30 info
        uint32_t fan_interval_seconds = 0, status_reg = 0;
        uint8_t fan_interval_days = 0;
        uint8_t version_major, version_minor;
        int16_t ret_fw = sps30_read_firmware_version(&version_major, &version_minor);
        int16_t ret_fan_interval = sps30_get_fan_auto_cleaning_interval(&fan_interval_seconds);
        int16_t ret_fan_days = sps30_get_fan_auto_cleaning_interval_days(&fan_interval_days);
        int16_t ret_status = sps30_read_device_status_register(&status_reg);

        // Compose response: p<ret_fw>,<fw>,<ret_sn>,<sn>,<ret_fan_interval>,<fan_interval>,<ret_fan_days>,<fan_days>,<ret_status>,<status_reg>
        sprintf(response_buffer, "%c%d,%u,%u,%d,%lu,%d,%lu,%d,%lu",
            RSP_SPS30_INFO,
            ret_fw, version_major, version_minor,
            ret_fan_interval, (unsigned long)fan_interval_seconds,
            ret_fan_days, (unsigned long)fan_interval_days,
            ret_status, (unsigned long)status_reg
        );
        checksum = calculate_checksum(response_buffer);
        Serial.print('<'); Serial.print(response_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
        break;
    }
    case CMD_SGP40_TEST: {
        // Perform SGP40 test
        uint16_t test_result = 0;
        uint16_t sgp40_ret = sgp40_sensor.executeSelfTest(test_result);

        // Compose response: g<sgp40_ret>,<test_result>
        sprintf(response_buffer, "%c%d,0x%4X", RSP_SGP40_TEST, sgp40_ret, test_result);
        checksum = calculate_checksum(response_buffer);
        Serial.print('<'); Serial.print(response_buffer); Serial.print(','); Serial.print(checksum); Serial.println('>');
        break;
    }
  }
}
#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <math.h>
#include <esp_system.h>
#include "secrets.h"
#include "LGFX_ESP32_2432S022C.h"
#include "CST820.h"
#include "ui.h"
#include "ui/interfaces/IUIManager.h"
#include "ui/interfaces/IUIUpdater.h"
#include "HomeAssistantManager.h"
#include "OtaManager.h"
#include "WebServerManager.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "RollingAverage.h"
#include "VOCGasIndexAlgorithm.h"
#include "NOxGasIndexAlgorithm.h"
#include "NanoCommands.h"
#include "I2CBridge.h"
#include "GeigerCounter.h"
#include "ZMOD4510Sensor.h"
#include "SensorTask.h"
#include "SerialMutex.h"
#include "UITask.h"
#include "ResetUtils.h"
#include "MainTaskEvents.h"
#include "HVACMonitor.h"
#include "GasConcentrationConverter.h"

// =================== CONSTANTS & GLOBALS ===================
const int WIFI_CHECK_INTERVAL_MS = 10000;
const int DEBUG_UPDATE_INTERVAL_MS = 5000;
const int HEALTH_CHECK_INTERVAL_MS = 5000;  // Reduced from 30000 for faster reboot detection
const int NANO_PACKET_TIMEOUT_MS = 7000;
const int HEALTH_RESPONSE_TIMEOUT_MS = 500;
const int SCD30_INFO_INTERVAL_MS = 20000;
const int STACK_CHECK_INTERVAL_MS = 60000;

enum InitCommand { CMD_NONE, CMD_VERSION, CMD_HEALTH, CMD_SPS30_INFO, CMD_SCD30_INFO };
InitCommand pending_init_commands[] = {CMD_VERSION, CMD_HEALTH, CMD_SPS30_INFO, CMD_SCD30_INFO, CMD_NONE};
bool init_sequence_active = false;
int current_init_command_index = 0;
unsigned long last_init_command_time = 0;
const int INIT_COMMAND_TIMEOUT_MS = 1000;

unsigned long last_wifi_check_time = 0;
unsigned long last_health_check_time = 0;
unsigned long last_debug_update_time = 0;
unsigned long last_sensor_data_time = 0;
unsigned long last_health_request_time = 0;
unsigned long last_stack_check_time = 0;
unsigned long nano_boot_millis = 0;
unsigned long last_scd30_info_time = 0;
bool is_sensor_module_connected = false;
bool health_request_pending = false;
static bool first_health_packet_received = false;
static bool latest_first_time_flag = false;

String serial_buffer = "";
String last_nano_version = "";
uint16_t last_nano_ram = 0;

ConfigManager configManager;
HomeAssistantManager haManager;
OtaManager otaManager;
WebServerManager webServerManager;

VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;
GeigerCounter geigerCounter;
SensorTask sensorTask;
RollingAverage<uint16_t> co2_avg(100);
RollingAverage<uint16_t> voc_avg(100);
RollingAverage<uint16_t> nox_avg(100);
RollingAverage<float> pm1_avg(100);
RollingAverage<float> pm25_avg(100);
RollingAverage<float> pm4_avg(100);
RollingAverage<float> pm10_avg(100);
RollingAverage<uint16_t> o3_avg(100);
RollingAverage<uint16_t> no2_avg(100);
RollingAverage<uint16_t> fast_aqi_avg(100);
RollingAverage<uint16_t> epa_aqi_avg(100);
RollingAverage<float> bmp280_pressure_avg(10); // Average over 10 readings for BMP280
RollingAverage<float> bmp280_temperature_avg(10); // Average over 10 readings for BMP280 temperature
#ifdef AHT20_ENABLED
RollingAverage<float> aht20_temperature_avg(10); // Average over 10 readings for AHT20 temperature
RollingAverage<float> aht20_humidity_avg(10); // Average over 10 readings for AHT20 humidity
#endif
RollingAverage<float> compressor_amps_avg(100); // Average over 100 readings for compressor CT clamp
RollingAverage<float> geothermal_pump_amps_avg(100); // Average over 100 readings for geothermal pump CT clamp

TaskHandle_t mainTaskHandle = nullptr;

// =================== UTILITY FUNCTIONS ===================
uint8_t calculate_checksum(const char* data_str) {
    uint8_t crc = 0x00;
    uint8_t polynomial = 0x07;
    size_t length = strlen(data_str);
    for (size_t i = 0; i < length; i++) {
        crc ^= data_str[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (crc << 1) ^ polynomial : (crc << 1);
        }
    }
    return crc;
}

void send_command_to_nano(char cmd) {
    char data_part[2] = {cmd, '\0'};
    uint8_t checksum = calculate_checksum(data_part);
    logger.debugf("Sending command to Nano: <%c,%d>", cmd, checksum);
    
    SerialMutex& serialMutex = SerialMutex::getInstance();
    if (serialMutex.lock()) {
        Serial.print('<');
        Serial.print(cmd);
        Serial.print(',');
        Serial.print(checksum);
        Serial.print('>');
        serialMutex.unlock();
    }
    
    if (cmd == CMD_REBOOT) {
        logger.info("Reboot command sent. Marking sensor stack as disconnected.");
        is_sensor_module_connected = false;
        UITask::getInstance().update_sensor_status(false);
        haManager.publishSensorConnectionStatus(false);
        haManager.setSensorStackVersionUnavailable();
        haManager.publishSensorStackUptime(0, true);
        UITask::getInstance().clearSensorReadings();
    }
}

void send_command_to_nano_with_param(char cmd, char param) {
    char data_part[3] = {cmd, param, '\0'};
    uint8_t checksum = calculate_checksum(data_part);
    logger.debugf("Sending command to Nano: <%c%c,%d>", cmd, param, checksum);

    SerialMutex& serialMutex = SerialMutex::getInstance();
    if (serialMutex.lock()) {
        Serial.print('<');
        Serial.print(cmd);
        Serial.print(param);
        Serial.print(',');
        Serial.print(checksum);
        Serial.print('>');
        serialMutex.unlock();
    }
}

const char* nano_reset_cause_to_string(uint8_t code) {
    switch (code) {
        case 1: return "Power-On";
        case 2: return "External";
        case 3: return "Brown-Out";
        case 4: return "Watchdog";
        case 0: default: return "Unknown";
    }
}

void update_wifi_status() {
    UITask::getInstance().update_wifi_status(WiFi.status() == WL_CONNECTED, WiFi.RSSI());
    haManager.publishWiFiStatus(WiFi.status() == WL_CONNECTED, WiFi.RSSI(), WIFI_SSID, WiFi.localIP().toString().c_str());
}



// =================== PACKET PROCESSING ===================
void process_packet(String packet) {
    packet.trim();
    if (!packet.startsWith("<") || !packet.endsWith(">")) {
        logger.warningf("Malformed packet (no start/end markers): %s", packet.c_str());
        return;
    }
    packet.remove(0, 1);
    packet.remove(packet.length() - 1);

    int last_comma = packet.lastIndexOf(',');
    if (last_comma == -1) {
        logger.warningf("Malformed packet (no checksum comma): %s", packet.c_str());
        return;
    }

    String data_part = packet.substring(0, last_comma);
    uint8_t received_checksum = packet.substring(last_comma + 1).toInt();
    if (calculate_checksum(data_part.c_str()) != received_checksum) {
        logger.warningf("Checksum mismatch! Rcvd: %s", packet.c_str());
        return;
    }

    // Log all received packets for debugging
    logger.debugf("ESP32: Received packet from Nano: %s", packet.c_str());

    // Special handling for I2C recovery events
    if (data_part.startsWith("E,I2C_RECOVER")) {
        if (data_part.endsWith(",1")) {
            logger.warning("ESP32: Nano reports I2C recovery started");
        } else if (data_part.endsWith(",0")) {
            logger.info("ESP32: Nano reports I2C recovery completed");
        } else {
            logger.warningf("ESP32: Unknown I2C recovery status: %s", data_part.c_str());
        }
    }

    last_sensor_data_time = millis();

    if (!is_sensor_module_connected) {
        is_sensor_module_connected = true;
        nano_boot_millis = millis();
        logger.info("Sensor module connection established.");
        init_sequence_active = true;
        current_init_command_index = 0;
    }

    UITask::getInstance().update_sensor_status(true);
    haManager.publishSensorConnectionStatus(true);

    char cmd = data_part.charAt(0);
    String payload = data_part.substring(1);

    switch (cmd) {
        case CMD_BROADCAST_SENSORS: {
            char data_cstr[payload.length() + 1];
            strcpy(data_cstr, payload.c_str());
            char* token = strtok(data_cstr, ","); if (!token) return; float p = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; uint16_t pulse_count = atoi(token);
            token = strtok(NULL, ","); if (!token) return; float t = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float h = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float co2 = atof(token);
            token = strtok(NULL, ","); if (!token) return; uint16_t voc_raw = atol(token);
            token = strtok(NULL, ","); if (!token) return; uint16_t nox_raw = atol(token);
            token = strtok(NULL, ","); if (!token) return; float amps = atof(token) / 100.0f;
            token = strtok(NULL, ","); if (!token) return; float pm1 = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float pm25 = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float pm4 = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float pm10 = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float compressor_amps = atof(token) / 100.0f;
            token = strtok(NULL, ","); if (!token) return; float geothermal_pump_amps = atof(token) / 100.0f;
            token = strtok(NULL, ","); if (!token) return; bool liquid_level_sensor_state = (atoi(token) == 0); // GPIO at 0 == sensor triggered
            
            // Add the pulse count to the geiger counter object
            geigerCounter.addSample(pulse_count);
            int c = geigerCounter.getCPM();
            
            int32_t voc_index = voc_algorithm.process(voc_raw);
            int32_t nox_index = nox_algorithm.process(nox_raw);
            co2_avg.add(co2);
            voc_avg.add(voc_index);
            nox_avg.add(nox_index);
            pm1_avg.add(pm1);
            pm25_avg.add(pm25);
            pm4_avg.add(pm4);
            pm10_avg.add(pm10);
            compressor_amps_avg.add(compressor_amps);
            geothermal_pump_amps_avg.add(geothermal_pump_amps);

            FanStatus fan_status;
            if (amps <= configManager.getFanOffCurrentThreshold()) {
                fan_status = FAN_STATUS_OFF;
            } else if (amps < configManager.getFanOnCurrentThreshold() || amps > configManager.getFanHighCurrentThreshold()) {
                fan_status = FAN_STATUS_ALERT;
            } else {
                fan_status = FAN_STATUS_NORMAL;
            }
            bool is_pressure_high = (p > configManager.getHighPressureThreshold());
            UITask::getInstance().update_high_pressure_status(is_pressure_high);
            UITask::getInstance().update_pressure(p);
            
            geigerCounter.checkAndLogHighRadiation();
            float usv_h = geigerCounter.getDoseRate();
            UITask::getInstance().update_geiger_reading(c, usv_h);

            UITask::getInstance().update_temp_humi(t, h);
            UITask::getInstance().update_fan_current(amps, fan_status);
            UITask::getInstance().update_co2(co2_avg.getAverage());
            UITask::getInstance().update_voc(voc_avg.getAverage());
            UITask::getInstance().update_pm_values(pm1_avg.getAverage(), pm25_avg.getAverage(), pm4_avg.getAverage(), pm10_avg.getAverage());

            haManager.publishHighPressureStatus(is_pressure_high);
            haManager.publishFanStatus(fan_status != FAN_STATUS_OFF);
            haManager.publishSensorData(p, c, t, h, 
                co2_avg.getAverage(), 
                voc_avg.getAverage(), nox_avg.getAverage(), 
                amps,
                pm1_avg.getAverage(), pm25_avg.getAverage(), pm4_avg.getAverage(), pm10_avg.getAverage(),
                compressor_amps_avg.getAverage(), geothermal_pump_amps_avg.getAverage(), liquid_level_sensor_state);

            sensorTask.setEnvironmentalData(t, h);
            break;
        }
        case RSP_VERSION: {
            haManager.publishSensorStackVersion(payload.c_str());
            last_nano_version = payload;
            UITask::getInstance().update_fw_version(last_nano_version.c_str());
            if (init_sequence_active && pending_init_commands[current_init_command_index] == CMD_VERSION) {
                current_init_command_index++;
            }
            break;
        }
        case RSP_HEALTH: {
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());
            
            char* token = strtok(payload_cstr, ","); 
            if (!token) return; 
            int first_time_flag = atoi(token);
            
            token = strtok(NULL, ","); 
            if (!token) return; 
            uint16_t nano_free_ram = atoi(token);

            token = strtok(NULL, ","); 
            uint8_t nano_reset_cause = 0; 
            if (token) nano_reset_cause = atoi(token);
            const char* reset_cause_str = nano_reset_cause_to_string(nano_reset_cause);

            logger.debugf("Nano Health: FirstTimeFlag=%d, FreeRAM=%d bytes, ResetCause=%s", first_time_flag, nano_free_ram, reset_cause_str);
            if (first_time_flag == 0 || !first_health_packet_received) {
                logger.infof("Sensor Stack Health: Flag=%d, FreeRAM=%d bytes, ResetCause=%s", first_time_flag, nano_free_ram, reset_cause_str);
                first_health_packet_received = true;
            }
            if (nano_free_ram < 64) {
                logger.warningf("Nano free RAM critically low: %d bytes", nano_free_ram);
            }
            haManager.publishSensorStackFreeRam(nano_free_ram);
            haManager.publishNanoResetCause(reset_cause_str);
            last_nano_ram = nano_free_ram;

            // Store first_time_flag for ZMOD4510 manager processing in main loop
            latest_first_time_flag = first_time_flag != 0;

            if (first_time_flag == 0) {
                send_command_to_nano(CMD_ACK_HEALTH);
                nano_boot_millis = millis();
                logger.warning("Nano reported first boot (or reboot). Uptime counter reset.");
                haManager.resetSensorStackUptimePublishTime();
            }
            if (health_request_pending) {
                health_request_pending = false;
            }
            if (init_sequence_active && pending_init_commands[current_init_command_index] == CMD_HEALTH) {
                current_init_command_index++;
            }
            break;
        }
        case RSP_SPS30_INFO: {
            // Format: p<ret_fw>,<fw_version_major>,<fw_version_minor>,<ret_fan_interval>,<fan_interval>,<ret_fan_days>,<fan_days>,<ret_status>,<status_reg>
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());
            char* token = strtok(payload_cstr, ","); if (!token) return; int ret_fw = atoi(token);
            token = strtok(NULL, ","); if (!token) return; int fw_version_major = atoi(token);
            token = strtok(NULL, ","); if (!token) return; int fw_version_minor = atoi(token);
            token = strtok(NULL, ","); if (!token) return; int ret_fan_interval = atoi(token);
            token = strtok(NULL, ","); if (!token) return; unsigned long fan_interval = strtoul(token, nullptr, 10);
            token = strtok(NULL, ","); if (!token) return; int ret_fan_days = atoi(token);
            token = strtok(NULL, ","); if (!token) return; unsigned long fan_days = strtoul(token, nullptr, 10);
            token = strtok(NULL, ","); if (!token) return; int ret_status = atoi(token);
            token = strtok(NULL, ","); if (!token) return; unsigned long status_reg = strtoul(token, nullptr, 10);

            logger.debugf("SPS30 Info: FwVersion(ret=%d): %u.%u, FanInterval(ret=%d): %lu, FanDays(ret=%d): %lu, StatusReg(ret=%d): 0x%lX",
                ret_fw, fw_version_major, fw_version_minor,
                ret_fan_interval, fan_interval,
                ret_fan_days, fan_days,
                ret_status, status_reg
            );
            UITask::getInstance().update_sps30_fan_interval(fan_interval);
            UITask::getInstance().update_sps30_fan_days(fan_days);
            if (init_sequence_active && pending_init_commands[current_init_command_index] == CMD_SPS30_INFO) {
                current_init_command_index++;
            }
            break;
        }
        case RSP_SPS30_CLEAN: {
            // Format: c<ret_status>    
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());

            char* token = strtok(payload_cstr, ","); 
            if (!token) return; 
            int ret_status = atoi(token);

            logger.debugf("SPS30 Manual Fan Cleaning: Status=%d", ret_status);
        }
        case RSP_SGP41_TEST: {
            // Format: g<ret_status>,<raw_value>
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());

            char* token = strtok(payload_cstr, ","); 
            if (!token) return; 
            int ret_status = atoi(token);

            token = strtok(NULL, ","); 
            if (!token) return; 
            uint16_t raw_value = strtoul(token, nullptr, 16);

            logger.debugf("SGP41 Test Result: Status=%d, RawValue=0x%04X", ret_status, raw_value);
            UITask::getInstance().update_sgp41_test_status(ret_status);
            UITask::getInstance().update_sgp41_test_value(raw_value);
            break;
        }
        case RSP_SCD30_INFO: {
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());

            auto parse_hex_u16 = [](const char* token) -> uint16_t {
                return static_cast<uint16_t>(strtol(token, nullptr, 16));
            };

            auto parse_hex_s16 = [](const char* token) -> int16_t {
                return static_cast<int16_t>(static_cast<uint16_t>(strtol(token, nullptr, 16)));
            };

            char* token;

            token = strtok(payload_cstr, ","); int16_t ret_interval = parse_hex_s16(token);
            token = strtok(NULL, ",");        uint16_t measurement_interval = parse_hex_u16(token);
            token = strtok(NULL, ",");        int16_t ret_auto_cal = parse_hex_s16(token);
            token = strtok(NULL, ",");        uint16_t auto_calibration = parse_hex_u16(token);
            token = strtok(NULL, ",");        int16_t ret_forced_cal = parse_hex_s16(token);
            token = strtok(NULL, ",");        uint16_t forced_recalibration_value = parse_hex_u16(token);
            token = strtok(NULL, ",");        int16_t ret_temp_offset = parse_hex_s16(token);
            token = strtok(NULL, ",");        uint16_t temperature_offset = parse_hex_u16(token);
            token = strtok(NULL, ",");        int16_t ret_altitude = parse_hex_s16(token);
            token = strtok(NULL, ",");        uint16_t altitude_compensation = parse_hex_u16(token);
            token = strtok(NULL, ",");        int16_t ret_firmware = parse_hex_s16(token);
            token = strtok(NULL, ",");        uint8_t fw_major = parse_hex_u16(token);
            token = strtok(NULL, ",");        uint8_t fw_minor = parse_hex_u16(token);

            logger.debugf(
                "SCD30 Info: "
                "MeasurementInterval(ret=%d): %u, "
                "AutoCalibration(ret=%d): %s, "
                "ForcedRecalibrationValue(ret=%d): %u, "
                "TemperatureOffset(ret=%d): %u, "
                "AltitudeCompensation(ret=%d): %u, "
                "FwVersion(ret=%d): %u.%u",
                ret_interval, measurement_interval,
                ret_auto_cal, auto_calibration ? "ON" : "OFF",
                ret_forced_cal, forced_recalibration_value,
                ret_temp_offset, temperature_offset,
                ret_altitude, altitude_compensation,
                ret_firmware, fw_major, fw_minor
            );
            // Update the AutoCalibration switch state and Force Calibration value in Home Assistant and UI
            haManager.updateScd30AutoCalState(auto_calibration != 0);
            haManager.updateScd30ForceCalValue(forced_recalibration_value);
            UITask::getInstance().update_scd30_autocal(auto_calibration != 0);
            UITask::getInstance().update_scd30_forcecal(forced_recalibration_value);

            if (init_sequence_active && pending_init_commands[current_init_command_index] == CMD_SCD30_INFO) {
                current_init_command_index++;
            }
            break;
        }
        case RSP_SCD30_AUTOCAL: {
            // Format: t<set_result>,<read_result>,<actual_state>
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());

            char* token = strtok(payload_cstr, ","); 
            if (!token) return; 
            int set_result = strtol(token, nullptr, 16);

            token = strtok(NULL, ","); 
            if (!token) return; 
            int read_result = strtol(token, nullptr, 16);

            token = strtok(NULL, ","); 
            if (!token) return; 
            uint16_t actual_state = strtol(token, nullptr, 16);
            
            logger.debugf("SCD30 AutoCalibration Set: SetResult=%d, ReadResult=%d, ActualState=%s", 
                set_result, read_result, actual_state ? "ON" : "OFF");
            
            // Update the switch state based on the actual readback value
            haManager.updateScd30AutoCalState(actual_state != 0);
            break;
        }
        case RSP_SCD30_FORCECAL: {
            // Format: f<set_result>,<read_result>,<actual_value>
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());

            char* token = strtok(payload_cstr, ","); 
            if (!token) return; 
            int set_result = strtol(token, nullptr, 16);

            token = strtok(NULL, ","); 
            if (!token) return; 
            int read_result = strtol(token, nullptr, 16);

            token = strtok(NULL, ","); 
            if (!token) return; 
            uint16_t actual_value = strtol(token, nullptr, 16);
            
            logger.debugf("SCD30 Force Calibration Set: SetResult=%d, ReadResult=%d, ActualValue=%u ppm", 
                set_result, read_result, actual_value);
            
            // Update the number value based on the actual readback value
            haManager.updateScd30ForceCalValue(actual_value);
            break;
        }
        case RSP_I2C_READ: {
            // Format: i<status_byte>,<num_bytes>[,<byte1>,<byte2>,...]
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());
            
            // Parse status byte
            char* token = strtok(payload_cstr, ","); 
            if (!token) return; 
            uint8_t status = strtol(token, nullptr, 16);
            
            // Parse number of bytes
            token = strtok(NULL, ","); 
            if (!token) return; 
            uint8_t num_bytes = strtol(token, nullptr, 16);
            
            // Parse data bytes
            uint8_t data[32]; // Max 32 bytes
            uint8_t bytes_read = 0;
            
            while ((token = strtok(NULL, ",")) != NULL && bytes_read < num_bytes) {
                data[bytes_read++] = strtol(token, nullptr, 16);
            }
            
            if (bytes_read != num_bytes) {
                logger.warningf("I2C read: Expected %d bytes, got %d", num_bytes, bytes_read);
            }
            
            if (status != I2C_ERROR_NONE) {
                logger.warningf("I2C read error: 0x%02X", status);
            } else {
                // Log the received data bytes
                String hexData;
                for (int i = 0; i < bytes_read; i++) {
                    if (i > 0) hexData += " ";
                    char hex[4];
                    sprintf(hex, "%02X", data[i]);
                    hexData += hex;
                }
            }
            
            // Update the I2CBridge with the response and send to queue
            I2CBridge::getInstance().processReadResponse(status, data, bytes_read);
            break;
        }
        case RSP_I2C_WRITE: {
            // Format: w<status_byte>
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());
            
            // Parse status byte
            char* token = strtok(payload_cstr, ","); 
            if (!token) return; 
            uint8_t status = strtol(token, nullptr, 16);
            
            if (status != I2C_ERROR_NONE) {
                logger.warningf("I2C write error: 0x%02X", status);
            }
            
            // Update the I2CBridge with the response and send to queue
            I2CBridge::getInstance().processWriteResponse(status);
            break;
        }
        default:
            logger.warningf("Unknown command from Nano: %c", cmd);
            break;
    }

    if (init_sequence_active && pending_init_commands[current_init_command_index] == CMD_NONE) {
        init_sequence_active = false;
    }
}

// =================== SETUP & LOOP ===================
void setup_wifi() {
    logger.info("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long start_time = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start_time < 15000) { delay(500); }
    if (WiFi.status() == WL_CONNECTED) {
        logger.infof("WiFi connected. IP address: %s", WiFi.localIP().toString().c_str());
    } else {
        logger.error("WiFi connection failed.");
    }
    update_wifi_status();
    UITask::getInstance().update_ssid(WIFI_SSID);
    UITask::getInstance().update_ip(WiFi.localIP().toString().c_str());
    UITask::getInstance().update_mac(WiFi.macAddress().c_str());
}

void setup() {
    MainTaskEventNotifier::getInstance().setMainTaskHandle(xTaskGetCurrentTaskHandle());
    logger.info("--- System Booting ---");
    Serial.begin(19200);
    //Serial.begin(115200);
    configManager.init();
    logger.init(haManager.getMqtt());
    logger.setLogLevel(configManager.getLogLevel());
    delay(500);
    
    SerialMutex::getInstance().init();
    
    const char* reset_reason = get_reset_reason_string();
    logger.infof("Firmware Version: %s", HVACMONITOR_FIRMWARE_VERSION);
    logger.infof("Reset Reason: %s", reset_reason);
    
    // Start UI task (UI objects are now owned by UITask)
    UITask::getInstance().start(&configManager);
    
    setup_wifi();
    haManager.init(&configManager, HVACMONITOR_FIRMWARE_VERSION); // UI pointers are now managed by UITask
    webServerManager.init(HVACMONITOR_FIRMWARE_VERSION);
    otaManager.init();
    
    I2CBridge::begin();
    
    sensorTask.init();
    vTaskPrioritySet(NULL, tskIDLE_PRIORITY + 3); // above ZMOD4510 task priority
}


void loop() {
    bool serial_command_sent_this_loop = false;
    static bool valid_packet_start_received = false;
    
    haManager.loop();
    otaManager.handle();
    webServerManager.handle();
    logger.loop();

    while (Serial.available() > 0) {
        char incoming_char = Serial.read();
        
        // Skip carriage return and line feed characters
        if (incoming_char == '\r' || incoming_char == '\n') {
            continue;
        }
        
        if (valid_packet_start_received == false && incoming_char == '<') {
            serial_buffer = "";
            valid_packet_start_received = true;
        } else if (valid_packet_start_received && incoming_char == '>') {
            if (serial_buffer.length() > 0) {
                process_packet("<" + serial_buffer + ">");
            }
            serial_buffer = "";
            valid_packet_start_received = false;
            break;
        } else if (valid_packet_start_received && serial_buffer.length() < 255) {
            serial_buffer += incoming_char;
        } else {
            logger.warningf("Invalid character received: %c", incoming_char);
            serial_buffer = "";
            valid_packet_start_received = false; // Reset state on invalid char
        }
    }

    if (millis() - last_wifi_check_time > WIFI_CHECK_INTERVAL_MS) {
        last_wifi_check_time = millis();
        update_wifi_status();
    }

    if (!serial_command_sent_this_loop && is_sensor_module_connected && (millis() - last_health_check_time > HEALTH_CHECK_INTERVAL_MS)) {
        last_health_check_time = millis();
        send_command_to_nano(CMD_GET_HEALTH);
        last_health_request_time = millis();
        health_request_pending = true;
    }

    if (health_request_pending && (millis() - last_health_request_time > HEALTH_RESPONSE_TIMEOUT_MS)) {
        logger.warning("No response to CMD_GET_HEALTH from Sensor Stack within 500ms.");
        health_request_pending = false;
    }

    if (millis() - last_debug_update_time > DEBUG_UPDATE_INTERVAL_MS) {
        last_debug_update_time = millis();
        uint32_t seconds_since_packet = (millis() - last_sensor_data_time) / 1000;
        uint32_t nano_current_uptime_seconds = is_sensor_module_connected ? (millis() - nano_boot_millis) / 1000 : 0;
        UITask::getInstance().update_last_packet_time(seconds_since_packet);
        haManager.publishSensorStackUptime(nano_current_uptime_seconds);
        haManager.publishEsp32FreeRam(ESP.getFreeHeap());
        haManager.publishEsp32Uptime(millis() / 1000);
        UITask::getInstance().update_runtime_uptime(millis() / 1000);
        UITask::getInstance().update_runtime_free_heap(ESP.getFreeHeap());
        
        if (is_sensor_module_connected) {
            UITask::getInstance().update_sensorstack_uptime(nano_current_uptime_seconds);
            UITask::getInstance().update_sensorstack_ram(last_nano_ram);
        } else {
            UITask::getInstance().update_sensorstack_uptime(0);
            UITask::getInstance().update_sensorstack_ram(0);
        }

        bool wifi_connected = (WiFi.status() == WL_CONNECTED);
        String ip = wifi_connected ? WiFi.localIP().toString() : "N/A";
        int8_t rssi = wifi_connected ? WiFi.RSSI() : 0;
        String ssid = wifi_connected ? WIFI_SSID : "N/A";
        bool ha_conn = haManager.isMqttConnected();
        
        UITask::getInstance().update_network_rssi(rssi);
        UITask::getInstance().update_ssid(ssid.c_str());
        UITask::getInstance().update_ip(ip.c_str());
        UITask::getInstance().update_network_ha_conn(ha_conn);
    }

    if (millis() - last_sensor_data_time > NANO_PACKET_TIMEOUT_MS) {
        if (is_sensor_module_connected) {
            is_sensor_module_connected = false;
            logger.warning("Sensor data timeout. Marking as disconnected.");
            UITask::getInstance().update_sensor_status(false);
            haManager.publishSensorConnectionStatus(false);
            haManager.setSensorStackVersionUnavailable();
            haManager.publishSensorStackUptime(0, true);
            UITask::getInstance().clearSensorReadings();
            
            // Nano disconnected status will be handled in main loop
        }
    }

    if (init_sequence_active && is_sensor_module_connected) {
        if (pending_init_commands[current_init_command_index] == CMD_NONE) {
            init_sequence_active = false;
        } else if (millis() - last_init_command_time > INIT_COMMAND_TIMEOUT_MS) {
            char cmd_to_send = 0;
            switch (pending_init_commands[current_init_command_index]) {
                case CMD_VERSION: cmd_to_send = CMD_GET_VERSION; break;
                case CMD_HEALTH: cmd_to_send = CMD_GET_HEALTH; break;
                case CMD_SPS30_INFO: cmd_to_send = CMD_GET_SPS30_INFO; break;
                case CMD_SCD30_INFO: cmd_to_send = CMD_GET_SCD30_INFO; break;
            }
            if (cmd_to_send) {
                send_command_to_nano(cmd_to_send);
                last_init_command_time = millis();
            }
        }
    }

    if (!serial_command_sent_this_loop && is_sensor_module_connected && !init_sequence_active && (millis() - last_scd30_info_time > SCD30_INFO_INTERVAL_MS)) {
        last_scd30_info_time = millis();
        send_command_to_nano(CMD_GET_SCD30_INFO);
    }

    // Update connection status for sensor task
    sensorTask.updateConnectionStatus(is_sensor_module_connected, latest_first_time_flag);
    
    // Get ZMOD4510 sensor data
    SensorTask::ZMOD4510Values zmod_values;
    const bool freshValues = sensorTask.getZMOD4510Data(zmod_values);
    
    if(freshValues)
    {
        o3_avg.add(zmod_values.o3_conc_ppb);
        no2_avg.add(zmod_values.no2_conc_ppb);
        fast_aqi_avg.add(zmod_values.fast_aqi);
        epa_aqi_avg.add(zmod_values.epa_aqi);
        
        // Get current temperature and pressure for conversion
        float current_temperature = bmp280_temperature_avg.getAverage();
        float current_pressure = bmp280_pressure_avg.getAverage();
        
        // Convert ppb values to µg/m³
        float o3_ug_per_m3 = GasConcentrationConverter::convertO3PpbToUgPerM3(
            o3_avg.getAverage(), current_temperature, current_pressure);
        float no2_ug_per_m3 = GasConcentrationConverter::convertNO2PpbToUgPerM3(
            no2_avg.getAverage(), current_temperature, current_pressure);
        
        // Debug logging for conversion
        logger.debugf("Gas Conversion: O3=%u ppb -> %.2f µg/m³, NO2=%u ppb -> %.2f µg/m³ (T=%.1f°C, P=%.1f Pa)", 
            o3_avg.getAverage(), o3_ug_per_m3, no2_avg.getAverage(), no2_ug_per_m3, 
            current_temperature, current_pressure);
        
        // Publish converted values to Home Assistant
        haManager.publish_O3_NOx_Values(
            o3_ug_per_m3, 
            no2_ug_per_m3, 
            fast_aqi_avg.getAverage(), 
            epa_aqi_avg.getAverage()
        );
    }
    
    // Get BMP280 sensor data
    SensorTask::BMP280Values bmp280_values;
    const bool freshBMP280Values = sensorTask.getBMP280Data(bmp280_values);
    
    if(freshBMP280Values)
    {
        // Add to rolling average
        bmp280_pressure_avg.add(bmp280_values.pressure_pa);
        bmp280_temperature_avg.add(bmp280_values.temperature_degc);
        
        // Publish averaged value to Home Assistant
        float averaged_pressure = bmp280_pressure_avg.getAverage();
        float averaged_temperature = bmp280_temperature_avg.getAverage();
        haManager.publishBMP280Data(averaged_pressure, averaged_temperature);
        
        logger.debugf("BMP280: P=%.1f Pa (avg: %.1f Pa), T=%.1f°C (avg: %.1f°C)", bmp280_values.pressure_pa, averaged_pressure, bmp280_values.temperature_degc, averaged_temperature);
    }
    
    // Get AHT20 sensor data
#ifdef AHT20_ENABLED
    SensorTask::AHT20Values aht20_values;
    const bool freshAHT20Values = sensorTask.getAHT20Data(aht20_values);

    if(freshAHT20Values)
    {
        // Publish AHT20 data to Home Assistant
        aht20_temperature_avg.add(aht20_values.temperature_degc);
        aht20_humidity_avg.add(aht20_values.humidity_pct);
        haManager.publishAHT20Data(aht20_temperature_avg.getAverage(), aht20_humidity_avg.getAverage());

        logger.debugf("AHT20: T=%.1f°C (avg: %.1f°C), H=%.1f%% (avg: %.1f%%)", aht20_values.temperature_degc, aht20_temperature_avg.getAverage(), aht20_values.humidity_pct, aht20_humidity_avg.getAverage());
    }
#endif
    
    if (millis() - last_stack_check_time > STACK_CHECK_INTERVAL_MS) {
        last_stack_check_time = millis();
        // Pass NULL to get the stack high water mark for the current task (the loop)
        // On ESP32, this function returns the size in bytes.
        UBaseType_t remaining_stack = uxTaskGetStackHighWaterMark(NULL);
        logger.debugf("Main loop task remaining stack: %u bytes", remaining_stack);
    }

    if (!serial_command_sent_this_loop) {
        MainTaskEventNotifier::getInstance().processOneEvent([&](MainTaskEventNotifier::EventBits event) {
            switch (event) {
                case MainTaskEventNotifier::EVT_SCD30_AUTOCAL_ON:
                    send_command_to_nano_with_param(CMD_SET_SCD30_AUTOCAL, '1');
                    serial_command_sent_this_loop = true;
                    break;
                case MainTaskEventNotifier::EVT_SCD30_AUTOCAL_OFF:
                    send_command_to_nano_with_param(CMD_SET_SCD30_AUTOCAL, '0');
                    serial_command_sent_this_loop = true;
                    break;
                case MainTaskEventNotifier::EVT_SPS30_CLEAN:
                    send_command_to_nano(CMD_SPS30_CLEAN);
                    serial_command_sent_this_loop = true;
                    break;
                case MainTaskEventNotifier::EVT_SGP41_TEST:
                    send_command_to_nano(CMD_SGP41_TEST);
                    serial_command_sent_this_loop = true;
                    break;
                default:
                    break;
            }
        });
    }
}
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
#include "NanoCommands.h"

// =================== CONSTANTS & GLOBALS ===================
const char* FIRMWARE_VERSION = "1.0.0";
const int WIFI_CHECK_INTERVAL_MS = 10000;
const int DEBUG_UPDATE_INTERVAL_MS = 5000;
const int HEALTH_CHECK_INTERVAL_MS = 30000;
const int NANO_PACKET_TIMEOUT_MS = 7000;
const int HEALTH_RESPONSE_TIMEOUT_MS = 500;
const int SCD30_INFO_INTERVAL_MS = 20000; // Request SCD30 info every 20 seconds
const unsigned long ROLLING_AVERAGE_WINDOW_MS = 30 * 60 * 1000;
const size_t MAX_AVG_SAMPLES = 1024;

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
unsigned long nano_boot_millis = 0;
unsigned long last_scd30_info_time = 0;
bool is_sensor_module_connected = false;
bool health_request_pending = false;
static bool first_health_packet_received = false;

String serial_buffer = "";
String last_nano_version = "";
uint16_t last_nano_ram = 0;

static LGFX tft;
CST820 touch(21, 22, -1, -1);
ConfigManager configManager;
HomeAssistantManager haManager;
OtaManager otaManager;
WebServerManager webServerManager;

VOCGasIndexAlgorithm voc_algorithm;
RollingAverage co2_avg(ROLLING_AVERAGE_WINDOW_MS, MAX_AVG_SAMPLES);
RollingAverage voc_avg(ROLLING_AVERAGE_WINDOW_MS, MAX_AVG_SAMPLES);
RollingAverage pm1_avg(ROLLING_AVERAGE_WINDOW_MS, MAX_AVG_SAMPLES);
RollingAverage pm25_avg(ROLLING_AVERAGE_WINDOW_MS, MAX_AVG_SAMPLES);
RollingAverage pm4_avg(ROLLING_AVERAGE_WINDOW_MS, MAX_AVG_SAMPLES);
RollingAverage pm10_avg(ROLLING_AVERAGE_WINDOW_MS, MAX_AVG_SAMPLES);

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
    Serial.print('<'); Serial.print(cmd); Serial.print(','); Serial.print(checksum); Serial.print('>');
    if (cmd == CMD_REBOOT) {
        logger.info("Reboot command sent. Marking sensor stack as disconnected.");
        is_sensor_module_connected = false;
        IUIUpdater& uiUpdater = UI::getInstance();
        uiUpdater.update_sensor_status(false);
        haManager.publishSensorConnectionStatus(false);
        haManager.setSensorStackVersionUnavailable();
        haManager.publishSensorStackUptime(0, true);
        uiUpdater.clearSensorReadings();
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

String get_reset_reason_string() {
    switch (esp_reset_reason()) {
        case ESP_RST_UNKNOWN:    return "Unknown";
        case ESP_RST_POWERON:    return "Power On";
        case ESP_RST_EXT:        return "External";
        case ESP_RST_SW:         return "Software";
        case ESP_RST_PANIC:      return "Panic";
        case ESP_RST_INT_WDT:    return "Interrupt WDT";
        case ESP_RST_TASK_WDT:   return "Task WDT";
        case ESP_RST_WDT:        return "Other WDT";
        case ESP_RST_DEEPSLEEP:  return "Deep Sleep";
        case ESP_RST_BROWNOUT:   return "Brownout";
        case ESP_RST_SDIO:       return "SDIO";
        default:                 return "No reason";
    }
}

void update_wifi_status() {
    IUIUpdater& uiUpdater = UI::getInstance();
    bool is_connected = (WiFi.status() == WL_CONNECTED);
    long rssi = is_connected ? WiFi.RSSI() : 0;
    uiUpdater.update_wifi_status(is_connected, rssi);
    haManager.publishWiFiStatus(is_connected, rssi, WIFI_SSID, WiFi.localIP().toString().c_str());
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

    last_sensor_data_time = millis();

    if (!is_sensor_module_connected) {
        is_sensor_module_connected = true;
        nano_boot_millis = millis();
        logger.info("Sensor module connection established.");
        init_sequence_active = true;
        current_init_command_index = 0;
    }

    IUIUpdater& uiUpdater = UI::getInstance();
    uiUpdater.update_sensor_status(true);
    haManager.publishSensorConnectionStatus(true);

    char cmd = data_part.charAt(0);
    String payload = data_part.substring(1);

    switch (cmd) {
        case CMD_BROADCAST_SENSORS: {
            char data_cstr[payload.length() + 1];
            strcpy(data_cstr, payload.c_str());
            char* token = strtok(data_cstr, ","); if (!token) return; float p = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; int c = atoi(token);
            token = strtok(NULL, ","); if (!token) return; float t = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float h = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float co2 = atof(token);
            token = strtok(NULL, ","); if (!token) return; uint16_t voc_raw = atol(token);
            token = strtok(NULL, ","); if (!token) return; float amps = atof(token) / 100.0f;
            token = strtok(NULL, ","); if (!token) return; float pm1 = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float pm25 = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float pm4 = atof(token) / 10.0f;
            token = strtok(NULL, ","); if (!token) return; float pm10 = atof(token) / 10.0f;

            int32_t voc_index = voc_algorithm.process(voc_raw);
            co2_avg.add(co2);
            voc_avg.add((float)voc_index);
            pm1_avg.add(pm1);
            pm25_avg.add(pm25);
            pm4_avg.add(pm4);
            pm10_avg.add(pm10);

            FanStatus fan_status;
            if (amps <= configManager.getFanOffCurrentThreshold()) {
                fan_status = FAN_STATUS_OFF;
            } else if (amps < configManager.getFanOnCurrentThreshold() || amps > configManager.getFanHighCurrentThreshold()) {
                fan_status = FAN_STATUS_ALERT;
            } else {
                fan_status = FAN_STATUS_NORMAL;
            }
            bool is_pressure_high = (p > configManager.getHighPressureThreshold());
            uiUpdater.update_high_pressure_status(is_pressure_high);
            uiUpdater.update_fan_status(fan_status);
            uiUpdater.update_pressure(p);
            float usv_h = c * 0.0057;
            uiUpdater.update_geiger_reading(c, usv_h);
            uiUpdater.update_temp_humi(t, h);
            uiUpdater.update_fan_current(amps, fan_status);
            uiUpdater.update_co2(co2_avg.getAverage());
            uiUpdater.update_voc((int32_t)voc_avg.getAverage());
            uiUpdater.update_pm_values(pm1_avg.getAverage(), pm25_avg.getAverage(), pm4_avg.getAverage(), pm10_avg.getAverage());

            haManager.publishHighPressureStatus(is_pressure_high);
            haManager.publishFanStatus(fan_status != FAN_STATUS_OFF);
            haManager.publishSensorData(p, c, t, h, co2_avg.getAverage(), (int32_t)voc_avg.getAverage(), amps, pm1_avg.getAverage(), pm25_avg.getAverage(), pm4_avg.getAverage(), pm10_avg.getAverage());
            break;
        }
        case RSP_VERSION: {
            haManager.publishSensorStackVersion(payload.c_str());
            last_nano_version = payload;
            if (init_sequence_active && pending_init_commands[current_init_command_index] == CMD_VERSION) {
                current_init_command_index++;
            }
            break;
        }
        case RSP_HEALTH: {
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());
            char* token = strtok(payload_cstr, ","); if (!token) return; int first_time_flag = atoi(token);
            token = strtok(NULL, ","); if (!token) return; uint16_t nano_free_ram = atoi(token);
            token = strtok(NULL, ","); uint8_t nano_reset_cause = 0; if (token) nano_reset_cause = atoi(token);
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
            uiUpdater.update_sps30_info(fan_interval, fan_days);
            if (init_sequence_active && pending_init_commands[current_init_command_index] == CMD_SPS30_INFO) {
                current_init_command_index++;
            }
            break;
        }
        case RSP_SPS30_CLEAN: {
            // Format: c<ret_status>    
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());
            char* token = strtok(payload_cstr, ","); if (!token) return; int ret_status = atoi(token);
            logger.debugf("SPS30 Manual Fan Cleaning: Status=%d", ret_status);
        }
        case RSP_SGP40_TEST: {
            // Format: g<ret_status>,<raw_value>
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());
            char* token = strtok(payload_cstr, ","); if (!token) return; int ret_status = atoi(token);
            token = strtok(NULL, ","); if (!token) return; uint16_t raw_value = strtoul(token, nullptr, 16);

            logger.debugf("SGP40 Test Result: Status=%d, RawValue=0x%04X", ret_status, raw_value);
            uiUpdater.update_sgp40_test(ret_status, raw_value);
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
            uiUpdater.update_scd30_autocal(auto_calibration != 0);
            uiUpdater.update_scd30_forcecal(forced_recalibration_value);
            
            if (init_sequence_active && pending_init_commands[current_init_command_index] == CMD_SCD30_INFO) {
                current_init_command_index++;
            }
            break;
        }
        case RSP_SCD30_AUTOCAL: {
            // Format: t<set_result>,<read_result>,<actual_state>
            char payload_cstr[payload.length() + 1];
            strcpy(payload_cstr, payload.c_str());
            char* token = strtok(payload_cstr, ","); if (!token) return; int set_result = strtol(token, nullptr, 16);
            token = strtok(NULL, ","); if (!token) return; int read_result = strtol(token, nullptr, 16);
            token = strtok(NULL, ","); if (!token) return; uint16_t actual_state = strtol(token, nullptr, 16);
            
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
            char* token = strtok(payload_cstr, ","); if (!token) return; int set_result = strtol(token, nullptr, 16);
            token = strtok(NULL, ","); if (!token) return; int read_result = strtol(token, nullptr, 16);
            token = strtok(NULL, ","); if (!token) return; uint16_t actual_value = strtol(token, nullptr, 16);
            
            logger.debugf("SCD30 Force Calibration Set: SetResult=%d, ReadResult=%d, ActualValue=%u ppm", 
                set_result, read_result, actual_value);
            
            // Update the number value based on the actual readback value
            haManager.updateScd30ForceCalValue(actual_value);
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
}

void setup() {
    logger.info("--- System Booting ---");
    Serial.begin(9600);
    configManager.init();
    logger.init(haManager.getMqtt());
    logger.setLogLevel(configManager.getLogLevel());
    delay(500);
    
    String reset_reason = get_reset_reason_string();
    logger.infof("Firmware Version: %s", FIRMWARE_VERSION);
    logger.infof("Reset Reason: %s", reset_reason.c_str());
    
    tft.init(); tft.setRotation(1); tft.setBrightness(255); tft.fillScreen(TFT_BLACK);
    logger.info("TFT display initialized.");
    touch.begin();
    logger.info("Touch controller initialized.");
    
    IUIManager& uiManager = UI::getInstance();
    IUIUpdater& uiUpdater = UI::getInstance();
    uiManager.init(&tft, &touch, &configManager);
    uiManager.create_widgets();
    uiUpdater.set_initial_debug_info(FIRMWARE_VERSION, reset_reason.c_str());
    logger.info("UI singleton initialized.");
    lv_timer_handler();
    
    setup_wifi();
    haManager.init(&tft, &uiUpdater, &configManager, FIRMWARE_VERSION);
    webServerManager.init(FIRMWARE_VERSION);
    otaManager.init();
}

void loop() {
    logger.loop();
    static bool valid_packet_start_received = false;

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

    haManager.loop();
    IUIManager& uiManager = UI::getInstance();
    IUIUpdater& uiUpdater = UI::getInstance();

    otaManager.handle();
    webServerManager.handle();

    if (millis() - last_wifi_check_time > WIFI_CHECK_INTERVAL_MS) {
        last_wifi_check_time = millis();
        update_wifi_status();
    }

    if (is_sensor_module_connected && (millis() - last_health_check_time > HEALTH_CHECK_INTERVAL_MS)) {
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
        uiUpdater.update_last_packet_time(seconds_since_packet, is_sensor_module_connected);
        haManager.publishSensorStackUptime(nano_current_uptime_seconds);
        haManager.publishEsp32FreeRam(ESP.getFreeHeap());
        haManager.publishEsp32Uptime(millis() / 1000);
        uiUpdater.update_runtime_info(ESP.getFreeHeap(), nano_current_uptime_seconds);
        
        // Update SensorStack tile with current info
        if (is_sensor_module_connected) {
            uiUpdater.update_sensorstack_info(last_nano_version.c_str(), nano_current_uptime_seconds, last_nano_ram, true);
        } else {
            uiUpdater.update_sensorstack_info("N/A", 0, 0, false);
        }

        bool wifi_connected = (WiFi.status() == WL_CONNECTED);
        String ip = wifi_connected ? WiFi.localIP().toString() : "N/A";
        int8_t rssi = wifi_connected ? WiFi.RSSI() : 0;
        String ssid = wifi_connected ? WIFI_SSID : "N/A";
        bool ha_conn = haManager.isMqttConnected();
        uiUpdater.update_network_info(ip.c_str(), WiFi.macAddress().c_str(), rssi, ssid.c_str(), ha_conn);
    }

    if (millis() - last_sensor_data_time > NANO_PACKET_TIMEOUT_MS) {
        if (is_sensor_module_connected) {
            is_sensor_module_connected = false;
            logger.warning("Sensor data timeout. Marking as disconnected.");
            IUIUpdater& uiUpdaterTimeout = UI::getInstance();
            uiUpdaterTimeout.update_sensor_status(false);
            haManager.publishSensorConnectionStatus(false);
            haManager.setSensorStackVersionUnavailable();
            haManager.publishSensorStackUptime(0, true);
            uiUpdaterTimeout.clearSensorReadings();
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

    if (is_sensor_module_connected && !init_sequence_active && (millis() - last_scd30_info_time > SCD30_INFO_INTERVAL_MS)) {
        last_scd30_info_time = millis();
        send_command_to_nano(CMD_GET_SCD30_INFO);
    }


    uiManager.run();
    delay(5);
}

#include "SensorStackStatusManager.h"
#include <string.h>
#include <stdio.h>
#include "Logger.h"
#include "UITask.h"
#include "HomeAssistantManager.h"

static SensorStackStatusManager* s_status_instance = nullptr;

SensorStackStatusManager::SensorStackStatusManager()
    : _ui(nullptr),
      _ha(nullptr),
      _connected(false),
      _last_packet_time(0),
      _nano_boot_millis(0),
      _free_ram(0),
      _reset_cause_code(0),
      _first_health_packet_received(false),
      _sps30_device_info{},
      _sgp41_test_result_data{} {
    _version[0] = '\0';
    s_status_instance = this;
}

void SensorStackStatusManager::begin(UITask* ui, HomeAssistantManager* ha) {
    _ui = ui;
    _ha = ha;
    s_status_instance = this;
}


const char* SensorStackStatusManager::resetCauseToString(uint8_t code) {
    switch (code) {
        case 1: return "Power-On";
        case 2: return "External";
        case 3: return "Brown-Out";
        case 4: return "Watchdog";
        case 0:
        default: return "Unknown";
    }
}

const char* SensorStackStatusManager::getResetCauseStr() const {
    return resetCauseToString(_reset_cause_code);
}

void SensorStackStatusManager::onPacketReceived() {
    _last_packet_time = millis();
    if (!_connected) {
        setConnected(true);
    }
}

void SensorStackStatusManager::setConnected(bool connected) {
    if (_connected == connected) {
        return;
    }
    _connected = connected;

    if (_connected) {
        _nano_boot_millis = millis();
        logger.info("Sensor module connection established.");
        if (_ui) _ui->update_sensor_status(true);
        if (_ha) _ha->publishSensorConnectionStatus(true);
    } else {
        logger.warning("Sensor module connection lost / timeout.");
        if (_ui) {
            _ui->update_sensor_status(false);
            _ui->clearSensorReadings();
            _ui->update_sensorstack_uptime(0);
            _ui->update_sensorstack_ram(0);
        }
        if (_ha) {
            _ha->publishSensorConnectionStatus(false);
            _ha->setSensorStackVersionUnavailable();
            _ha->publishSensorStackUptime(0, true);
        }
    }
}

void SensorStackStatusManager::setVersion(const char* version_str) {
    if (!version_str) return;
    strncpy(_version, version_str, sizeof(_version) - 1);
    _version[sizeof(_version) - 1] = '\0';

    if (_ha) _ha->publishSensorStackVersion(_version);
    if (_ui) _ui->update_fw_version(_version);
}

void SensorStackStatusManager::setHealth(int first_time_flag, uint16_t free_ram, uint8_t reset_cause) {
    const char *reset_cause_str = resetCauseToString(reset_cause);

    if (first_time_flag == 0 || !_first_health_packet_received) {
        logger.infof("Sensor Stack Health: Flag=%d, FreeRAM=%d bytes, ResetCause=%s",
                     first_time_flag, free_ram, reset_cause_str);
        _first_health_packet_received = true;
    }
    if (free_ram < 64) {
        logger.warningf("Nano free RAM critically low: %d bytes", free_ram);
    }

    _free_ram = free_ram;
    _reset_cause_code = reset_cause;

    if (_ha) {
        _ha->publishSensorStackFreeRam(free_ram);
        _ha->publishNanoResetCause(reset_cause_str);
    }

    if (first_time_flag == 0) {
        _nano_boot_millis = millis();
        logger.warning("Nano reported first boot (or reboot). Uptime counter reset.");
        if (_ha) _ha->resetSensorStackUptimePublishTime();
    }
}

void SensorStackStatusManager::setSps30Info(const SPS30DeviceInfo& info) {
    _sps30_device_info = info;
    if (_ui) {
        _ui->update_sps30_fan_interval(info.fan_interval);
        _ui->update_sps30_fan_days(info.fan_days);
    }
}

void SensorStackStatusManager::setSgp41TestResult(const SGP41TestResult& result) {
    _sgp41_test_result_data = result;
    if (_ui) {
        _ui->update_sgp41_test_status(result.ret_status);
        _ui->update_sgp41_test_value(result.raw_value);
    }
}

void SensorStackStatusManager::setScd30Info(bool auto_cal, uint16_t force_cal) {
    if (_ha) {
        _ha->updateScd30AutoCalState(auto_cal);
        _ha->updateScd30ForceCalValue(force_cal);
    }
    if (_ui) {
        _ui->update_scd30_autocal(auto_cal);
        _ui->update_scd30_forcecal(force_cal);
    }
}

void SensorStackStatusManager::setScd30AutoCalState(bool auto_cal) {
    if (_ha) _ha->updateScd30AutoCalState(auto_cal);
    if (_ui) _ui->update_scd30_autocal(auto_cal);
}

void SensorStackStatusManager::setScd30ForceCalValue(uint16_t force_cal) {
    if (_ha) _ha->updateScd30ForceCalValue(force_cal);
    if (_ui) _ui->update_scd30_forcecal(force_cal);
}

bool SensorStackStatusManager::processHealthPayload(char* payload, int* out_first_time_flag) {
    char *token = strtok(payload, ",");
    if (!token) return false;
    int first_time_flag = atoi(token);

    token = strtok(NULL, ",");
    if (!token) return false;
    uint16_t nano_free_ram = atoi(token);

    token = strtok(NULL, ",");
    uint8_t nano_reset_cause = 0;
    if (token) nano_reset_cause = atoi(token);

    setHealth(first_time_flag, nano_free_ram, nano_reset_cause);
    if (out_first_time_flag) {
        *out_first_time_flag = first_time_flag;
    }
    return true;
}

bool SensorStackStatusManager::processSps30InfoPayload(char* payload) {
    char *token = strtok(payload, ",");
    if (!token) return false;
    int ret_fw = atoi(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    int fw_version_major = atoi(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    int fw_version_minor = atoi(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    int ret_fan_interval = atoi(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    unsigned long fan_interval = strtoul(token, nullptr, 10);
    token = strtok(NULL, ",");
    if (!token) return false;
    int ret_fan_days = atoi(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    unsigned long fan_days = strtoul(token, nullptr, 10);
    token = strtok(NULL, ",");
    if (!token) return false;
    int ret_status = atoi(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    unsigned long status_reg = strtoul(token, nullptr, 10);

    logger.debugf("SPS30 Info: FwVersion(ret=%d): %u.%u, FanInterval(ret=%d): %lu, FanDays(ret=%d): %lu, StatusReg(ret=%d): 0x%lX",
                  ret_fw, fw_version_major, fw_version_minor, ret_fan_interval,
                  fan_interval, ret_fan_days, fan_days, ret_status, status_reg);

    SPS30DeviceInfo info;
    info.valid = true;
    info.fw_major = static_cast<uint8_t>(fw_version_major);
    info.fw_minor = static_cast<uint8_t>(fw_version_minor);
    info.fan_interval = fan_interval;
    info.fan_days = fan_days;
    info.status_reg = status_reg;

    setSps30Info(info);
    return true;
}

void SensorStackStatusManager::processSps30CleanPayload(char* payload) {
    char *token = strtok(payload, ",");
    if (!token) return;
    int ret_status = atoi(token);
    logger.debugf("SPS30 Manual Fan Cleaning: Status=%d", ret_status);
}

bool SensorStackStatusManager::processSgp41TestPayload(char* payload) {
    char *token = strtok(payload, ",");
    if (!token) return false;
    int ret_status = atoi(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    uint16_t raw_value = strtoul(token, nullptr, 16);

    logger.debugf("SGP41 Test Result: Status=%d, RawValue=0x%04X", ret_status, raw_value);
    SGP41TestResult res;
    res.valid = true;
    res.ret_status = ret_status;
    res.raw_value = raw_value;

    setSgp41TestResult(res);
    return true;
}

bool SensorStackStatusManager::processScd30InfoPayload(char* payload) {
    auto parse_hex_u16 = [](const char *t) -> uint16_t {
        return static_cast<uint16_t>(strtol(t, nullptr, 16));
    };
    auto parse_hex_s16 = [](const char *t) -> int16_t {
        return static_cast<int16_t>(static_cast<uint16_t>(strtol(t, nullptr, 16)));
    };

    char *token = strtok(payload, ",");
    if (!token) return false;
    [[maybe_unused]] int16_t ret_interval = parse_hex_s16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] uint16_t measurement_interval = parse_hex_u16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] int16_t ret_auto_cal = parse_hex_s16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    uint16_t auto_calibration = parse_hex_u16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] int16_t ret_forced_cal = parse_hex_s16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    uint16_t forced_recalibration_value = parse_hex_u16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] int16_t ret_temp_offset = parse_hex_s16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] uint16_t temperature_offset = parse_hex_u16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] int16_t ret_altitude = parse_hex_s16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] uint16_t altitude_compensation = parse_hex_u16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] int16_t ret_firmware = parse_hex_s16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] uint8_t fw_major = parse_hex_u16(token);
    token = strtok(NULL, ",");
    if (!token) return false;
    [[maybe_unused]] uint8_t fw_minor = parse_hex_u16(token);

#ifdef SERIAL_PACKET_DEBUG
    logger.debugf("SCD30 Info: MeasurementInterval(ret=%d): %u, AutoCalibration(ret=%d): %s, "
                  "ForcedRecalibrationValue(ret=%d): %u, TemperatureOffset(ret=%d): %u, "
                  "AltitudeCompensation(ret=%d): %u, FwVersion(ret=%d): %u.%u",
                  ret_interval, measurement_interval, ret_auto_cal,
                  auto_calibration ? "ON" : "OFF", ret_forced_cal,
                  forced_recalibration_value, ret_temp_offset,
                  temperature_offset, ret_altitude, altitude_compensation,
                  ret_firmware, fw_major, fw_minor);
#endif

    setScd30Info(auto_calibration != 0, forced_recalibration_value);
    return true;
}

bool SensorStackStatusManager::processScd30AutoCalPayload(char* payload) {
    char *token = strtok(payload, ",");
    if (!token) return false;
    int set_result = strtol(token, nullptr, 16);
    token = strtok(NULL, ",");
    if (!token) return false;
    int read_result = strtol(token, nullptr, 16);
    token = strtok(NULL, ",");
    if (!token) return false;
    uint16_t actual_state = strtol(token, nullptr, 16);

    logger.debugf("SCD30 AutoCalibration Set: SetResult=%d, ReadResult=%d, ActualState=%s",
                  set_result, read_result, actual_state ? "ON" : "OFF");
    setScd30AutoCalState(actual_state != 0);
    return true;
}

bool SensorStackStatusManager::processScd30ForceCalPayload(char* payload) {
    char *token = strtok(payload, ",");
    if (!token) return false;
    int set_result = strtol(token, nullptr, 16);
    token = strtok(NULL, ",");
    if (!token) return false;
    int read_result = strtol(token, nullptr, 16);
    token = strtok(NULL, ",");
    if (!token) return false;
    uint16_t actual_value = strtol(token, nullptr, 16);

    logger.debugf("SCD30 Force Calibration Set: SetResult=%d, ReadResult=%d, ActualValue=%u ppm",
                  set_result, read_result, actual_value);
    setScd30ForceCalValue(actual_value);
    return true;
}

void SensorStackStatusManager::updateDiagnostics() {
    uint32_t seconds_since_packet = (_last_packet_time > 0) ? (millis() - _last_packet_time) / 1000 : 0;
    uint32_t nano_current_uptime_seconds =
        (_connected && _nano_boot_millis > 0) ? (millis() - _nano_boot_millis) / 1000 : 0;

    if (_ui) {
        _ui->update_last_packet_time(seconds_since_packet);
        if (_connected) {
            _ui->update_sensorstack_uptime(nano_current_uptime_seconds);
            _ui->update_sensorstack_ram(_free_ram);
        } else {
            _ui->update_sensorstack_uptime(0);
            _ui->update_sensorstack_ram(0);
        }
    }

    if (_ha) {
        _ha->publishSensorStackUptime(nano_current_uptime_seconds);
    }
}

void SensorStackStatusManager::populateDashboardData(LiveDashboardData& data) const {
    data.sensor_module_connected = _connected;
    data.seconds_since_packet = (_last_packet_time > 0) ? (millis() - _last_packet_time) / 1000 : 0;
    data.nano_uptime_seconds = (_connected && _nano_boot_millis > 0) ? (millis() - _nano_boot_millis) / 1000 : 0;
    data.nano_free_ram = _free_ram;
    snprintf(data.nano_version, sizeof(data.nano_version), "%s", _version);
    snprintf(data.nano_reset_cause, sizeof(data.nano_reset_cause), "%s", getResetCauseStr());
}

const SPS30DeviceInfo& get_sps30_device_info() {
    static SPS30DeviceInfo dummy;
    return s_status_instance ? s_status_instance->getSps30DeviceInfo() : dummy;
}

const SGP41TestResult& get_sgp41_test_result() {
    static SGP41TestResult dummy;
    return s_status_instance ? s_status_instance->getSgp41TestResult() : dummy;
}


#include "SensorStackStatusManager.h"
#include <string.h>
#include <stdio.h>
#include "Logger.h"
#include "SensorDefines.h"
#include "SystemStatusAccessor.h"
#include "DiagnosticDataAccessor.h"

static SensorStackStatusManager* s_status_instance = nullptr;

SensorStackStatusManager::SensorStackStatusManager()
    : _connected(false),
      _last_packet_time(0),
      _nano_boot_millis(0),
      _free_ram(0),
      _reset_cause_code(0),
      _first_health_packet_received(false),
      _firmware_state(SensorStackFirmwareState::Ok),
      _sensor_fault_mask(0) {
    _version[0] = '\0';
    s_status_instance = this;
}

void SensorStackStatusManager::begin() {
    s_status_instance = this;
}


const char* SensorStackStatusManager::resetCauseToString(uint8_t code) {
    switch (code) {
        case SensorDefines::RESET_CAUSE_POWER_ON:  return "Power-On";
        case SensorDefines::RESET_CAUSE_EXTERNAL:  return "External";
        case SensorDefines::RESET_CAUSE_BROWN_OUT: return "Brown-Out";
        case SensorDefines::RESET_CAUSE_WATCHDOG:  return "Watchdog";
        case SensorDefines::RESET_CAUSE_SOFTWARE:  return "Software";
        case SensorDefines::RESET_CAUSE_UNKNOWN:
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
    // A valid packet can only come from a running application
    if (_firmware_state == SensorStackFirmwareState::Faulty) {
        setFirmwareState(SensorStackFirmwareState::Ok);
    }
}

void SensorStackStatusManager::setSensorFaultMask(uint8_t mask) {
    uint8_t changed = mask ^ _sensor_fault_mask;
    if (!changed) {
        return;
    }
    _sensor_fault_mask = mask;

    static const struct { uint8_t bit; const char* name; const char* readings; } sensors[] = {
        { SensorDefines::SENSOR_FAULT_SPS30, "SPS30", "particulate matter" },
        { SensorDefines::SENSOR_FAULT_SCD30, "SCD30", "CO2, temperature and humidity" },
        { SensorDefines::SENSOR_FAULT_SGP41, "SGP41", "VOC and NOx" },
    };
    for (const auto& s : sensors) {
        if (!(changed & s.bit)) continue;
        if (mask & s.bit) {
            logger.errorf("SensorStack: %s is not responding (defective or missing); %s readings are ignored.",
                          s.name, s.readings);
        } else {
            logger.infof("SensorStack: %s is responding again; %s readings resumed.", s.name, s.readings);
        }
    }
    {
        SystemStatusAccessor shared;
        shared->sensorStackSensorFaults = mask;
    }
}

void SensorStackStatusManager::setFirmwareState(SensorStackFirmwareState state) {
    if (_firmware_state == state) {
        return;
    }
    _firmware_state = state;

    switch (state) {
        case SensorStackFirmwareState::Updating:
            logger.info("SensorStack firmware update in progress.");
            break;
        case SensorStackFirmwareState::Faulty:
            logger.error("SensorStack firmware issue: bootloader is waiting for a firmware upload (no valid application).");
            break;
        case SensorStackFirmwareState::Ok:
        default:
            logger.info("SensorStack firmware state OK.");
            break;
    }
    {
        SystemStatusAccessor shared;
        shared->sensorStackFirmwareState = state;
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
        SystemStatusAccessor shared;
        shared->sensorConnectionStatus = true;
    } else {
        logger.warning("Sensor module connection lost / timeout.");
        SystemStatusAccessor shared;
        shared->sensorConnectionStatus = false;
        shared->sensorStackVersionUnavailable = true;
        shared->sensorStackUptime = 0;
        shared->sensorStackUptimeForce = true;
    }
}

void SensorStackStatusManager::setVersion(const char* version_str) {
    if (!version_str) return;
    strncpy(_version, version_str, sizeof(_version) - 1);
    _version[sizeof(_version) - 1] = '\0';

    SystemStatusAccessor shared;
    strncpy(shared->sensorStackVersion, _version, sizeof(shared->sensorStackVersion) - 1);
    shared->sensorStackVersion[sizeof(shared->sensorStackVersion) - 1] = '\0';
    shared->sensorStackVersionUnavailable = false;
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

    {
        SystemStatusAccessor shared;
        shared->sensorStackFreeRam = free_ram;
        strncpy(shared->nanoResetCause, reset_cause_str, sizeof(shared->nanoResetCause) - 1);
        shared->nanoResetCause[sizeof(shared->nanoResetCause) - 1] = '\0';
    }

    if (first_time_flag == 0) {
        _nano_boot_millis = millis();
        logger.warning("Nano reported first boot (or reboot). Uptime counter reset.");
    }
}

void SensorStackStatusManager::setSps30Info(const SPS30DeviceInfo& info) {
    DiagnosticDataAccessor diag;
    diag->sps30.valid = info.valid;
    diag->sps30.timestamp = millis();
    diag->sps30.fwMajor = info.fw_major;
    diag->sps30.fwMinor = info.fw_minor;
    diag->sps30.fanInterval = info.fan_interval;
    diag->sps30.fanDays = info.fan_days;
    diag->sps30.statusReg = info.status_reg;
}

void SensorStackStatusManager::setSgp41TestResult(const SGP41TestResult& result) {
    DiagnosticDataAccessor diag;
    diag->sgp41.valid = result.valid;
    diag->sgp41.timestamp = millis();
    diag->sgp41.retStatus = result.ret_status;
    diag->sgp41.rawValue = result.raw_value;
}

void SensorStackStatusManager::setScd30Info(bool auto_cal, uint16_t force_cal) {
    {
        DiagnosticDataAccessor diag;
        diag->scd30.valid = true;
        diag->scd30.timestamp = millis();
        diag->scd30.autoCalState = auto_cal;
        diag->scd30.forceCalValue = force_cal;
    }
    {
        SystemStatusAccessor shared;
        shared->scd30AutoCalState = auto_cal;
        shared->scd30ForceCalValue = force_cal;
    }
}

void SensorStackStatusManager::setScd30AutoCalState(bool auto_cal) {
    SystemStatusAccessor shared;
    shared->scd30AutoCalState = auto_cal;
    shared->scd30AutoCalSeq++;
}

void SensorStackStatusManager::setScd30ForceCalValue(uint16_t force_cal) {
    SystemStatusAccessor shared;
    shared->scd30ForceCalValue = force_cal;
    shared->scd30ForceCalSeq++;
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

    // Sensor fault mask (absent from SensorStack firmware before 1.4.4)
    token = strtok(NULL, ",");
    uint8_t sensor_fault_mask = token ? atoi(token) : 0;

    setHealth(first_time_flag, nano_free_ram, nano_reset_cause);
    setSensorFaultMask(sensor_fault_mask);
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

    if (read_result != 0) {
        // The state the SensorStack applied is unknown: republish the last confirmed one
        logger.warningf("SCD30 AutoCalibration could not be confirmed: SetResult=%d, ReadResult=%d",
                        set_result, read_result);
        SystemStatusAccessor()->scd30AutoCalSeq++;
        return true;
    }
    if (set_result != 0) {
        logger.warningf("SCD30 AutoCalibration change failed: SetResult=%d, ActualState=%s",
                        set_result, actual_state ? "ON" : "OFF");
    } else {
        logger.debugf("SCD30 AutoCalibration Set: SetResult=%d, ReadResult=%d, ActualState=%s",
                      set_result, read_result, actual_state ? "ON" : "OFF");
    }
    // Also reverts the controls when the change was refused
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

    if (set_result == 0 && read_result == 0 && SensorDefines::isValidForceCal(actual_value)) {
        logger.debugf("SCD30 Force Calibration Set: SetResult=%d, ReadResult=%d, ActualValue=%u ppm",
                      set_result, read_result, actual_value);
        setScd30ForceCalValue(actual_value);
    } else {
        logger.warningf("SCD30 Force Calibration rejected or failed: SetResult=%d, ReadResult=%d, Value=%u ppm",
                        set_result, read_result, actual_value);
        // Revert the controls to the last confirmed value
        SystemStatusAccessor()->scd30ForceCalSeq++;
    }
    return true;
}

void SensorStackStatusManager::updateDiagnostics() {
    uint32_t seconds_since_packet = (_last_packet_time > 0) ? (millis() - _last_packet_time) / 1000 : 0;
    uint32_t nano_current_uptime_seconds =
        (_connected && _nano_boot_millis > 0) ? (millis() - _nano_boot_millis) / 1000 : 0;

    SystemStatusAccessor shared;
    shared->sensorStackSecondsSincePacket = seconds_since_packet;
    shared->sensorStackUptime = nano_current_uptime_seconds;
    shared->sensorStackFreeRam = _connected ? _free_ram : 0;
}

void SensorStackStatusManager::populateDashboardData(LiveDashboardData& data) const {
    data.sensor_module_connected = _connected;
    data.sensorstack_firmware_state = static_cast<uint8_t>(_firmware_state);
    data.sensorstack_sensor_faults = _sensor_fault_mask;
    data.seconds_since_packet = (_last_packet_time > 0) ? (millis() - _last_packet_time) / 1000 : 0;
    data.nano_uptime_seconds = (_connected && _nano_boot_millis > 0) ? (millis() - _nano_boot_millis) / 1000 : 0;
    data.nano_free_ram = _free_ram;
    snprintf(data.nano_version, sizeof(data.nano_version), "%s", _version);
    snprintf(data.nano_reset_cause, sizeof(data.nano_reset_cause), "%s", getResetCauseStr());
}


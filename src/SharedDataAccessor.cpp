#include "SharedDataAccessor.h"

SemaphoreHandle_t SharedDataAccessor::_sharedMutex = nullptr;
StaticSemaphore_t SharedDataAccessor::_mutexBuffer;
SharedStateData SharedDataAccessor::_state;

void SharedDataAccessor::init() {
    if (_sharedMutex == nullptr) {
        _sharedMutex = xSemaphoreCreateRecursiveMutexStatic(&_mutexBuffer);
    }
}

SharedDataAccessor::SharedDataAccessor()
    : BaseAccessor((init(), _sharedMutex)) {}

void SharedDataAccessor::setScd30AutoCalState(bool state, bool force) {
    if (_state.scd30AutoCalState != state || force) {
        _state.scd30AutoCalState = state;
        _state.scd30AutoCalForce = force;
        _state.scd30AutoCalVersion++;
    }
}

void SharedDataAccessor::setScd30ForceCalValue(uint16_t value, bool force) {
    if (_state.scd30ForceCalValue != value || force) {
        _state.scd30ForceCalValue = value;
        _state.scd30ForceCalForce = force;
        _state.scd30ForceCalVersion++;
    }
}

void SharedDataAccessor::setBacklightState(bool state, uint8_t brightness) {
    if (_state.backlightState != state || _state.backlightBrightness != brightness) {
        _state.backlightState = state;
        _state.backlightBrightness = brightness;
        _state.backlightVersion++;
    }
}

void SharedDataAccessor::setInactivityTimerDelay(int delay, bool force) {
    if (_state.inactivityTimerDelay != delay || force) {
        _state.inactivityTimerDelay = delay;
        _state.inactivityTimerDelayForce = force;
        _state.inactivityTimerDelayVersion++;
    }
}

void SharedDataAccessor::setLogLevel(AppLogLevel level) {
    if (_state.logLevel != level) {
        _state.logLevel = level;
        _state.logLevelVersion++;
    }
}

void SharedDataAccessor::setMqttConnected(bool connected) {
    if (_state.mqttConnected != connected) {
        _state.mqttConnected = connected;
    }
}

void SharedDataAccessor::setSensorData(
    float pressure, int cpm, float temp, float humi,
    float co2, int32_t voc_index, int32_t nox_index,
    float amps,
    float pm1, float pm25, float pm4, float pm10,
    float compressor_amps, float geothermal_pump_amps, bool liquid_level_sensor_state,
    float co_ppm,
    float filter_pressure
) {
    _state.pressure = pressure;
    _state.cpm = cpm;
    _state.temp = temp;
    _state.humi = humi;
    _state.co2 = co2;
    _state.voc_index = voc_index;
    _state.nox_index = nox_index;
    _state.fan_amps = amps;
    _state.pm1 = pm1;
    _state.pm25 = pm25;
    _state.pm4 = pm4;
    _state.pm10 = pm10;
    _state.compressor_amps = compressor_amps;
    _state.geothermal_pump_amps = geothermal_pump_amps;
    _state.liquid_level_sensor_state = liquid_level_sensor_state;
    _state.co_ppm = co_ppm;
    _state.filter_pressure = filter_pressure;
    _state.sensorDataVersion++;
}

void SharedDataAccessor::setHighPressureStatus(bool is_high) {
    if (_state.high_pressure != is_high) {
        _state.high_pressure = is_high;
        _state.sensorDataVersion++;
    }
}

void SharedDataAccessor::setFanStatus(bool is_on) {
    if (_state.fan_status != is_on) {
        _state.fan_status = is_on;
        _state.sensorDataVersion++;
    }
}

void SharedDataAccessor::setZmodData(float o3_conc_ug_per_m3, float no2_conc_ug_per_m3, uint16_t fast_aqi, uint16_t epa_aqi) {
    _state.o3_conc_ug_per_m3 = o3_conc_ug_per_m3;
    _state.no2_conc_ug_per_m3 = no2_conc_ug_per_m3;
    _state.fast_aqi = fast_aqi;
    _state.epa_aqi = epa_aqi;
    _state.zmodDataVersion++;
}

#ifdef BMP280_ENABLED
void SharedDataAccessor::setBmpData(float pressure_pa, float temp_c) {
    _state.bmp_pressure_pa = pressure_pa;
    _state.bmp_temp_c = temp_c;
    _state.bmpDataVersion++;
}
#endif

#ifdef AHT20_ENABLED
void SharedDataAccessor::setAhtData(float temp_c, float humi_pct) {
    _state.aht_temp_c = temp_c;
    _state.aht_humi_pct = humi_pct;
    _state.ahtDataVersion++;
}
#endif

void SharedDataAccessor::setWifiStatus(bool connected, int8_t rssi, const char* ssid, const char* ip) {
    _state.wifiConnected = connected;
    _state.wifiRssi = rssi;
    if (ssid) {
        strncpy(_state.wifiSsid, ssid, sizeof(_state.wifiSsid) - 1);
        _state.wifiSsid[sizeof(_state.wifiSsid) - 1] = '\0';
    }
    if (ip) {
        strncpy(_state.wifiIp, ip, sizeof(_state.wifiIp) - 1);
        _state.wifiIp[sizeof(_state.wifiIp) - 1] = '\0';
    }
    _state.wifiStatusVersion++;
}

void SharedDataAccessor::setSensorStackStatus(bool connected) {
    if (_state.sensorConnectionStatus != connected) {
        _state.sensorConnectionStatus = connected;
        _state.sensorConnectionStatusVersion++;
    }
}

void SharedDataAccessor::setSensorStackVersion(const char* version) {
    if (version) {
        strncpy(_state.sensorStackVersion, version, sizeof(_state.sensorStackVersion) - 1);
        _state.sensorStackVersion[sizeof(_state.sensorStackVersion) - 1] = '\0';
        _state.sensorStackVersionUnavailable = false;
        _state.sensorStackVersionCount++;
    }
}

void SharedDataAccessor::setSensorStackVersionUnavailable() {
    _state.sensorStackVersionUnavailable = true;
    _state.sensorStackVersionCount++;
}

void SharedDataAccessor::setSensorStackUptime(uint32_t uptime_seconds, bool force) {
    _state.sensorStackUptime = uptime_seconds;
    _state.sensorStackUptimeForce = force;
    _state.sensorStackUptimeVersion++;
}

void SharedDataAccessor::setSensorStackFreeRam(uint16_t free_ram) {
    _state.sensorStackFreeRam = free_ram;
    _state.sensorStackFreeRamVersion++;
}

void SharedDataAccessor::setNanoResetCause(const char* cause) {
    if (cause) {
        strncpy(_state.nanoResetCause, cause, sizeof(_state.nanoResetCause) - 1);
        _state.nanoResetCause[sizeof(_state.nanoResetCause) - 1] = '\0';
        _state.nanoResetCauseVersion++;
    }
}

void SharedDataAccessor::setEsp32FreeHeap(uint32_t free_heap) {
    _state.esp32FreeHeap = free_heap;
    _state.esp32HealthVersion++;
}

void SharedDataAccessor::setEsp32Uptime(uint32_t uptime_seconds) {
    _state.esp32Uptime = uptime_seconds;
    _state.esp32HealthVersion++;
}

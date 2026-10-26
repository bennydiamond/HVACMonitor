#include "SensorDataProcessor.h"
#include <string.h>
#include <math.h>

#include "Logger.h"
#include "ConfigManager.h"
#include "UITask.h"
#include "HomeAssistantManager.h"
#include "FilterPressureTracker.h"
#include "SensorTask.h"
#include "SensorDefines.h"

// --- Sensor Calculation Constants ---
#define SHUNT_RESISTOR 150.0f
#define VOLTS_TO_MILLIVOLTS 1000.0f
#define CO_FEEDBACK_RESISTOR_OHMS 2200000.0f
#define ARDUINO_SUPPLY_VOLTAGE 5.0f
#define CO_SENSITIVITY_A_PPM 1.415e-9f
#define ARDUINO_ADC_RESOLUTION_BITS 10
#define ARDUINO_ADC_MAX_VALUE ((1 << ARDUINO_ADC_RESOLUTION_BITS) - 1)
#define PPM_PER_ADC_UNIT \
  (ARDUINO_SUPPLY_VOLTAGE / \
   (ARDUINO_ADC_MAX_VALUE * CO_FEEDBACK_RESISTOR_OHMS * CO_SENSITIVITY_A_PPM))

SensorDataProcessor::SensorDataProcessor()
    : _ui(nullptr),
      _ha(nullptr),
      _filterTracker(nullptr),
      _sensorTask(nullptr),
      _last_received_timestamp(0),
      _sensor_fault_mask(0),
      _co2_avg(100),
      _co_avg(20),
      _voc_avg(100),
      _nox_avg(100),
      _diff_pressure_avg(10),
      _pm1_avg(100),
      _pm25_avg(100),
      _pm4_avg(100),
      _pm10_avg(100),
      _compressor_amps_avg(100),
      _geothermal_pump_amps_avg(100),
      _live_temp_c(0.0f),
      _live_humi_pct(0.0f),
      _live_fan_amps(0.0f),
      _live_fan_status(FAN_STATUS_OFF),
      _live_is_pressure_high(false),
      _live_water_detected(false) {}

void SensorDataProcessor::begin(UITask* ui,
                               HomeAssistantManager* ha,
                               FilterPressureTracker* filterTracker,
                               SensorTask* sensorTask) {
    _ui = ui;
    _ha = ha;
    _filterTracker = filterTracker;
    _sensorTask = sensorTask;
}

void SensorDataProcessor::setSensorFaultMask(uint8_t mask) {
    uint8_t new_faults = mask & ~_sensor_fault_mask;
    _sensor_fault_mask = mask;
    if (!new_faults) {
        return;
    }

    // Drop what was collected before the fault, so nothing stale is shown or published
    if (new_faults & SENSOR_FAULT_SPS30) {
        _pm1_avg.clear();
        _pm25_avg.clear();
        _pm4_avg.clear();
        _pm10_avg.clear();
    }
    if (new_faults & SENSOR_FAULT_SCD30) {
        _co2_avg.clear();
        _live_temp_c = NAN;
        _live_humi_pct = NAN;
    }
    if (new_faults & SENSOR_FAULT_SGP41) {
        _voc_avg.clear();
        _nox_avg.clear();
    }
    // The next packets refill every reading except those of the faulty sensors
    if (_ui) {
        _ui->clearSensorReadings();
    }
}

void SensorDataProcessor::processPayload(char* payload) {
    NanoSensorPacket pkt(payload);
    if (pkt.isValid()) {
        processPacket(pkt);
    } else {
        logger.warning("Malformed sensor packet payload received from Nano.");
    }
}

void SensorDataProcessor::processPacket(const NanoSensorPacket& pkt) {
    // 1. Calculate Differential Pressure from raw ADC
    float voltage = static_cast<float>(pkt.getPressureAdcRaw()) * ARDUINO_SUPPLY_VOLTAGE / ARDUINO_ADC_MAX_VALUE;
    float current_ma = (voltage / SHUNT_RESISTOR) * VOLTS_TO_MILLIVOLTS;
    float diff_pressure_pa = (current_ma > 4.0f) ? (current_ma - 4.0f) * (300.0f / 16.0f) : 0.0f;

    // 2. Timestamp Duplicate & Interval Checking
    // The timestamp is the SensorStack's millis(): it restarts near 0 when the stack reboots
    // (or wraps after ~49 days), so a timestamp going backwards starts a new series like boot does.
    bool is_first_sensor_packet = (_last_received_timestamp == 0) ||
                                  (pkt.getTimestamp() < _last_received_timestamp);
    unsigned long timestamp_diff = 0;
    if (!is_first_sensor_packet) {
        timestamp_diff = pkt.getTimestamp() - _last_received_timestamp;
        if (timestamp_diff == 0) {
            logger.warningf("Received duplicate sensor data: timestamp=%lu (same as previous)", pkt.getTimestamp());
            return;
        } else if (timestamp_diff < 1000) {
            logger.warningf("Received very recent sensor data: timestamp=%lu, diff=%lu ms (expected ~2000ms)",
                            pkt.getTimestamp(), timestamp_diff);
        }
    }
    _last_received_timestamp = pkt.getTimestamp();

    // 3. Geiger Counter Sample Accumulation
    if (!is_first_sensor_packet && timestamp_diff >= 1000 && timestamp_diff <= 3500) {
        _geigerCounter.addSample(pkt.getPulseCount());
    } else if (is_first_sensor_packet) {
        logger.infof("Discarded initial geiger pulse count after boot/reconnect (%u pulses accumulated during downtime)",
                     pkt.getPulseCount());
    } else {
        logger.warningf("Skipped geiger sample due to abnormal interval: %lu ms (%u pulses)",
                        timestamp_diff, pkt.getPulseCount());
    }
    int c = _geigerCounter.getCPM();

    // 4. CO Concentration Calculation
    uint16_t co_ppm = 0;
    if (pkt.getTimestamp() >= 60000 || millis() >= 60000) {
        co_ppm = static_cast<uint16_t>(pkt.getCoAdcRaw() * PPM_PER_ADC_UNIT);
    }

    // Sensors the SensorStack could not initialize: their values are placeholders, ignore them
    const bool sps30_ok = !(_sensor_fault_mask & SENSOR_FAULT_SPS30);
    const bool scd30_ok = !(_sensor_fault_mask & SENSOR_FAULT_SCD30);
    const bool sgp41_ok = !(_sensor_fault_mask & SENSOR_FAULT_SGP41);
    const float temp_c = scd30_ok ? pkt.getTemperatureC() : NAN;
    const float humi_pct = scd30_ok ? pkt.getHumidityPct() : NAN;

    // 5. Gas Index Algorithms
    int32_t voc_index = 0;
    int32_t nox_index = 0;
    if (sgp41_ok && isValidVocRaw(pkt.getVocRaw())) {
        voc_index = _voc_algorithm.process(pkt.getVocRaw());
        if (isValidGasIndex(voc_index)) {
            _voc_avg.add(voc_index);
        }
    }
    if (sgp41_ok && isValidNoxRaw(pkt.getNoxRaw())) {
        nox_index = _nox_algorithm.process(pkt.getNoxRaw());
        if (isValidGasIndex(nox_index)) {
            _nox_avg.add(nox_index);
        }
    }
    if (scd30_ok && isValidCO2(pkt.getCo2Ppm())) {
        _co2_avg.add(pkt.getCo2Ppm());
    }

    // 6. Update Rolling Averages
    _co_avg.add(co_ppm);
    _diff_pressure_avg.add(diff_pressure_pa);
    if (sps30_ok) {
        _pm1_avg.add(pkt.getPm1());
        _pm25_avg.add(pkt.getPm25());
        _pm4_avg.add(pkt.getPm4());
        _pm10_avg.add(pkt.getPm10());
    }
    _compressor_amps_avg.add(pkt.getCompressorAmps());
    _geothermal_pump_amps_avg.add(pkt.getPumpAmps());

    // 7. Equipment Status & Filter Tracker
    FanStatus fan_status;
    {
        ConfigManagerAccessor config;
        if (pkt.getFanAmps() <= config->getFanOffCurrentThreshold()) {
            fan_status = FAN_STATUS_OFF;
        } else if (pkt.getFanAmps() < config->getFanOnCurrentThreshold() ||
                   pkt.getFanAmps() > config->getFanHighCurrentThreshold()) {
            fan_status = FAN_STATUS_ALERT;
        } else {
            fan_status = FAN_STATUS_NORMAL;
        }
    }

    float filter_pressure = 0.0f;
    if (_filterTracker) {
        _filterTracker->update(_diff_pressure_avg.getAverage(), pkt.getFanAmps(), fan_status, millis());
        filter_pressure = _filterTracker->getFilterPressure();
    }

    bool is_pressure_high = false;
    {
        ConfigManagerAccessor config;
        is_pressure_high =
            (filter_pressure > config->getHighPressureThreshold()) ||
            (fan_status != FAN_STATUS_OFF &&
             _diff_pressure_avg.getAverage() > config->getHighPressureThreshold());
    }

    // 8. Update UI Elements
    if (_ui) {
        _ui->update_high_pressure_status(is_pressure_high);
        _ui->update_pressure(filter_pressure > 0.0f ? filter_pressure : _diff_pressure_avg.getAverage());

        if (_geigerCounter.hasValidData()) {
            _geigerCounter.checkAndLogHighRadiation();
            float usv_h = _geigerCounter.getDoseRate();
            _ui->update_geiger_reading(c, usv_h);
        }

        // Readings of faulty sensors are left blank (cleared when the fault was reported)
        if (scd30_ok) {
            _ui->update_temp_humi(temp_c, humi_pct);
            _ui->update_co2(_co2_avg.getAverage());
        }
        _ui->update_fan_current(pkt.getFanAmps(), fan_status);
        _ui->update_compressor_amps(_compressor_amps_avg.getAverage());
        _ui->update_pump_amps(_geothermal_pump_amps_avg.getAverage());
        if (sgp41_ok) {
            _ui->update_voc(_voc_avg.getAverage());
            _ui->update_nox(_nox_avg.getAverage());
        }
        if (sps30_ok) {
            _ui->update_pm_values(_pm1_avg.getAverage(), _pm25_avg.getAverage(), _pm4_avg.getAverage(), _pm10_avg.getAverage());
        }
        _ui->update_co(_co_avg.getAverage());
        _ui->update_water_sensor(pkt.isWaterDetected());
    }

    // 9. Publish to Home Assistant
    if (_ha) {
        _ha->publishHighPressureStatus(is_pressure_high);
        _ha->publishFanStatus(fan_status != FAN_STATUS_OFF);
        // Invalid values (NaN, or a gas index of 0) are not published
        _ha->publishSensorData(
            _diff_pressure_avg.getAverage(), c, temp_c, humi_pct,
            scd30_ok ? _co2_avg.getAverage() : NAN,
            sgp41_ok ? _voc_avg.getAverage() : 0, sgp41_ok ? _nox_avg.getAverage() : 0, pkt.getFanAmps(),
            sps30_ok ? _pm1_avg.getAverage() : NAN, sps30_ok ? _pm25_avg.getAverage() : NAN,
            sps30_ok ? _pm4_avg.getAverage() : NAN, sps30_ok ? _pm10_avg.getAverage() : NAN,
            _compressor_amps_avg.getAverage(), _geothermal_pump_amps_avg.getAverage(),
            pkt.isWaterDetected(), _co_avg.getAverage(), filter_pressure);
    }

    // 10. Pass ambient conditions to ESP32 local sensor task for compensation
    if (_sensorTask && scd30_ok) {
        _sensorTask->setEnvironmentalData(temp_c, humi_pct);
    }

    // Cache live values
    _live_temp_c = temp_c;
    _live_humi_pct = humi_pct;
    _live_fan_amps = pkt.getFanAmps();
    _live_fan_status = fan_status;
    _live_is_pressure_high = is_pressure_high;
    _live_water_detected = pkt.isWaterDetected();
}

void SensorDataProcessor::populateDashboardData(LiveDashboardData& data) const {
    data.temperature_c = (_last_received_timestamp == 0 || isnan(_live_temp_c)) ? -999.0f : _live_temp_c;
    data.humidity_pct = (_last_received_timestamp == 0 || isnan(_live_humi_pct)) ? -1.0f : _live_humi_pct;
    data.co2_ppm = _co2_avg.isEmpty() ? -1.0f : _co2_avg.getAverage();
    data.voc_index = _voc_avg.isEmpty() ? 0 : _voc_avg.getAverage();
    data.nox_index = _nox_avg.isEmpty() ? 0 : _nox_avg.getAverage();
    data.pm1_ug = _pm1_avg.isEmpty() ? -1.0f : _pm1_avg.getAverage();
    data.pm25_ug = _pm25_avg.isEmpty() ? -1.0f : _pm25_avg.getAverage();
    data.pm4_ug = _pm4_avg.isEmpty() ? -1.0f : _pm4_avg.getAverage();
    data.pm10_ug = _pm10_avg.isEmpty() ? -1.0f : _pm10_avg.getAverage();
    data.co_ppm = (millis() < 60000 && _last_received_timestamp < 60000) ? -1 : (int)_co_avg.getAverage();
    data.geiger_cpm = _geigerCounter.hasValidData() ? _geigerCounter.getCPM() : -1;
    data.geiger_usv_h = _geigerCounter.hasValidData() ? _geigerCounter.getDoseRate() : -1.0f;

    data.diff_pressure_pa = _diff_pressure_avg.getAverage();
    data.filter_pressure_pa = _filterTracker ? _filterTracker->getFilterPressure() : 0.0f;
    data.is_pressure_high = _live_is_pressure_high;
    data.fan_amps = _live_fan_amps;
    data.fan_status = (_live_fan_status == FAN_STATUS_OFF) ? 0 : ((_live_fan_status == FAN_STATUS_ALERT) ? 1 : 2);
    data.compressor_amps = _compressor_amps_avg.getAverage();
    data.pump_amps = _geothermal_pump_amps_avg.getAverage();
    data.water_detected = _live_water_detected;
}

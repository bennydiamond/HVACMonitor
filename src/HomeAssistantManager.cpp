#include "HomeAssistantManager.h"
#include <math.h>
#include "LGFX_ESP32_2432S022C.h"
#include "ui/interfaces/IUIUpdater.h"
#include "secrets.h" 
#include "Logger.h"
#include "NanoCommands.h"
#include "ConfigManager.h"

const unsigned long FORCE_PUBLISH_INTERVAL_MS = 20000;
const unsigned long SENSOR_EXPIRE_TIMEOUT_S = 30;

HomeAssistantManager* HomeAssistantManager::_instance = nullptr;

HomeAssistantManager::HomeAssistantManager() :
    _tft(nullptr),
    _uiUpdater(nullptr),
    _device("hvac_diff_pressure_sensor_01"), // TODO: Make device ID configurable
    _mqtt(_wifiClient, _device),
    _pressureSensor("pressure", HASensor::PrecisionP1),
    _backlight("backlight", HALight::BrightnessFeature),
    _wifi_rssi("wifi_rssi", HASensor::PrecisionP0),
    _wifi_ssid("wifi_ssid"),
    _wifi_ip("wifi_ip"),
    _geiger_cpm("geiger_cpm", HASensor::PrecisionP0),
    _geiger_dose("geiger_dose", HASensor::PrecisionP2),
    _temperatureSensor("temperature", HASensor::PrecisionP1),
    _humiditySensor("humidity", HASensor::PrecisionP1),
    _sensorStatus("sensor_status"),
    _highPressureSensor("high_pressure_status"),
    _fanStatus("fan_status"),
    _rebootButton("reboot"),
    _rebootSensorStackButton("reboot_sensorStack"),
    _pm1_0_sensor("pm1_0", HASensor::PrecisionP1),
    _pm2_5_sensor("pm2_5", HASensor::PrecisionP1),
    _pm4_0_sensor("pm4_0", HASensor::PrecisionP1),
    _pm10_0_sensor("pm10_0", HASensor::PrecisionP1),
    _co2_sensor("co2", HASensor::PrecisionP0),
    _sensorStackUptimeSensor("nano_uptime", HASensor::PrecisionP0), // Initialize Nano Uptime Sensor
    _sensorStackVersionSensor("nano_firmware_version"), 
    _sensorStackFreeRamSensor("nano_free_ram", HASensor::PrecisionP0),
    _voc_index_sensor("voc_index", HASensor::PrecisionP0),
    _currentSensor("current", HASensor::PrecisionP2),
    _sensorStackResetCauseSensor("nano_reset_cause"),
    _getSps30InfoButton("get_sps30_info"),
    _Sps30ManualCleanButton("sps30_manual_clean"),
    _getSgp40SelftestButton("get_sgp40_selftest"),
    _logLevelSelect("log_level"),
    _scd30AutoCalSwitch("scd30_autocal"),
    _scd30ForceCalNumber("scd30_forcecal", HANumber::PrecisionP0)
{
    _instance = this;

    // Initialize state tracking variables
    _lastPublishedPressure = -9999.0f;
    _lastPublishedCpm = -1;
    _lastPublishedTemp = -9999.0f;
    _lastPublishedHumi = -1.0f;
    _lastPublishedCo2 = -1.0f;
    _lastPublishedVocIndex = -1;
    _lastPublishedAmps = -1.0f;
    _lastPublishedWifiRssi = 0;
    _lastPublishedWifiConnected = false;
    _lastPublishedSensorConnected = true;
    _lastPublishedHighPressure = true;
    _lastPublishedFanStatus = true;
    _lastSensorStackUptimePublishTime = 0;
    _lastPublishedSensorStackVersion = "unavailable";
    _lastPublishedPm1_0 = -1.0f;
    _lastPublishedPm2_5 = -1.0f;
    _lastPublishedPm4_0 = -1.0f;
    _lastPublishedPm10_0 = -1.0f;

    _lastPressurePublishTime = 0;
    _lastCpmPublishTime = 0;
    _lastTempPublishTime = 0;
    _lastHumiPublishTime = 0;
    _lastWifiStatusPublishTime = 0;
    _lastSensorStatusPublishTime = 0;
    _lastHighPressurePublishTime = 0;
    _lastPmPublishTime = 0;
    _lastCo2PublishTime = 0;
    _lastVocPublishTime = 0;
    _lastAmpsPublishTime = 0;
    _lastFanStatusPublishTime = 0;
}

void HomeAssistantManager::init(LGFX* tft, IUIUpdater* uiUpdater, ConfigManager* config, const char* firmware_version) {
    _tft = tft;
    _uiUpdater = uiUpdater;
    _config = config;
    
    const char* const entity_category_diagnostic = "diagnostic";
    const char* const entity_category_config = "config";
    
    _device.setManufacturer("Guition");
    _device.setModel("ESP32-2432S022C");
    _device.setName("HVAC Sensor Display");
    _device.setSoftwareVersion(firmware_version);
    
    _pressureSensor.setName("Differential Pressure");
    _pressureSensor.setDeviceClass("pressure");
    _pressureSensor.setUnitOfMeasurement("Pa");
    _pressureSensor.setIcon("mdi:gauge");
    _pressureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _temperatureSensor.setName("Temperature");
    _temperatureSensor.setDeviceClass("temperature");
    _temperatureSensor.setUnitOfMeasurement("°C");
    _temperatureSensor.setIcon("mdi:thermometer");
    _temperatureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _humiditySensor.setName("Humidity");
    _humiditySensor.setDeviceClass("humidity");
    _humiditySensor.setUnitOfMeasurement("%");
    _humiditySensor.setIcon("mdi:water-percent");
    _humiditySensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _currentSensor.setName("Fan Current");
    _currentSensor.setDeviceClass("current");
    _currentSensor.setUnitOfMeasurement("A");
    _currentSensor.setIcon("mdi:current-ac");
    _currentSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _wifi_rssi.setName("WiFi RSSI");
    _wifi_rssi.setDeviceClass("signal_strength");
    _wifi_rssi.setUnitOfMeasurement("dBm");
    _wifi_rssi.setIcon("mdi:wifi");
    _wifi_rssi.setEntityCategory(entity_category_diagnostic);

    _wifi_ssid.setName("WiFi SSID");
    _wifi_ssid.setIcon("mdi:wifi-check");
    _wifi_ssid.setEntityCategory(entity_category_diagnostic);
    
    _wifi_ip.setName("WiFi IP Address");
    _wifi_ip.setIcon("mdi:ip-network");
    _wifi_ip.setEntityCategory(entity_category_diagnostic);
    
    _geiger_cpm.setName("Geiger CPM");
    _geiger_cpm.setUnitOfMeasurement("CPM");
    _geiger_cpm.setIcon("mdi:radioactive");
    _geiger_cpm.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _geiger_dose.setName("Geiger Dose Rate");
    _geiger_dose.setUnitOfMeasurement("µSv/h");
    _geiger_dose.setIcon("mdi:radioactive");
    _geiger_dose.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _backlight.setName("Display Backlight");
    _backlight.onStateCommand(onStateCommand);
    _backlight.onBrightnessCommand(onBrightnessCommand);
    
    _sensorStatus.setName("Sensor Stack Connection");
    _sensorStatus.setDeviceClass("connectivity");
    _sensorStatus.setEntityCategory(entity_category_diagnostic);
    
    _highPressureSensor.setName("High Pressure Alert");
    _highPressureSensor.setDeviceClass("problem");
    _highPressureSensor.setIcon("mdi:alert");
    _highPressureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _fanStatus.setName("Fan Status");
    _fanStatus.setDeviceClass("power");
    _fanStatus.setIcon("mdi:fan");
    _fanStatus.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    const char* pm_unit = "µg/m³";
    const char* pm_icon = "mdi:blur";
    _pm1_0_sensor.setName("PM 1.0");
    _pm1_0_sensor.setDeviceClass("pm1");
    _pm1_0_sensor.setUnitOfMeasurement(pm_unit);
    _pm1_0_sensor.setIcon(pm_icon);
    _pm1_0_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _pm2_5_sensor.setName("PM 2.5");
    _pm2_5_sensor.setDeviceClass("pm25");
    _pm2_5_sensor.setUnitOfMeasurement(pm_unit);
    _pm2_5_sensor.setIcon(pm_icon);
    _pm2_5_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _pm4_0_sensor.setName("PM 4.0");
    _pm4_0_sensor.setDeviceClass("pm10");
    _pm4_0_sensor.setUnitOfMeasurement(pm_unit);
    _pm4_0_sensor.setIcon(pm_icon);
    _pm4_0_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _pm10_0_sensor.setName("PM 10.0");
    _pm10_0_sensor.setDeviceClass("pm10");
    _pm10_0_sensor.setUnitOfMeasurement(pm_unit);
    _pm10_0_sensor.setIcon(pm_icon);
    _pm10_0_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _co2_sensor.setName("Carbon Dioxide");
    _co2_sensor.setDeviceClass("carbon_dioxide");
    _co2_sensor.setUnitOfMeasurement("ppm");
    _co2_sensor.setIcon("mdi:molecule-co2");
    _co2_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _voc_index_sensor.setName("VOC Index");
    _voc_index_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    _voc_index_sensor.setIcon("mdi:lungs");
    
    _sensorStackUptimeSensor.setName("Sensor Stack Uptime");
    _sensorStackUptimeSensor.setIcon("mdi:timer-sand");
    _sensorStackUptimeSensor.setUnitOfMeasurement("s");
    _sensorStackUptimeSensor.setEntityCategory(entity_category_diagnostic);
    _sensorStackUptimeSensor.setValue(static_cast<uint32_t>(0)); // Initialize to 0

    _sensorStackVersionSensor.setName("Sensor Stack Firmware Version");
    _sensorStackVersionSensor.setIcon("mdi:chip");
    _sensorStackVersionSensor.setEntityCategory(entity_category_diagnostic);
    _sensorStackVersionSensor.setValue(nullptr); // Set to unavailable on boot

    _sensorStackFreeRamSensor.setName("Sensor Stack Free RAM");
    _sensorStackFreeRamSensor.setIcon("mdi:memory");
    _sensorStackFreeRamSensor.setUnitOfMeasurement("B");
    _sensorStackFreeRamSensor.setEntityCategory(entity_category_diagnostic);
    // Set expire time to > 2x the poll interval (30s * 2 + 5s buffer)
    _sensorStackFreeRamSensor.setExpireAfter(65);

    _sensorStackResetCauseSensor.setName("Nano Reset Cause");
    _sensorStackResetCauseSensor.setIcon("mdi:restart-alert");
    _sensorStackResetCauseSensor.setEntityCategory(entity_category_diagnostic);
    _sensorStackResetCauseSensor.setExpireAfter(65);

    _rebootButton.setName("Reboot Device");
    _rebootButton.setIcon("mdi:restart");
    _rebootButton.setEntityCategory(entity_category_config);
    _rebootButton.onCommand(onRebootCommand);

    _rebootSensorStackButton.setName("Reboot Sensor");
    _rebootSensorStackButton.setIcon("mdi:restart-alert");
    _rebootSensorStackButton.setEntityCategory(entity_category_config);
    _rebootSensorStackButton.onCommand(onRebootSensorStackCommand);

    _getSps30InfoButton.setName("SPS30 Get Info");
    _getSps30InfoButton.setIcon("mdi:information");
    _getSps30InfoButton.setEntityCategory(entity_category_diagnostic);
    _getSps30InfoButton.onCommand(onGetSps30InfoCommand);

    _Sps30ManualCleanButton.setName("SPS30 Manual Fan Clean");
    _Sps30ManualCleanButton.setIcon("mdi:fan-alert");
    _Sps30ManualCleanButton.setEntityCategory(entity_category_diagnostic);
    _Sps30ManualCleanButton.onCommand(onGetSps30ManualCleanCommand);

    _getSgp40SelftestButton.setName("SGP40 Self-Test");
    _getSgp40SelftestButton.setIcon("mdi:information");
    _getSgp40SelftestButton.setEntityCategory(entity_category_diagnostic);
    _getSgp40SelftestButton.onCommand(onGetSgp40SelftestCommand);

    _logLevelSelect.setName("Log Level");
    _logLevelSelect.setIcon("mdi:message-text");
    _logLevelSelect.setEntityCategory(entity_category_config);
    _logLevelSelect.setOptions("Debug;Info;Warning;Error"); // must match order of AppLogLevel enum
    _logLevelSelect.onCommand(onLogLevelCommand);
    _logLevelSelect.setCurrentState(_config->getLogLevel());

    _scd30AutoCalSwitch.setName("SCD30 Auto Calibration");
    _scd30AutoCalSwitch.setIcon("mdi:tune");
    _scd30AutoCalSwitch.setEntityCategory(entity_category_config);
    _scd30AutoCalSwitch.onCommand(onScd30AutoCalCommand);

    _scd30ForceCalNumber.setName("SCD30 Force Calibration");
    _scd30ForceCalNumber.setIcon("mdi:tune-variant");
    _scd30ForceCalNumber.setEntityCategory(entity_category_config);
    _scd30ForceCalNumber.setUnitOfMeasurement("ppm");
    _scd30ForceCalNumber.setMin(400);
    _scd30ForceCalNumber.setMax(2000);
    _scd30ForceCalNumber.setStep(1);
    _scd30ForceCalNumber.onCommand(onScd30ForceCalCommand);

    _device.enableSharedAvailability();
    _device.enableLastWill();


    _mqtt.onConnected(onMqttConnected);
    _mqtt.onDisconnected(onMqttDisconnected);
    _mqtt.begin(MQTT_HOST, MQTT_USER, MQTT_PASSWORD);
}

HAMqtt* HomeAssistantManager::getMqtt() {
    return &_mqtt;
}

bool HomeAssistantManager::isMqttConnected() {
    return _mqtt.isConnected();
}

void HomeAssistantManager::loop() {
    _mqtt.loop();
}

void HomeAssistantManager::onMqttConnected() {
    if (!_instance) return;
    logger.info("MQTT connected.");
    if (_instance->_uiUpdater) _instance->_uiUpdater->update_ha_status(true);


    _instance->_wifi_ssid.setValue(WIFI_SSID);
    _instance->_wifi_ip.setValue(WiFi.localIP().toString().c_str());

    uint8_t current_brightness = _instance->_tft->getBrightness();
    _instance->_backlight.setState(current_brightness > 0, current_brightness);
}

void HomeAssistantManager::onMqttDisconnected() {
    if (!_instance) return;
    logger.warning("MQTT disconnected.");
    if (_instance->_uiUpdater) _instance->_uiUpdater->update_ha_status(false);
}

void HomeAssistantManager::publishSensorData(
    float pressure, int cpm, float temp, float humi,
    float co2, int32_t voc_index, float amps,
    float pm1, float pm25, float pm4, float pm10
) {
    unsigned long currentTime = millis();
    char data_str[10];

    if (fabs(pressure - _lastPublishedPressure) > 0.05f || (currentTime - _lastPressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(pressure, 4, 1, data_str);
        _pressureSensor.setValue(data_str);
        _lastPublishedPressure = pressure;
        _lastPressurePublishTime = currentTime;
    }
    if (cpm != _lastPublishedCpm || (currentTime - _lastCpmPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        itoa(cpm, data_str, 10);
        _geiger_cpm.setValue(data_str);
        float usv_h = cpm * 0.0057;
        dtostrf(usv_h, 4, 2, data_str);
        _geiger_dose.setValue(data_str);
        _lastPublishedCpm = cpm;
        _lastCpmPublishTime = currentTime;
    }
    if (fabs(temp - _lastPublishedTemp) > 0.05f || (currentTime - _lastTempPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(temp, 4, 1, data_str);
        _temperatureSensor.setValue(data_str);
        _lastPublishedTemp = temp;
        _lastTempPublishTime = currentTime;
    }
    if (fabs(humi - _lastPublishedHumi) > 0.5f || (currentTime - _lastHumiPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(humi, 4, 0, data_str);
        _humiditySensor.setValue(data_str);
        _lastPublishedHumi = humi;
        _lastHumiPublishTime = currentTime;
    }
     if (fabs(amps - _lastPublishedAmps) > 0.01f || (currentTime - _lastAmpsPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(amps, 4, 2, data_str);
        _currentSensor.setValue(data_str);
        _lastPublishedAmps = amps;
        _lastAmpsPublishTime = currentTime;
    }
    if (fabs(co2 - _lastPublishedCo2) > 1.0f || (currentTime - _lastCo2PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(co2, 4, 0, data_str);
        _co2_sensor.setValue(data_str);
        _lastPublishedCo2 = co2;
        _lastCo2PublishTime = currentTime;
    }
    if (voc_index != _lastPublishedVocIndex || (currentTime - _lastVocPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        itoa(voc_index, data_str, 10);
        _voc_index_sensor.setValue(data_str);
        _lastPublishedVocIndex = voc_index;
        _lastVocPublishTime = currentTime;
    }
    if (fabs(pm1 - _lastPublishedPm1_0) > 0.1f || (currentTime - _lastPmPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(pm1, 4, 1, data_str); _pm1_0_sensor.setValue(data_str);
        dtostrf(pm25, 4, 1, data_str); _pm2_5_sensor.setValue(data_str);
        dtostrf(pm4, 4, 1, data_str); _pm4_0_sensor.setValue(data_str);
        dtostrf(pm10, 4, 1, data_str); _pm10_0_sensor.setValue(data_str);

        _lastPublishedPm1_0 = pm1; _lastPublishedPm2_5 = pm25;
        _lastPublishedPm4_0 = pm4; _lastPublishedPm10_0 = pm10;
        _lastPmPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishWiFiStatus(bool connected, long rssi, const char* ssid, const char* ip) {
    unsigned long currentTime = millis();
    if (connected != _lastPublishedWifiConnected || rssi != _lastPublishedWifiRssi || (currentTime - _lastWifiStatusPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        if (connected) {
            char rssi_str[12];
            itoa(rssi, rssi_str, 10);
            _wifi_rssi.setValue(rssi_str);
        } else {
            _wifi_rssi.setValue("0");
        }
        _lastPublishedWifiConnected = connected;
        _lastPublishedWifiRssi = rssi;
        _lastWifiStatusPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishSensorConnectionStatus(bool connected) {
    unsigned long currentTime = millis();
    if (connected != _lastPublishedSensorConnected || (currentTime - _lastSensorStatusPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _sensorStatus.setState(connected);
        _lastPublishedSensorConnected = connected;
        _lastSensorStatusPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishHighPressureStatus(bool is_high) {
    unsigned long currentTime = millis();
    if (is_high != _lastPublishedHighPressure || (currentTime - _lastHighPressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _highPressureSensor.setState(is_high, true);
        _lastPublishedHighPressure = is_high;
        _lastHighPressurePublishTime = currentTime;
    }
}

void HomeAssistantManager::publishFanStatus(bool is_on) {
    unsigned long currentTime = millis();
    if (is_on != _lastPublishedFanStatus || (currentTime - _lastFanStatusPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _fanStatus.setState(is_on, true);
        _lastPublishedFanStatus = is_on;
        _lastFanStatusPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishSensorStackVersion(const char* version) {
    if (version == nullptr) return;

    if (_lastPublishedSensorStackVersion.equals(version)) {
        return; // No change, do not publish
    }

    logger.infof("Received SensorStack Version: %s", version);
    _sensorStackVersionSensor.setValue(version);
    _lastPublishedSensorStackVersion = version;
}

void HomeAssistantManager::setSensorStackVersionUnavailable() {
    if (_lastPublishedSensorStackVersion.equals("unavailable")) {
        return; // Already unavailable
    }
    _sensorStackVersionSensor.setValue(nullptr); // This sends "none" which HA interprets as unavailable
    _lastPublishedSensorStackVersion = "unavailable";
}

void HomeAssistantManager::publishSensorStackUptime(uint32_t uptime_seconds, bool force) {
    uint32_t currentTime = millis();

    // The check below ensures we either publish on a regular interval, or when forced.
    // The underlying ArduinoHA library's setValue() method is smart and will only
    // actually send an MQTT message if the value has changed, preventing spam.
    if (force || (currentTime - _lastSensorStackUptimePublishTime >= FORCE_PUBLISH_INTERVAL_MS)) {
        _sensorStackUptimeSensor.setValue(uptime_seconds);
        _lastSensorStackUptimePublishTime = currentTime;

        if (force) {
            logger.debugf("Forced publish of SensorStack Uptime: %lu s", uptime_seconds);
        }
    }
}

void HomeAssistantManager::publishSensorStackFreeRam(uint16_t free_ram) {
    _sensorStackFreeRamSensor.setValue(free_ram, true);
}

void HomeAssistantManager::onRebootCommand(HAButton* sender) {
    logger.warning("Reboot command received from Home Assistant. Rebooting now.");
    sender->setAvailability(false);
    delay(500); 
    ESP.restart();
}

void HomeAssistantManager::onRebootSensorStackCommand(HAButton* sender) {
    logger.warning("Reboot command sent to SensorStack from Home Assistant.");
    send_command_to_nano(CMD_REBOOT);
}

void HomeAssistantManager::onGetSps30InfoCommand(HAButton* sender) {
    logger.info("Get SPS30 Info command sent to SensorStack from Home Assistant.");
    send_command_to_nano(CMD_GET_SPS30_INFO);
}

void HomeAssistantManager::onGetSps30ManualCleanCommand(HAButton* sender) {
    logger.info("Get SPS30 Manual Fan Cleaning request sent to SensorStack from Home Assistant.");
    send_command_to_nano(CMD_SPS30_CLEAN);
}

void HomeAssistantManager::onGetSgp40SelftestCommand(HAButton* sender) {
    logger.info("SGP40 Self-Test command sent to SensorStack from Home Assistant.");
    send_command_to_nano(CMD_SGP40_TEST);
}

void HomeAssistantManager::onLogLevelCommand(int8_t index, HASelect* sender) {
    if (!_instance) return;
    AppLogLevel level = static_cast<AppLogLevel>(index);
    _instance->_config->setLogLevel(level);
    logger.setLogLevel(level);
    sender->setState(index);
}

void HomeAssistantManager::resetSensorStackUptimePublishTime() {
    _lastSensorStackUptimePublishTime = 0; // Force immediate publish on next loop
}

void HomeAssistantManager::onBrightnessCommand(uint8_t brightness, HALight* sender) {
    if (!_instance) return;
    _instance->_tft->setBrightness(brightness);
    sender->setBrightness(brightness);
    if (brightness > 0 && !sender->getCurrentState()) {
        sender->setState(true);
    }
}

void HomeAssistantManager::onStateCommand(bool state, HALight* sender) {
    if (!_instance) return;
    if (state) {
        uint8_t brightness = sender->getCurrentBrightness();
        if (brightness == 0) brightness = 255; 
        _instance->_tft->setBrightness(brightness);
    } else {
        _instance->_tft->setBrightness(0);
    }
    sender->setState(state);
}

void HomeAssistantManager::publishNanoResetCause(const char* cause) {
    _sensorStackResetCauseSensor.setValue(cause);
}

void HomeAssistantManager::onScd30AutoCalCommand(bool state, HASwitch* sender) {
    logger.infof("SCD30 AutoCalibration %s command from Home Assistant.", state ? "ON" : "OFF");
    // Use the existing send_command_to_nano function with parameter
    char cmd_with_param[3];
    cmd_with_param[0] = CMD_SET_SCD30_AUTOCAL;
    cmd_with_param[1] = state ? '1' : '0';
    cmd_with_param[2] = '\0';
    
    uint8_t crc = 0x00;
    uint8_t polynomial = 0x07;
    for (int i = 0; i < 2; i++) {
        crc ^= cmd_with_param[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (crc << 1) ^ polynomial : (crc << 1);
        }
    }
    
    Serial.print('<'); Serial.print(cmd_with_param); Serial.print(','); Serial.print(crc); Serial.print('>');
}

void HomeAssistantManager::updateScd30AutoCalState(bool state) {
    _scd30AutoCalSwitch.setState(state);
}

void HomeAssistantManager::onScd30ForceCalCommand(HANumeric number, HANumber* sender) {
    uint16_t ppm_value = (uint16_t)number.toUInt16();
    logger.infof("SCD30 Force Calibration to %u ppm command from Home Assistant.", ppm_value);
    
    char cmd_with_param[8];
    sprintf(cmd_with_param, "%c%u", CMD_SET_SCD30_FORCECAL, ppm_value);
    
    uint8_t crc = 0x00;
    uint8_t polynomial = 0x07;
    for (int i = 0; cmd_with_param[i] != '\0'; i++) {
        crc ^= cmd_with_param[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (crc << 1) ^ polynomial : (crc << 1);
        }
    }
    
    Serial.print('<'); Serial.print(cmd_with_param); Serial.print(','); Serial.print(crc); Serial.print('>');
}

void HomeAssistantManager::updateScd30ForceCalValue(uint16_t value) {
    _scd30ForceCalNumber.setState(value);
}

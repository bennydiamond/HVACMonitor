#include "HomeAssistantManager.h"
#include <math.h>
#include "LGFX_ESP32_2432S022C.h"
#include "ui/interfaces/IUIUpdater.h"
#include "secrets.h" 
#include "Logger.h"
#include "NanoCommands.h"
#include "ConfigManager.h"
#include "UITask.h"

const unsigned long FORCE_PUBLISH_INTERVAL_MS = 20000;
const unsigned long SENSOR_EXPIRE_TIMEOUT_S = 30;
#define RANDOM_SUFFIX "_HVAC_MON_01"

HomeAssistantManager* HomeAssistantManager::_instance = nullptr;

HomeAssistantManager::HomeAssistantManager() :
    _device(MQTT_DEVICE_ID),
    _mqtt(_wifiClient, _device),
    _pressureSensor("pressure" RANDOM_SUFFIX, HASensor::PrecisionP1),
    _backlight("backlight" RANDOM_SUFFIX, HALight::BrightnessFeature),
    _wifi_rssi("wifi_rssi" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _wifi_ssid("wifi_ssid" RANDOM_SUFFIX),
    _wifi_ip("wifi_ip" RANDOM_SUFFIX),
    _geiger_cpm("geiger_cpm" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _geiger_dose("geiger_dose" RANDOM_SUFFIX, HASensor::PrecisionP2),
    _temperatureSensor("temperature" RANDOM_SUFFIX, HASensor::PrecisionP1),
    _humiditySensor("humidity" RANDOM_SUFFIX, HASensor::PrecisionP1),
#ifdef BMP280_ENABLED
    _bmp280PressureSensor("bmp280_pressure" RANDOM_SUFFIX, HASensorNumber::PrecisionP1),
    _bmp280TemperatureSensor("bmp280_temperature" RANDOM_SUFFIX, HASensorNumber::PrecisionP1),
#endif
#ifdef AHT20_ENABLED
    _aht20TemperatureSensor("aht20_temperature", HASensorNumber::PrecisionP1),
    _aht20HumiditySensor("aht20_humidity", HASensorNumber::PrecisionP1),
#endif
    _sensorStatus("sensor_status" RANDOM_SUFFIX),
    _highPressureSensor("high_pressure_status" RANDOM_SUFFIX),
    _fanStatus("fan_status" RANDOM_SUFFIX),
    _rebootButton("reboot" RANDOM_SUFFIX),
    _rebootSensorStackButton("reboot_sensorStack" RANDOM_SUFFIX),
    _pm1_0_sensor("pm1_0" RANDOM_SUFFIX, HASensor::PrecisionP1),
    _pm2_5_sensor("pm2_5" RANDOM_SUFFIX, HASensor::PrecisionP1),
    _pm4_0_sensor("pm4_0" RANDOM_SUFFIX, HASensor::PrecisionP1),
    _pm10_0_sensor("pm10_0" RANDOM_SUFFIX, HASensor::PrecisionP1),
    _o3_sensor("o3" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
    _no2_sensor("no2" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
    _fast_aqi_sensor("fast_aqi" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
    _epa_aqi_sensor("epa_aqi" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
    _co2_sensor("co2" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _co_sensor("co" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _sensorStackUptimeSensor("nano_uptime" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _sensorStackVersionSensor("nano_firmware_version" RANDOM_SUFFIX), 
    _sensorStackFreeRamSensor("nano_free_ram" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _voc_index_sensor("voc_index" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _nox_index_sensor("nox_index" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _currentSensor("current" RANDOM_SUFFIX, HASensor::PrecisionP2),
    _sensorStackResetCauseSensor("nano_reset_cause" RANDOM_SUFFIX),
    _getSps30InfoButton("get_sps30_info" RANDOM_SUFFIX),
    _Sps30ManualCleanButton("sps30_manual_clean" RANDOM_SUFFIX),
    _getSgp41SelftestButton("get_sgp41_selftest" RANDOM_SUFFIX),
    _logLevelSelect("log_level" RANDOM_SUFFIX),
    _scd30AutoCalSwitch("scd30_autocal" RANDOM_SUFFIX),
    _scd30ForceCalNumber("scd30_forcecal" RANDOM_SUFFIX, HANumber::PrecisionP0),
    _esp32FreeRamSensor("esp32_free_ram" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _esp32UptimeSensor("esp32_uptime" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
    _compressorCurrentSensor("compressor_current" RANDOM_SUFFIX, HASensorNumber::PrecisionP2),
    _geothermalPumpCurrentSensor("geothermal_pump_current" RANDOM_SUFFIX, HASensorNumber::PrecisionP2),
    _liquidLevelSensor("liquid_level_sensor" RANDOM_SUFFIX),
    _inactivityTimerDelay("inactivity_timer_delay" RANDOM_SUFFIX, HANumber::PrecisionP0)
{
    _instance = this;

    // Initialize state tracking variables
    _lastPublishedPressure = -9999.0f;
    _lastPublishedCpm = -1;
    _lastPublishedTemp = -9999.0f;
    _lastPublishedHumi = -1.0f;
    _lastPublishedCo2 = -1.0f;
    _lastPublishedCo = -1.0f;  // Initialize CO tracking
    _lastPublishedVocIndex = -1;
    _lastPublishedNOxIndex = -1;
    _lastPublishedAmps = -1.0f;
#ifdef BMP280_ENABLED
    _lastPublishedBMP280Pressure = -9999.0f;
    _lastPublishedBMP280Temperature = -9999.0f;
#endif
#ifdef AHT20_ENABLED
    _lastPublishedAHT20Temperature = -9999.0f;
    _lastPublishedAHT20Humidity = -1.0f;
#endif
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
    _lastPublishedO3 = -9999.0f;
    _lastPublishedNO2 = -9999.0f;
    _lastPublishedFastAQI = UINT16_MAX;
    _lastPublishedEPAAQI = UINT16_MAX;

    _lastPressurePublishTime = 0;
    _lastCpmPublishTime = 0;
    _lastTempPublishTime = 0;
    _lastHumiPublishTime = 0;
    _lastWifiStatusPublishTime = 0;
    _lastSensorStatusPublishTime = 0;
    _lastHighPressurePublishTime = 0;
    _lastPmPublishTime = 0;
    _LastO3PublishTime = 0;
    _lastNO2PublishTime = 0;
    _lastFastAQIPublishTime = 0;
    _lastEPAAQIPublishTime = 0;
    _lastCo2PublishTime = 0;
    _lastCoPublishTime = 0;  // Initialize CO publish time
    _lastVocPublishTime = 0;
    _lastNOxPublishTime = 0;
    _lastAmpsPublishTime = 0;
    _lastFanStatusPublishTime = 0;
#ifdef BMP280_ENABLED
    _lastBMP280PressurePublishTime = 0;
    _lastBMP280TemperaturePublishTime = 0;
#endif
#ifdef AHT20_ENABLED
    _lastAHT20TemperaturePublishTime = 0;
    _lastAHT20HumidityPublishTime = 0;
#endif
    _lastCompressorAmpsPublishTime = 0;
    _lastGeothermalPumpAmpsPublishTime = 0;
    _lastLiquidLevelPublishTime = 0;
    _lastInactivityTimerDelayPublishTime = 0;
}

HomeAssistantManager::~HomeAssistantManager() {

}

void HomeAssistantManager::init(const char* firmware_version) {
    const char* const entity_category_diagnostic = "diagnostic";
    const char* const entity_category_config = "config";

    ConfigManagerAccessor config;
    
    _device.setManufacturer("Guition");
    _device.setModel("ESP32-2432S022C");
    
    // Safely set device name with fallback
    const char* deviceName = config->getMqttDeviceName();
    if (deviceName != nullptr && strlen(deviceName) > 0) {
        logger.infof("Setting device name: %s", deviceName);
        _device.setName(deviceName);
    } else {
        logger.warningf("Invalid device name, using default");
        _device.setName("HVAC Sensor Display");
    }
    
    _device.setSoftwareVersion(firmware_version);
    
    // Safely set device ID with fallback
    const char* deviceId = config->getMqttDeviceId();
    if (deviceId != nullptr && strlen(deviceId) > 0) {
        logger.infof("Setting device ID: %s", deviceId);
        char mqtt_device_id[32];
        snprintf(mqtt_device_id, sizeof(mqtt_device_id), "%s", deviceId);
        _device.setUniqueId(reinterpret_cast<const byte*>(mqtt_device_id), strlen(mqtt_device_id));
    } else {
        logger.warningf("Invalid device ID, using default");
        _device.setUniqueId(reinterpret_cast<const byte*>("hvac_diff_pressure_sensor_01"), strlen("hvac_diff_pressure_sensor_01"));
    }
    
    _pressureSensor.setName("Differential Pressure");
    _pressureSensor.setDeviceClass("pressure");
    _pressureSensor.setUnitOfMeasurement("Pa");
    _pressureSensor.setIcon("mdi:gauge");
    _pressureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
#ifdef BMP280_ENABLED
    _bmp280PressureSensor.setName("BMP280 Pressure");
    _bmp280PressureSensor.setDeviceClass("pressure");
    _bmp280PressureSensor.setUnitOfMeasurement("Pa");
    _bmp280PressureSensor.setIcon("mdi:gauge");
    _bmp280PressureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _bmp280TemperatureSensor.setName("BMP280 Temperature");
    _bmp280TemperatureSensor.setDeviceClass("temperature");
    _bmp280TemperatureSensor.setUnitOfMeasurement("°C");
    _bmp280TemperatureSensor.setIcon("mdi:thermometer");
    _bmp280TemperatureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
#endif
    
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
    
#ifdef AHT20_ENABLED
    _aht20TemperatureSensor.setName("AHT20 Temperature");
    _aht20TemperatureSensor.setDeviceClass("temperature");
    _aht20TemperatureSensor.setUnitOfMeasurement("°C");
    _aht20TemperatureSensor.setIcon("mdi:thermometer");
    _aht20TemperatureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _aht20HumiditySensor.setName("AHT20 Humidity");
    _aht20HumiditySensor.setDeviceClass("humidity");
    _aht20HumiditySensor.setUnitOfMeasurement("%");
    _aht20HumiditySensor.setIcon("mdi:water-percent");
    _aht20HumiditySensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
#endif
    _currentSensor.setName("Fan Current");
    _currentSensor.setDeviceClass("current");
    _currentSensor.setUnitOfMeasurement("A");
    _currentSensor.setIcon("mdi:current-ac");
    _currentSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _compressorCurrentSensor.setName("Compressor Current");
    _compressorCurrentSensor.setDeviceClass("current");
    _compressorCurrentSensor.setUnitOfMeasurement("A");
    _compressorCurrentSensor.setIcon("mdi:current-ac");
    _compressorCurrentSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _geothermalPumpCurrentSensor.setName("Geothermal Pump Current");
    _geothermalPumpCurrentSensor.setDeviceClass("current");
    _geothermalPumpCurrentSensor.setUnitOfMeasurement("A");
    _geothermalPumpCurrentSensor.setIcon("mdi:current-ac");
    _geothermalPumpCurrentSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _liquidLevelSensor.setName("Liquid Level Sensor");
    _liquidLevelSensor.setDeviceClass("moisture");
    _liquidLevelSensor.setIcon("mdi:water");
    _liquidLevelSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

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
    
    _sensorStatus.setName("SensorStack Connection");
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

    _o3_sensor.setName("Ozone (O3)");
    _o3_sensor.setDeviceClass("ozone");
    _o3_sensor.setUnitOfMeasurement("µg/m³");
    _o3_sensor.setIcon("mdi:chemical-weapon");
    _o3_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _no2_sensor.setName("Nitrogen Dioxide (NO2)");
    _no2_sensor.setDeviceClass("nitrogen_dioxide");
    _no2_sensor.setUnitOfMeasurement("µg/m³");
    _no2_sensor.setIcon("mdi:smog");
    _no2_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _fast_aqi_sensor.setName("Fast AQI");
    _fast_aqi_sensor.setDeviceClass("aqi");
    _fast_aqi_sensor.setIcon("mdi:air-filter");
    _fast_aqi_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

    _epa_aqi_sensor.setName("EPA AQI");
    _epa_aqi_sensor.setDeviceClass("aqi");
    _epa_aqi_sensor.setIcon("mdi:air-filter");
    _epa_aqi_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _co2_sensor.setName("Carbon Dioxide");
    _co2_sensor.setDeviceClass("carbon_dioxide");
    _co2_sensor.setUnitOfMeasurement("ppm");
    _co2_sensor.setIcon("mdi:molecule-co2");
    _co2_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _co_sensor.setName("Carbon Monoxide");
    _co_sensor.setDeviceClass("carbon_monoxide");
    _co_sensor.setUnitOfMeasurement("ppm");
    _co_sensor.setIcon("mdi:molecule-co");
    _co_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
    _voc_index_sensor.setName("VOC Index");
    _voc_index_sensor.setUnitOfMeasurement("");
    _voc_index_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    _voc_index_sensor.setIcon("mdi:lungs");

    _nox_index_sensor.setName("NOx Index");
    _nox_index_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    _nox_index_sensor.setIcon("mdi:smog");
    
    _sensorStackUptimeSensor.setName("SensorStack Uptime");
    _sensorStackUptimeSensor.setIcon("mdi:timer-sand");
    _sensorStackUptimeSensor.setUnitOfMeasurement("s");
    _sensorStackUptimeSensor.setEntityCategory(entity_category_diagnostic);
    _sensorStackUptimeSensor.setValue(static_cast<uint32_t>(0));

    _sensorStackVersionSensor.setName("SensorStack Firmware Version");
    _sensorStackVersionSensor.setIcon("mdi:chip");
    _sensorStackVersionSensor.setEntityCategory(entity_category_diagnostic);
    _sensorStackVersionSensor.setValue(nullptr); // Set to unavailable on boot

    _sensorStackFreeRamSensor.setName("SensorStack Free RAM");
    _sensorStackFreeRamSensor.setIcon("mdi:memory");
    _sensorStackFreeRamSensor.setUnitOfMeasurement("B");
    _sensorStackFreeRamSensor.setEntityCategory(entity_category_diagnostic);
    // Set expire time to > 2x the poll interval (30s * 2 + 5s buffer)
    _sensorStackFreeRamSensor.setExpireAfter(65);

    _sensorStackResetCauseSensor.setName("SensorStack Reset Cause");
    _sensorStackResetCauseSensor.setIcon("mdi:restart-alert");
    _sensorStackResetCauseSensor.setEntityCategory(entity_category_diagnostic);
    _sensorStackResetCauseSensor.setExpireAfter(65);

    _rebootButton.setName("Reboot Device");
    _rebootButton.setIcon("mdi:restart");
    _rebootButton.setEntityCategory(entity_category_config);
    _rebootButton.onCommand(onRebootCommand);

    _rebootSensorStackButton.setName("SensorStack Reboot");
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

    _getSgp41SelftestButton.setName("SGP41 Self-Test");
    _getSgp41SelftestButton.setIcon("mdi:information");
    _getSgp41SelftestButton.setEntityCategory(entity_category_diagnostic);
    _getSgp41SelftestButton.onCommand(onGetSgp41SelftestCommand);

    _logLevelSelect.setName("Log Level");
    _logLevelSelect.setIcon("mdi:message-text");
    _logLevelSelect.setEntityCategory(entity_category_config);
    _logLevelSelect.setOptions("Debug;Info;Warning;Error"); // must match order of AppLogLevel enum
    _logLevelSelect.onCommand(onLogLevelCommand);
    _logLevelSelect.setCurrentState(config->getLogLevel());

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
    
    _inactivityTimerDelay.setName("Inactivity Timer Delay");
    _inactivityTimerDelay.setIcon("mdi:timer");
    _inactivityTimerDelay.setEntityCategory(entity_category_config);
    _inactivityTimerDelay.setUnitOfMeasurement("s");
    _inactivityTimerDelay.setMin(10);
    _inactivityTimerDelay.setMax(120);
    _inactivityTimerDelay.setStep(5);
    _inactivityTimerDelay.onCommand(onInactivityTimerDelayCommand);

    _esp32FreeRamSensor.setName("ESP32 Free RAM");
    _esp32FreeRamSensor.setIcon("mdi:memory");
    _esp32FreeRamSensor.setUnitOfMeasurement("B");
    _esp32FreeRamSensor.setEntityCategory(entity_category_diagnostic);

    _esp32UptimeSensor.setName("ESP32 Uptime");
    _esp32UptimeSensor.setIcon("mdi:timer-sand");
    _esp32UptimeSensor.setUnitOfMeasurement("s");
    _esp32UptimeSensor.setEntityCategory(entity_category_diagnostic);

    _device.enableSharedAvailability();
    _device.enableLastWill();


    _mqtt.onConnected(onMqttConnected);
    _mqtt.onDisconnected(onMqttDisconnected);
    
    // Configure MQTT client for better stability
    //_mqtt.setBufferSize(1024);
    //_mqtt.setKeepAlive(30);
    
    // Safely set up MQTT connection with fallback
    const char* mqttHost = config->getMqttHost();
    int mqttPort = config->getMqttPort();
    const char* mqttUser = config->getMqttUser();
    const char* mqttPassword = config->getMqttPassword();
    
    logger.infof("MQTT Host: %s", mqttHost ? mqttHost : "null");
    logger.infof("MQTT Port: %d", mqttPort);
    logger.infof("MQTT User: %s", mqttUser ? mqttUser : "null");
    logger.infof("MQTT Password: %s", mqttPassword ? "***" : "null");
    
    if (mqttHost != nullptr && strlen(mqttHost) > 0 && 
        mqttUser != nullptr && strlen(mqttUser) > 0 && 
        mqttPassword != nullptr && strlen(mqttPassword) > 0) {
        logger.infof("Using ConfigManager MQTT settings");
        _mqtt.begin(mqttHost, mqttPort, mqttUser, mqttPassword);
    } else {
        logger.warningf("Invalid MQTT settings, using hardcoded defaults");
        _mqtt.begin(MQTT_HOST, MQTT_USER, MQTT_PASSWORD);
    }
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
    UITask::getInstance().update_network_ha_conn(true);


    _instance->_wifi_ssid.setValue(WIFI_SSID);
    _instance->_wifi_ip.setValue(WiFi.localIP().toString().c_str());

    uint8_t current_brightness = UITask::getInstance().get_brightness();
    _instance->_backlight.setState(current_brightness > 0, true);
    _instance->_backlight.setBrightness(current_brightness, true);
    
    // Set initial state for configurable entities
    ConfigManagerAccessor config;
    _instance->_inactivityTimerDelay.setState(config->getInactivityTimerDelay());
}

void HomeAssistantManager::onMqttDisconnected() {
    if (!_instance) return;
    logger.warning("MQTT disconnected.");
    UITask::getInstance().update_network_ha_conn(false);
}

void HomeAssistantManager::publishSensorData(
    float pressure, int cpm, float temp, float humi,
    float co2, int32_t voc_index, int32_t nox_index, 
    float amps,
    float pm1, float pm25, float pm4, float pm10,
    float compressor_amps, float geothermal_pump_amps, bool liquid_level_sensor_state,
    float co_ppm
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
    if (fabs(co_ppm - _lastPublishedCo) > 0.1f || (currentTime - _lastCoPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _co_sensor.setValue(co_ppm, true);
        _lastPublishedCo = co_ppm;
        _lastCoPublishTime = currentTime;
    }
    if (voc_index != _lastPublishedVocIndex || (currentTime - _lastVocPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        itoa(voc_index, data_str, 10);
        _voc_index_sensor.setValue(data_str);
        _lastPublishedVocIndex = voc_index;
        _lastVocPublishTime = currentTime;
    }
    if (nox_index != _lastPublishedNOxIndex || (currentTime - _lastNOxPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        itoa(nox_index, data_str, 10);
        _nox_index_sensor.setValue(data_str);
        _lastPublishedNOxIndex = nox_index;
        _lastNOxPublishTime = currentTime;
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
    
    // Publish compressor current
    if (fabs(compressor_amps - _lastPublishedCompressorAmps) > 0.01f || (currentTime - _lastCompressorAmpsPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _compressorCurrentSensor.setValue(compressor_amps, true);
        _lastPublishedCompressorAmps = compressor_amps;
        _lastCompressorAmpsPublishTime = currentTime;
    }
    
    // Publish geothermal pump current
    if (fabs(geothermal_pump_amps - _lastPublishedGeothermalPumpAmps) > 0.01f || (currentTime - _lastGeothermalPumpAmpsPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _geothermalPumpCurrentSensor.setValue(geothermal_pump_amps, true);
        _lastPublishedGeothermalPumpAmps = geothermal_pump_amps;
        _lastGeothermalPumpAmpsPublishTime = currentTime;
    }
    
    // Publish liquid level sensor state
    if (liquid_level_sensor_state != _lastPublishedLiquidLevelState || (currentTime - _lastLiquidLevelPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _liquidLevelSensor.setState(liquid_level_sensor_state, true);
        _lastPublishedLiquidLevelState = liquid_level_sensor_state;
        _lastLiquidLevelPublishTime = currentTime;
    }
}

void HomeAssistantManager::publish_O3_NOx_Values(
        float o3_conc_ug_per_m3, float no2_conc_ug_per_m3, 
        uint16_t fast_aqi, uint16_t epa_aqi
    )
    {
        unsigned long currentTime = millis();

        if(o3_conc_ug_per_m3 != _lastPublishedO3 || (currentTime - _LastO3PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
            _o3_sensor.setValue(o3_conc_ug_per_m3, true);
            _lastPublishedO3 = o3_conc_ug_per_m3;
            _LastO3PublishTime = currentTime;
        }

        if(no2_conc_ug_per_m3 != _lastPublishedNO2 || (currentTime - _lastNO2PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
            _no2_sensor.setValue(no2_conc_ug_per_m3, true);
            _lastPublishedNO2 = no2_conc_ug_per_m3;
            _lastNO2PublishTime = currentTime;
        }

        if(fast_aqi != _lastPublishedFastAQI || (currentTime - _lastFastAQIPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
            _fast_aqi_sensor.setValue(fast_aqi, true);
            _lastPublishedFastAQI = fast_aqi;
            _lastFastAQIPublishTime = currentTime;
        }

        if(epa_aqi != _lastPublishedEPAAQI || (currentTime - _lastEPAAQIPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
            _epa_aqi_sensor.setValue(epa_aqi, true);
            _lastPublishedEPAAQI = epa_aqi;
            _lastEPAAQIPublishTime = currentTime;
        }
    }

#ifdef BMP280_ENABLED
void HomeAssistantManager::publishBMP280Data(float pressure_pa) {
    unsigned long currentTime = millis();
    
    if(pressure_pa != _lastPublishedBMP280Pressure || (currentTime - _lastBMP280PressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _bmp280PressureSensor.setValue(pressure_pa, true);
        _lastPublishedBMP280Pressure = pressure_pa;
        _lastBMP280PressurePublishTime = currentTime;
    }
}

void HomeAssistantManager::publishBMP280Data(float pressure_pa, float temperature_degc) {
    unsigned long currentTime = millis();
    
    // Publish pressure if changed or force interval reached
    if(pressure_pa != _lastPublishedBMP280Pressure || (currentTime - _lastBMP280PressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _bmp280PressureSensor.setValue(pressure_pa, true);
        _lastPublishedBMP280Pressure = pressure_pa;
        _lastBMP280PressurePublishTime = currentTime;
    }
    
    // Publish temperature if changed or force interval reached
    if(temperature_degc != _lastPublishedBMP280Temperature || (currentTime - _lastBMP280TemperaturePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _bmp280TemperatureSensor.setValue(temperature_degc, true);
        _lastPublishedBMP280Temperature = temperature_degc;
        _lastBMP280TemperaturePublishTime = currentTime;
    }
}
#endif

#ifdef AHT20_ENABLED
void HomeAssistantManager::publishAHT20Data(float temperature_degc, float humidity_pct) {
    unsigned long currentTime = millis();
    
    // Publish temperature if changed or force interval reached
    if(temperature_degc != _lastPublishedAHT20Temperature || (currentTime - _lastAHT20TemperaturePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _aht20TemperatureSensor.setValue(temperature_degc, true);
        _lastPublishedAHT20Temperature = temperature_degc;
        _lastAHT20TemperaturePublishTime = currentTime;
    }
    
    // Publish humidity if changed or force interval reached
    if(humidity_pct != _lastPublishedAHT20Humidity || (currentTime - _lastAHT20HumidityPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _aht20HumiditySensor.setValue(humidity_pct, true);
        _lastPublishedAHT20Humidity = humidity_pct;
        _lastAHT20HumidityPublishTime = currentTime;
    }
}
#endif

void HomeAssistantManager::publishWiFiStatus(bool connected, int8_t rssi, const char* ssid, const char* ip) {
    unsigned long currentTime = millis();
    if (connected != _lastPublishedWifiConnected || rssi != _lastPublishedWifiRssi || (currentTime - _lastWifiStatusPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        if (connected) {
            _wifi_rssi.setValue(rssi, true);
        } else {
            _wifi_rssi.setValue(0, true);
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

void HomeAssistantManager::onGetSgp41SelftestCommand(HAButton* sender) {
    logger.info("SGP41 Self-Test command sent to SensorStack from Home Assistant.");
    send_command_to_nano(CMD_SGP41_TEST);
}

void HomeAssistantManager::onLogLevelCommand(int8_t index, HASelect* sender) {
    if (!_instance) return;
    AppLogLevel level = static_cast<AppLogLevel>(index);
    {
        ConfigManagerAccessor config;
        config->setLogLevel(level);
    }
    logger.setLogLevel(level);
    sender->setState(index);
}

void HomeAssistantManager::resetSensorStackUptimePublishTime() {
    _lastSensorStackUptimePublishTime = 0; // Force immediate publish on next loop
}

void HomeAssistantManager::onBrightnessCommand(uint8_t brightness, HALight* sender) {
    if (!_instance) return;
    UITask::getInstance().set_brightness(brightness);
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
        UITask::getInstance().set_brightness(brightness);
    } else {
        UITask::getInstance().set_brightness(0);
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

void HomeAssistantManager::onInactivityTimerDelayCommand(HANumeric number, HANumber* sender) {
    if (_instance) {
        int delay = number.toInt32();
        ConfigManagerAccessor config;
        config->setInactivityTimerDelay(delay);
        logger.infof("Inactivity timer delay updated to %d seconds via Home Assistant", delay);
    }
}

void HomeAssistantManager::publishEsp32FreeRam(uint32_t free_ram) {
    _esp32FreeRamSensor.setValue(free_ram, true);
}

void HomeAssistantManager::publishEsp32Uptime(uint32_t uptime_seconds) {
    _esp32UptimeSensor.setValue(uptime_seconds);
}

void HomeAssistantManager::updateInactivityTimerDelayState() {
    unsigned long currentTime = millis();
    // Use a longer interval for configuration values that don't change frequently
    const unsigned long CONFIG_UPDATE_INTERVAL_MS = 30000; // 30 seconds
    if (currentTime - _lastInactivityTimerDelayPublishTime >= CONFIG_UPDATE_INTERVAL_MS) {
        ConfigManagerAccessor config;
        _inactivityTimerDelay.setState(config->getInactivityTimerDelay());
        _lastInactivityTimerDelayPublishTime = currentTime;
    }
}

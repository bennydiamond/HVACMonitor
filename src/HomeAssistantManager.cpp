#include "HomeAssistantManager.h"
#include <math.h>
#include "LGFX_ESP32_2432S022C.h"
#include "ui/interfaces/IUIUpdater.h"
#include "secrets.h" 
#include "Logger.h"
#include "NanoCommands.h"
#include "ConfigManager.h"
#include "UITask.h"
#include "RebootManager.h"
#include "SensorDefines.h"
#ifdef ENABLE_TASK_WATCHDOG
#include "WatchdogManager.h"
#endif

const unsigned long FORCE_PUBLISH_INTERVAL_MS = 120000;
const unsigned long SENSOR_EXPIRE_TIMEOUT_S = 300;
#define RANDOM_SUFFIX "_HVAC_MON_01"

// Publishing intervals and deadbands
namespace {
    // General intervals
    constexpr unsigned long FAST_INTERVAL_5S_MS = 5000;
    constexpr unsigned long FAST_INTERVAL_10S_MS = 10000;
    constexpr unsigned long STEADY_INTERVAL_30S_MS = 30000;
    constexpr unsigned long STEADY_INTERVAL_60S_MS = 60000;
    constexpr unsigned long WIFI_STATUS_FORCE_INTERVAL_MS = 300000;

    // Differential Pressure & Filter Pressure
    constexpr float DIFF_PRESSURE_FAST_DELTA_PA = 10.0f;
    constexpr float DIFF_PRESSURE_STEADY_DELTA_PA = 1.0f;
    constexpr float FILTER_PRESSURE_FAST_DELTA_PA = 2.0f;
    constexpr float FILTER_PRESSURE_STEADY_DELTA_PA = 0.5f;

    // Geiger Counter
    constexpr int GEIGER_CPM_SPIKE_THRESHOLD = 40;
    constexpr float GEIGER_CPM_TO_USV_H = 0.0057f;

    // Temperature & Humidity
    constexpr float TEMP_FAST_DELTA_DEG_C = 0.5f;
    constexpr float TEMP_STEADY_DELTA_DEG_C = 0.15f;
    constexpr float HUMI_FAST_DELTA_PCT = 2.0f;
    constexpr float HUMI_STEADY_DELTA_PCT = 0.5f;

    // Current sensors
    constexpr float FAN_AMPS_STEADY_DELTA_A = 0.15f;
    constexpr float COMPRESSOR_AMPS_STEADY_DELTA_A = 0.2f;
    constexpr float PUMP_AMPS_STEADY_DELTA_A = 0.15f;

    // Gas sensors
    constexpr float CO2_FAST_DELTA_PPM = 50.0f;
    constexpr float CO2_STEADY_DELTA_PPM = 15.0f;
    constexpr float CO_STEADY_DELTA_PPM = 1.0f;
    constexpr int32_t VOC_FAST_DELTA = 25;
    constexpr int32_t VOC_STEADY_DELTA = 5;
    constexpr int32_t NOX_FAST_DELTA = 5;
    constexpr int32_t NOX_STEADY_DELTA = 1;

    // Particulate Matter
    constexpr float PM_FAST_DELTA = 10.0f;
    constexpr float PM_STEADY_DELTA = 1.0f;

    // ZMOD4510 Gas & AQI
    constexpr float O3_FAST_DELTA_UG_M3 = 10.0f;
    constexpr float O3_STEADY_DELTA_UG_M3 = 2.0f;
    constexpr float NO2_FAST_DELTA_UG_M3 = 10.0f;
    constexpr float NO2_STEADY_DELTA_UG_M3 = 2.0f;
    constexpr int FAST_AQI_FAST_DELTA = 20;
    constexpr int EPA_AQI_FAST_DELTA = 20;

#ifdef BMP280_ENABLED
    constexpr float BMP280_PRESSURE_FAST_DELTA_PA = 20.0f;
    constexpr float BMP280_PRESSURE_STEADY_DELTA_PA = 5.0f;
    constexpr float BMP280_TEMP_FAST_DELTA_DEG_C = 0.5f;
    constexpr float BMP280_TEMP_STEADY_DELTA_DEG_C = 0.2f;
#endif

#ifdef AHT20_ENABLED
    constexpr float AHT20_TEMP_FAST_DELTA_DEG_C = 0.5f;
    constexpr float AHT20_TEMP_STEADY_DELTA_DEG_C = 0.2f;
    constexpr float AHT20_HUMI_FAST_DELTA_PCT = 2.0f;
    constexpr float AHT20_HUMI_STEADY_DELTA_PCT = 0.5f;
#endif

    // WiFi
    constexpr int8_t WIFI_RSSI_DELTA_DBM = 5;
}

HomeAssistantManager* HomeAssistantManager::_instance = nullptr;

HomeAssistantManager::HomeAssistantManager() :
    _device(MQTT_DEVICE_ID),
    _mqtt(_wifiClient, _device),
    _pressureSensor("pressure" RANDOM_SUFFIX, HASensor::PrecisionP1),
    _filterPressureSensor("filter_pressure" RANDOM_SUFFIX, HASensor::PrecisionP1),
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
    _compressorCurrentSensor("compressor_current" RANDOM_SUFFIX, HASensorNumber::PrecisionP2),
    _geothermalPumpCurrentSensor("geothermal_pump_current" RANDOM_SUFFIX, HASensorNumber::PrecisionP2),
    _liquidLevelSensor("liquid_level_sensor" RANDOM_SUFFIX),
    _sensorStackResetCauseSensor("nano_reset_cause" RANDOM_SUFFIX),
    _inactivityTimerDelay("inactivity_timer_delay" RANDOM_SUFFIX, HANumber::PrecisionP0),
    _getSps30InfoButton("get_sps30_info" RANDOM_SUFFIX),
    _Sps30ManualCleanButton("sps30_manual_clean" RANDOM_SUFFIX),
    _getSgp41SelftestButton("get_sgp41_selftest" RANDOM_SUFFIX),
    _logLevelSelect("log_level" RANDOM_SUFFIX),
    _scd30AutoCalSwitch("scd30_autocal" RANDOM_SUFFIX),
    _scd30ForceCalNumber("scd30_forcecal" RANDOM_SUFFIX, HANumber::PrecisionP0),
    _esp32FreeRamSensor("esp32_free_ram" RANDOM_SUFFIX, HASensor::PrecisionP0),
    _esp32UptimeSensor("esp32_uptime" RANDOM_SUFFIX, HASensorNumber::PrecisionP0)
{
    _instance = this;

    // Initialize state tracking variables
    _lastPublishedPressure = -9999.0f;
    _lastPublishedFilterPressure = -9999.0f;
    _lastFilterPressurePublishTime = 0;
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
    _lastPublishedLiquidLevelState = false;

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
    _lastSensorStackFreeRamPublishTime = 0;
    _lastEsp32FreeRamPublishTime = 0;
    _lastEsp32UptimePublishTime = 0;

    _configurationUrl[0] = '\0';
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
    
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(_configurationUrl, sizeof(_configurationUrl), "http://%s", WiFi.localIP().toString().c_str());
        _device.setConfigurationUrl(_configurationUrl);
        logger.infof("Setting device configuration URL: %s", _configurationUrl);
    }
    
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

    _filterPressureSensor.setName("Filter Differential Pressure");
    _filterPressureSensor.setDeviceClass("pressure");
    _filterPressureSensor.setUnitOfMeasurement("Pa");
    _filterPressureSensor.setIcon("mdi:air-filter");
    _filterPressureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
    
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
    _sensorStackFreeRamSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);
 
     _sensorStackResetCauseSensor.setName("SensorStack Reset Cause");
     _sensorStackResetCauseSensor.setIcon("mdi:restart-alert");
     _sensorStackResetCauseSensor.setEntityCategory(entity_category_diagnostic);
     _sensorStackResetCauseSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

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
    _scd30ForceCalNumber.setMin(SCD30_FORCECAL_MIN_PPM);
    _scd30ForceCalNumber.setMax(SCD30_FORCECAL_MAX_PPM);
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
    _mqtt.onStateChanged(onMqttStateChanged);

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
    // Without Wi-Fi the session is dead even if the client has not noticed yet
    return WiFi.status() == WL_CONNECTED && _mqtt.isConnected();
}

void HomeAssistantManager::loop() {
    // ArduinoHA retries the broker every 10 s whenever it is disconnected: only let it
    // run once Wi-Fi is up (the main loop retries Wi-Fi itself). A session lost with
    // Wi-Fi is detected and re-established here once Wi-Fi is back.
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    _mqtt.loop();
}

void HomeAssistantManager::onMqttStateChanged(HAMqtt::ConnectionState state) {
    if (!_instance) return;
    // Each attempt goes Connecting -> Connected or a failure state. Log the first
    // failure, then one reminder every 5 min (30 attempts, 10 s apart) to keep syslog readable.
    if (state == HAMqtt::StateConnected) {
        if (_instance->_mqttFailedAttempts > 0) {
            logger.infof("MQTT broker reachable again after %lu failed attempts.",
                         (unsigned long)_instance->_mqttFailedAttempts);
        }
        _instance->_mqttFailedAttempts = 0;
        return;
    }
    if (state == HAMqtt::StateConnecting || state == HAMqtt::StateDisconnected) {
        return;
    }

    uint32_t attempts = ++_instance->_mqttFailedAttempts;
    if (attempts == 1 || attempts % 30 == 0) {
        const char* reason;
        switch (state) {
            case HAMqtt::StateConnectionTimeout: reason = "no answer from broker"; break;
            case HAMqtt::StateConnectionLost:    reason = "connection lost"; break;
            case HAMqtt::StateConnectionFailed:  reason = "connection refused or broker unreachable"; break;
            case HAMqtt::StateBadProtocol:       reason = "protocol not supported"; break;
            case HAMqtt::StateBadClientId:       reason = "client ID rejected"; break;
            case HAMqtt::StateUnavailable:       reason = "broker unavailable"; break;
            case HAMqtt::StateBadCredentials:    reason = "bad username or password"; break;
            case HAMqtt::StateUnauthorized:      reason = "not authorized"; break;
            default:                             reason = "unknown error"; break;
        }
        ConfigManagerAccessor config;
        logger.warningf("MQTT connection to %s:%d failed (%s, state %d); retrying every 10 s (attempt %lu).",
                        config->getMqttHost(), config->getMqttPort(), reason, (int)state, (unsigned long)attempts);
    }
}

void HomeAssistantManager::onMqttConnected() {
    if (!_instance) return;
    logger.info("MQTT connected.");
    UITask::getInstance().update_network_ha_conn(true);


    if (_instance->_configurationUrl[0] == '\0' && WiFi.status() == WL_CONNECTED) {
        snprintf(_instance->_configurationUrl, sizeof(_instance->_configurationUrl), "http://%s", WiFi.localIP().toString().c_str());
        _instance->_device.setConfigurationUrl(_instance->_configurationUrl);
        logger.infof("Setting device configuration URL on MQTT connect: %s", _instance->_configurationUrl);
    }

    _instance->_wifi_ssid.setValue(WIFI_SSID);
    _instance->_wifi_ip.setValue(WiFi.localIP().toString().c_str());

    uint8_t current_brightness = UITask::getInstance().get_brightness();
    _instance->_backlight.setState(current_brightness > 0, true);
    _instance->_backlight.setBrightness(current_brightness, true);
    
    // Set initial state for configurable entities
    ConfigManagerAccessor config;
    _instance->_inactivityTimerDelay.setState(config->getInactivityTimerDelay());
    // The watchdog timeout covering the initial publishing is managed by the main loop
}

void HomeAssistantManager::onMqttDisconnected() {
    if (!_instance) return;
    logger.warning("MQTT disconnected.");
    UITask::getInstance().update_network_ha_conn(false);
}

void HomeAssistantManager::publishDifferentialPressure(float pressure, unsigned long currentTime) {
    char data_str[16];
    bool is_first = (_lastPressurePublishTime == 0);
    bool diff_pressure_fast = fabs(pressure - _lastPublishedPressure) >= DIFF_PRESSURE_FAST_DELTA_PA;
    bool diff_pressure_steady = (currentTime - _lastPressurePublishTime >= STEADY_INTERVAL_30S_MS && fabs(pressure - _lastPublishedPressure) >= DIFF_PRESSURE_STEADY_DELTA_PA);
    if (is_first || diff_pressure_fast || diff_pressure_steady || (currentTime - _lastPressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(pressure, 4, 1, data_str);
        _pressureSensor.setValue(data_str);
        _lastPublishedPressure = pressure;
        _lastPressurePublishTime = currentTime;
    }
}

void HomeAssistantManager::publishFilterPressure(float filter_pressure, unsigned long currentTime) {
    if (filter_pressure <= 0.0f) return;
    char data_str[16];
    bool is_first = (_lastFilterPressurePublishTime == 0);
    bool filter_p_fast = fabs(filter_pressure - _lastPublishedFilterPressure) >= FILTER_PRESSURE_FAST_DELTA_PA;
    bool filter_p_steady = (currentTime - _lastFilterPressurePublishTime >= STEADY_INTERVAL_60S_MS && fabs(filter_pressure - _lastPublishedFilterPressure) >= FILTER_PRESSURE_STEADY_DELTA_PA);
    if (is_first || filter_p_fast || filter_p_steady || (currentTime - _lastFilterPressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(filter_pressure, 4, 1, data_str);
        _filterPressureSensor.setValue(data_str);
        _lastPublishedFilterPressure = filter_pressure;
        _lastFilterPressurePublishTime = currentTime;
    }
}

void HomeAssistantManager::publishGeiger(int cpm, unsigned long currentTime) {
    if (cpm < 0) return; // Do not publish during warmup/settling until valid data is available
    char data_str[16];
    bool is_first = (_lastCpmPublishTime == 0);
    bool cpm_spike = (cpm >= GEIGER_CPM_SPIKE_THRESHOLD && _lastPublishedCpm < GEIGER_CPM_SPIKE_THRESHOLD);
    bool cpm_steady = (currentTime - _lastCpmPublishTime >= STEADY_INTERVAL_60S_MS && cpm != _lastPublishedCpm);
    if (is_first || cpm_spike || cpm_steady || (currentTime - _lastCpmPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        itoa(cpm, data_str, 10);
        _geiger_cpm.setValue(data_str);
        float usv_h = cpm * GEIGER_CPM_TO_USV_H;
        dtostrf(usv_h, 4, 2, data_str);
        _geiger_dose.setValue(data_str);
        _lastPublishedCpm = cpm;
        _lastCpmPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishTemperature(float temp, unsigned long currentTime) {
    if (isnan(temp)) return; // No valid reading (e.g. SCD30 fault)
    char data_str[16];
    bool is_first = (_lastTempPublishTime == 0);
    bool temp_fast = (currentTime - _lastTempPublishTime >= FAST_INTERVAL_5S_MS && fabs(temp - _lastPublishedTemp) >= TEMP_FAST_DELTA_DEG_C);
    bool temp_steady = (currentTime - _lastTempPublishTime >= STEADY_INTERVAL_30S_MS && fabs(temp - _lastPublishedTemp) >= TEMP_STEADY_DELTA_DEG_C);
    if (is_first || temp_fast || temp_steady || (currentTime - _lastTempPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(temp, 4, 1, data_str);
        _temperatureSensor.setValue(data_str);
        _lastPublishedTemp = temp;
        _lastTempPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishHumidity(float humi, unsigned long currentTime) {
    if (isnan(humi)) return; // No valid reading (e.g. SCD30 fault)
    char data_str[16];
    bool is_first = (_lastHumiPublishTime == 0);
    bool humi_fast = (currentTime - _lastHumiPublishTime >= FAST_INTERVAL_5S_MS && fabs(humi - _lastPublishedHumi) >= HUMI_FAST_DELTA_PCT);
    bool humi_steady = (currentTime - _lastHumiPublishTime >= STEADY_INTERVAL_60S_MS && fabs(humi - _lastPublishedHumi) >= HUMI_STEADY_DELTA_PCT);
    if (is_first || humi_fast || humi_steady || (currentTime - _lastHumiPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(humi, 4, 0, data_str);
        _humiditySensor.setValue(data_str);
        _lastPublishedHumi = humi;
        _lastHumiPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishFanCurrent(float amps, unsigned long currentTime) {
    char data_str[16];
    ConfigManagerAccessor config;
    float fan_thresh = config->getFanOffCurrentThreshold();
    bool is_first = (_lastAmpsPublishTime == 0);
    bool fan_state_changed = !is_first && ((amps > fan_thresh) != (_lastPublishedAmps > fan_thresh));
    bool fan_amps_steady = (currentTime - _lastAmpsPublishTime >= STEADY_INTERVAL_60S_MS && fabs(amps - _lastPublishedAmps) >= FAN_AMPS_STEADY_DELTA_A);
    if (is_first || fan_state_changed || fan_amps_steady || (currentTime - _lastAmpsPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(amps, 4, 2, data_str);
        _currentSensor.setValue(data_str);
        _lastPublishedAmps = amps;
        _lastAmpsPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishCO2(float co2, unsigned long currentTime) {
    if (!isValidCO2(co2)) return; // Do not publish during sensor warmup or if invalid
    char data_str[16];
    bool is_first = (_lastCo2PublishTime == 0);
    bool co2_fast = (currentTime - _lastCo2PublishTime >= FAST_INTERVAL_10S_MS && fabs(co2 - _lastPublishedCo2) >= CO2_FAST_DELTA_PPM);
    bool co2_steady = (currentTime - _lastCo2PublishTime >= STEADY_INTERVAL_60S_MS && fabs(co2 - _lastPublishedCo2) >= CO2_STEADY_DELTA_PPM);
    if (is_first || co2_fast || co2_steady || (currentTime - _lastCo2PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(co2, 4, 0, data_str);
        _co2_sensor.setValue(data_str);
        _lastPublishedCo2 = co2;
        _lastCo2PublishTime = currentTime;
    }
}

void HomeAssistantManager::publishCO(float co_ppm, unsigned long currentTime) {
    bool is_first = (_lastCoPublishTime == 0);
    bool co_alert = (co_ppm > 0.0f || (co_ppm == 0.0f && _lastPublishedCo > 0.0f));
    bool co_steady = (currentTime - _lastCoPublishTime >= STEADY_INTERVAL_60S_MS && fabs(co_ppm - _lastPublishedCo) >= CO_STEADY_DELTA_PPM);
    if (is_first || co_alert || co_steady || (currentTime - _lastCoPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _co_sensor.setValue(co_ppm, true);
        _lastPublishedCo = co_ppm;
        _lastCoPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishVOC(int32_t voc_index, unsigned long currentTime) {
    if (!isValidGasIndex(voc_index)) return; // Do not publish during sensor warmup/blackout
    char data_str[16];
    bool is_first = (_lastVocPublishTime == 0);
    bool voc_fast = (currentTime - _lastVocPublishTime >= FAST_INTERVAL_10S_MS && abs(voc_index - _lastPublishedVocIndex) >= VOC_FAST_DELTA);
    bool voc_steady = (currentTime - _lastVocPublishTime >= STEADY_INTERVAL_30S_MS && abs(voc_index - _lastPublishedVocIndex) >= VOC_STEADY_DELTA);
    if (is_first || voc_fast || voc_steady || (currentTime - _lastVocPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        itoa(voc_index, data_str, 10);
        _voc_index_sensor.setValue(data_str);
        _lastPublishedVocIndex = voc_index;
        _lastVocPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishNOx(int32_t nox_index, unsigned long currentTime) {
    if (!isValidGasIndex(nox_index)) return; // Do not publish during sensor warmup/blackout
    char data_str[16];
    bool is_first = (_lastNOxPublishTime == 0);
    bool nox_fast = (currentTime - _lastNOxPublishTime >= FAST_INTERVAL_10S_MS && abs(nox_index - _lastPublishedNOxIndex) >= NOX_FAST_DELTA);
    bool nox_steady = (currentTime - _lastNOxPublishTime >= STEADY_INTERVAL_60S_MS && abs(nox_index - _lastPublishedNOxIndex) >= NOX_STEADY_DELTA);
    if (is_first || nox_fast || nox_steady || (currentTime - _lastNOxPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        itoa(nox_index, data_str, 10);
        _nox_index_sensor.setValue(data_str);
        _lastPublishedNOxIndex = nox_index;
        _lastNOxPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishPM(float pm1, float pm25, float pm4, float pm10, unsigned long currentTime) {
    if (isnan(pm25)) return; // No valid reading (e.g. SPS30 fault)
    char data_str[16];
    bool is_first = (_lastPmPublishTime == 0);
    bool pm_fast = (currentTime - _lastPmPublishTime >= FAST_INTERVAL_10S_MS && fabs(pm25 - _lastPublishedPm2_5) >= PM_FAST_DELTA);
    bool pm_steady = (currentTime - _lastPmPublishTime >= STEADY_INTERVAL_60S_MS && fabs(pm25 - _lastPublishedPm2_5) >= PM_STEADY_DELTA);
    if (is_first || pm_fast || pm_steady || (currentTime - _lastPmPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        dtostrf(pm1, 4, 1, data_str); _pm1_0_sensor.setValue(data_str);
        dtostrf(pm25, 4, 1, data_str); _pm2_5_sensor.setValue(data_str);
        dtostrf(pm4, 4, 1, data_str); _pm4_0_sensor.setValue(data_str);
        dtostrf(pm10, 4, 1, data_str); _pm10_0_sensor.setValue(data_str);

        _lastPublishedPm1_0 = pm1; _lastPublishedPm2_5 = pm25;
        _lastPublishedPm4_0 = pm4; _lastPublishedPm10_0 = pm10;
        _lastPmPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishCompressorCurrent(float compressor_amps, unsigned long currentTime) {
    ConfigManagerAccessor config;
    float comp_thresh = config->getCompressorOffCurrentThreshold();
    bool is_first = (_lastCompressorAmpsPublishTime == 0);
    bool comp_state_changed = !is_first && ((compressor_amps > comp_thresh) != (_lastPublishedCompressorAmps > comp_thresh));
    bool comp_amps_steady = (currentTime - _lastCompressorAmpsPublishTime >= STEADY_INTERVAL_60S_MS && fabs(compressor_amps - _lastPublishedCompressorAmps) >= COMPRESSOR_AMPS_STEADY_DELTA_A);
    if (is_first || comp_state_changed || comp_amps_steady || (currentTime - _lastCompressorAmpsPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _compressorCurrentSensor.setValue(compressor_amps, true);
        _lastPublishedCompressorAmps = compressor_amps;
        _lastCompressorAmpsPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishGeothermalPumpCurrent(float geothermal_pump_amps, unsigned long currentTime) {
    ConfigManagerAccessor config;
    float pump_thresh = config->getPumpOffCurrentThreshold();
    bool is_first = (_lastGeothermalPumpAmpsPublishTime == 0);
    bool pump_state_changed = !is_first && ((geothermal_pump_amps > pump_thresh) != (_lastPublishedGeothermalPumpAmps > pump_thresh));
    bool pump_amps_steady = (currentTime - _lastGeothermalPumpAmpsPublishTime >= STEADY_INTERVAL_60S_MS && fabs(geothermal_pump_amps - _lastPublishedGeothermalPumpAmps) >= PUMP_AMPS_STEADY_DELTA_A);
    if (is_first || pump_state_changed || pump_amps_steady || (currentTime - _lastGeothermalPumpAmpsPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _geothermalPumpCurrentSensor.setValue(geothermal_pump_amps, true);
        _lastPublishedGeothermalPumpAmps = geothermal_pump_amps;
        _lastGeothermalPumpAmpsPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishLiquidLevel(bool liquid_level_sensor_state, unsigned long currentTime) {
    bool is_first = (_lastLiquidLevelPublishTime == 0);
    if (is_first || liquid_level_sensor_state != _lastPublishedLiquidLevelState || (currentTime - _lastLiquidLevelPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _liquidLevelSensor.setState(liquid_level_sensor_state, true);
        _lastPublishedLiquidLevelState = liquid_level_sensor_state;
        _lastLiquidLevelPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishSensorData(
    float pressure, int cpm, float temp, float humi,
    float co2, int32_t voc_index, int32_t nox_index, 
    float amps,
    float pm1, float pm25, float pm4, float pm10,
    float compressor_amps, float geothermal_pump_amps, bool liquid_level_sensor_state,
    float co_ppm,
    float filter_pressure
) {
    unsigned long currentTime = millis();

    publishDifferentialPressure(pressure, currentTime);
    publishFilterPressure(filter_pressure, currentTime);
    publishGeiger(cpm, currentTime);
    publishTemperature(temp, currentTime);
    publishHumidity(humi, currentTime);
    publishFanCurrent(amps, currentTime);
    publishCO2(co2, currentTime);
    publishCO(co_ppm, currentTime);
    publishVOC(voc_index, currentTime);
    publishNOx(nox_index, currentTime);
    publishPM(pm1, pm25, pm4, pm10, currentTime);
    publishCompressorCurrent(compressor_amps, currentTime);
    publishGeothermalPumpCurrent(geothermal_pump_amps, currentTime);
    publishLiquidLevel(liquid_level_sensor_state, currentTime);
}

void HomeAssistantManager::publish_O3_NOx_Values(
    float o3_conc_ug_per_m3, float no2_conc_ug_per_m3, 
    uint16_t fast_aqi, uint16_t epa_aqi
) {
    unsigned long currentTime = millis();

    bool o3_first = (_LastO3PublishTime == 0);
    bool o3_fast = (fabs(o3_conc_ug_per_m3 - _lastPublishedO3) >= O3_FAST_DELTA_UG_M3);
    bool o3_steady = (currentTime - _LastO3PublishTime >= STEADY_INTERVAL_60S_MS && fabs(o3_conc_ug_per_m3 - _lastPublishedO3) >= O3_STEADY_DELTA_UG_M3);
    if (o3_first || o3_fast || o3_steady || (currentTime - _LastO3PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _o3_sensor.setValue(o3_conc_ug_per_m3, true);
        _lastPublishedO3 = o3_conc_ug_per_m3;
        _LastO3PublishTime = currentTime;
    }

    bool no2_first = (_lastNO2PublishTime == 0);
    bool no2_fast = (fabs(no2_conc_ug_per_m3 - _lastPublishedNO2) >= NO2_FAST_DELTA_UG_M3);
    bool no2_steady = (currentTime - _lastNO2PublishTime >= STEADY_INTERVAL_60S_MS && fabs(no2_conc_ug_per_m3 - _lastPublishedNO2) >= NO2_STEADY_DELTA_UG_M3);
    if (no2_first || no2_fast || no2_steady || (currentTime - _lastNO2PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _no2_sensor.setValue(no2_conc_ug_per_m3, true);
        _lastPublishedNO2 = no2_conc_ug_per_m3;
        _lastNO2PublishTime = currentTime;
    }

    bool fast_aqi_first = (_lastFastAQIPublishTime == 0);
    bool fast_aqi_fast = (abs((int)fast_aqi - (int)_lastPublishedFastAQI) >= FAST_AQI_FAST_DELTA);
    bool fast_aqi_steady = (currentTime - _lastFastAQIPublishTime >= STEADY_INTERVAL_60S_MS && fast_aqi != _lastPublishedFastAQI);
    if (fast_aqi_first || fast_aqi_fast || fast_aqi_steady || (currentTime - _lastFastAQIPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _fast_aqi_sensor.setValue(fast_aqi, true);
        _lastPublishedFastAQI = fast_aqi;
        _lastFastAQIPublishTime = currentTime;
    }

    bool epa_aqi_first = (_lastEPAAQIPublishTime == 0);
    bool epa_aqi_fast = (abs((int)epa_aqi - (int)_lastPublishedEPAAQI) >= EPA_AQI_FAST_DELTA);
    bool epa_aqi_steady = (currentTime - _lastEPAAQIPublishTime >= STEADY_INTERVAL_60S_MS && epa_aqi != _lastPublishedEPAAQI);
    if (epa_aqi_first || epa_aqi_fast || epa_aqi_steady || (currentTime - _lastEPAAQIPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _epa_aqi_sensor.setValue(epa_aqi, true);
        _lastPublishedEPAAQI = epa_aqi;
        _lastEPAAQIPublishTime = currentTime;
    }
}

#ifdef BMP280_ENABLED
void HomeAssistantManager::publishBMP280Data(float pressure_pa) {
    unsigned long currentTime = millis();
    
    bool pressure_first = (_lastBMP280PressurePublishTime == 0);
    bool pressure_fast = fabs(pressure_pa - _lastPublishedBMP280Pressure) >= BMP280_PRESSURE_FAST_DELTA_PA;
    bool pressure_steady = (currentTime - _lastBMP280PressurePublishTime >= STEADY_INTERVAL_60S_MS && fabs(pressure_pa - _lastPublishedBMP280Pressure) >= BMP280_PRESSURE_STEADY_DELTA_PA);
    if (pressure_first || pressure_fast || pressure_steady || (currentTime - _lastBMP280PressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _bmp280PressureSensor.setValue(pressure_pa, true);
        _lastPublishedBMP280Pressure = pressure_pa;
        _lastBMP280PressurePublishTime = currentTime;
    }
}

void HomeAssistantManager::publishBMP280Data(float pressure_pa, float temperature_degc) {
    unsigned long currentTime = millis();
    
    bool pressure_first = (_lastBMP280PressurePublishTime == 0);
    bool pressure_fast = fabs(pressure_pa - _lastPublishedBMP280Pressure) >= BMP280_PRESSURE_FAST_DELTA_PA;
    bool pressure_steady = (currentTime - _lastBMP280PressurePublishTime >= STEADY_INTERVAL_60S_MS && fabs(pressure_pa - _lastPublishedBMP280Pressure) >= BMP280_PRESSURE_STEADY_DELTA_PA);
    if (pressure_first || pressure_fast || pressure_steady || (currentTime - _lastBMP280PressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _bmp280PressureSensor.setValue(pressure_pa, true);
        _lastPublishedBMP280Pressure = pressure_pa;
        _lastBMP280PressurePublishTime = currentTime;
    }
    
    bool temp_first = (_lastBMP280TemperaturePublishTime == 0);
    bool temp_fast = (currentTime - _lastBMP280TemperaturePublishTime >= FAST_INTERVAL_5S_MS && fabs(temperature_degc - _lastPublishedBMP280Temperature) >= BMP280_TEMP_FAST_DELTA_DEG_C);
    bool temp_steady = (currentTime - _lastBMP280TemperaturePublishTime >= STEADY_INTERVAL_60S_MS && fabs(temperature_degc - _lastPublishedBMP280Temperature) >= BMP280_TEMP_STEADY_DELTA_DEG_C);
    if (temp_first || temp_fast || temp_steady || (currentTime - _lastBMP280TemperaturePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _bmp280TemperatureSensor.setValue(temperature_degc, true);
        _lastPublishedBMP280Temperature = temperature_degc;
        _lastBMP280TemperaturePublishTime = currentTime;
    }
}
#endif

#ifdef AHT20_ENABLED
void HomeAssistantManager::publishAHT20Data(float temperature_degc, float humidity_pct) {
    unsigned long currentTime = millis();
    
    bool temp_first = (_lastAHT20TemperaturePublishTime == 0);
    bool temp_fast = (currentTime - _lastAHT20TemperaturePublishTime >= FAST_INTERVAL_5S_MS && fabs(temperature_degc - _lastPublishedAHT20Temperature) >= AHT20_TEMP_FAST_DELTA_DEG_C);
    bool temp_steady = (currentTime - _lastAHT20TemperaturePublishTime >= STEADY_INTERVAL_60S_MS && fabs(temperature_degc - _lastPublishedAHT20Temperature) >= AHT20_TEMP_STEADY_DELTA_DEG_C);
    if (temp_first || temp_fast || temp_steady || (currentTime - _lastAHT20TemperaturePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _aht20TemperatureSensor.setValue(temperature_degc, true);
        _lastPublishedAHT20Temperature = temperature_degc;
        _lastAHT20TemperaturePublishTime = currentTime;
    }
    
    bool humi_first = (_lastAHT20HumidityPublishTime == 0);
    bool humi_fast = (currentTime - _lastAHT20HumidityPublishTime >= FAST_INTERVAL_5S_MS && fabs(humidity_pct - _lastPublishedAHT20Humidity) >= AHT20_HUMI_FAST_DELTA_PCT);
    bool humi_steady = (currentTime - _lastAHT20HumidityPublishTime >= STEADY_INTERVAL_60S_MS && fabs(humidity_pct - _lastPublishedAHT20Humidity) >= AHT20_HUMI_STEADY_DELTA_PCT);
    if (humi_first || humi_fast || humi_steady || (currentTime - _lastAHT20HumidityPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _aht20HumiditySensor.setValue(humidity_pct, true);
        _lastPublishedAHT20Humidity = humidity_pct;
        _lastAHT20HumidityPublishTime = currentTime;
    }
}
#endif

void HomeAssistantManager::publishWiFiStatus(bool connected, int8_t rssi, const char* ssid, const char* ip) {
    unsigned long currentTime = millis();
    bool is_first = (_lastWifiStatusPublishTime == 0);
    bool state_change = (connected != _lastPublishedWifiConnected);
    bool rssi_change = (currentTime - _lastWifiStatusPublishTime >= STEADY_INTERVAL_60S_MS && abs(rssi - _lastPublishedWifiRssi) >= WIFI_RSSI_DELTA_DBM);
    if (is_first || state_change || rssi_change || (currentTime - _lastWifiStatusPublishTime > WIFI_STATUS_FORCE_INTERVAL_MS)) {
        if (connected) {
            _wifi_rssi.setValue(rssi, true);
        } else {
            _wifi_rssi.setValue(static_cast<int8_t>(0), true);
        }
        _lastPublishedWifiConnected = connected;
        _lastPublishedWifiRssi = rssi;
        _lastWifiStatusPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishSensorConnectionStatus(bool connected) {
    unsigned long currentTime = millis();
    bool is_first = (_lastSensorStatusPublishTime == 0);
    if (is_first || connected != _lastPublishedSensorConnected || (currentTime - _lastSensorStatusPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _sensorStatus.setState(connected);
        _lastPublishedSensorConnected = connected;
        _lastSensorStatusPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishHighPressureStatus(bool is_high) {
    unsigned long currentTime = millis();
    bool is_first = (_lastHighPressurePublishTime == 0);
    if (is_first || is_high != _lastPublishedHighPressure || (currentTime - _lastHighPressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
        _highPressureSensor.setState(is_high, true);
        _lastPublishedHighPressure = is_high;
        _lastHighPressurePublishTime = currentTime;
    }
}

void HomeAssistantManager::publishFanStatus(bool is_on) {
    unsigned long currentTime = millis();
    bool is_first = (_lastFanStatusPublishTime == 0);
    if (is_first || is_on != _lastPublishedFanStatus || (currentTime - _lastFanStatusPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
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
    bool is_first = (_lastSensorStackUptimePublishTime == 0);

    // The check below ensures we either publish on a regular interval (60s), when forced, or on initial boot reading.
    if (force || is_first || (currentTime - _lastSensorStackUptimePublishTime >= STEADY_INTERVAL_60S_MS)) {
        _sensorStackUptimeSensor.setValue(uptime_seconds);
        _lastSensorStackUptimePublishTime = currentTime;

        if (force) {
            logger.debugf("Forced publish of SensorStack Uptime: %lu s", uptime_seconds);
        }
    }
}

void HomeAssistantManager::publishSensorStackFreeRam(uint16_t free_ram) {
    uint32_t currentTime = millis();
    bool is_first = (_lastSensorStackFreeRamPublishTime == 0);
    if (is_first || (currentTime - _lastSensorStackFreeRamPublishTime >= STEADY_INTERVAL_60S_MS)) {
        _sensorStackFreeRamSensor.setValue(free_ram, true);
        _lastSensorStackFreeRamPublishTime = currentTime;
    }
}

void HomeAssistantManager::onRebootCommand(HAButton* sender) {
    sender->setAvailability(false);
    RebootManager::getInstance().requestReboot("Home Assistant MQTT command", 500);
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
    if (index < 0 || index > 3) {
        logger.warningf("Rejected invalid log level from Home Assistant: %d (allowed 0-3)", index);
        return;
    }
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
    sender->setState(state);
    send_command_to_nano_with_param(CMD_SET_SCD30_AUTOCAL, state ? '1' : '0');
}

void HomeAssistantManager::updateScd30AutoCalState(bool state) {
    _scd30AutoCalSwitch.setState(state);
}

void HomeAssistantManager::onScd30ForceCalCommand(HANumeric number, HANumber* sender) {
    int32_t ppm_raw = number.toInt32();
    if (ppm_raw < SCD30_FORCECAL_MIN_PPM || ppm_raw > SCD30_FORCECAL_MAX_PPM) {
        logger.warningf("Rejected out-of-range SCD30 Force Cal from Home Assistant: %d ppm (allowed %u-%u)",
                        ppm_raw, SCD30_FORCECAL_MIN_PPM, SCD30_FORCECAL_MAX_PPM);
        sender->setState(sender->getCurrentState());
        return;
    }
    uint16_t ppm_value = static_cast<uint16_t>(ppm_raw);
    logger.infof("SCD30 Force Calibration to %u ppm command from Home Assistant.", ppm_value);
    
    char cmd_with_param[8];
    snprintf(cmd_with_param, sizeof(cmd_with_param), "%c%u", CMD_SET_SCD30_FORCECAL, ppm_value);
    send_command_to_nano_str(cmd_with_param);
}

void HomeAssistantManager::updateScd30ForceCalValue(uint16_t value) {
    _scd30ForceCalNumber.setState(value);
}

void HomeAssistantManager::updateBacklightState(bool state, uint8_t brightness) {
    _backlight.setState(state);
    if (state && brightness > 0) {
        _backlight.setBrightness(brightness);
    }
}

void HomeAssistantManager::onInactivityTimerDelayCommand(HANumeric number, HANumber* sender) {
    if (_instance) {
        int32_t delay = number.toInt32();
        if (delay < 10 || delay > 120) {
            logger.warningf("Rejected out-of-range inactivity timer delay from Home Assistant: %d (allowed 10-120)", delay);
            ConfigManagerAccessor config;
            sender->setState(static_cast<int32_t>(config->getInactivityTimerDelay()));
            return;
        }
        ConfigManagerAccessor config;
        config->setInactivityTimerDelay(delay);
        sender->setState(delay);
        _instance->_lastInactivityTimerDelayPublishTime = millis();
        logger.infof("Inactivity timer delay updated to %d seconds via Home Assistant", delay);
    }
}

void HomeAssistantManager::publishEsp32FreeRam(uint32_t free_ram) {
    unsigned long currentTime = millis();
    bool is_first = (_lastEsp32FreeRamPublishTime == 0);
    if (is_first || (currentTime - _lastEsp32FreeRamPublishTime >= STEADY_INTERVAL_60S_MS)) {
        _esp32FreeRamSensor.setValue(free_ram, true);
        _lastEsp32FreeRamPublishTime = currentTime;
    }
}

void HomeAssistantManager::publishEsp32Uptime(uint32_t uptime_seconds) {
    unsigned long currentTime = millis();
    bool is_first = (_lastEsp32UptimePublishTime == 0);
    if (is_first || (currentTime - _lastEsp32UptimePublishTime >= STEADY_INTERVAL_60S_MS)) {
        _esp32UptimeSensor.setValue(uptime_seconds);
        _lastEsp32UptimePublishTime = currentTime;
    }
}

void HomeAssistantManager::updateInactivityTimerDelayState(bool force) {
    unsigned long currentTime = millis();
    // Use a longer interval for configuration values that don't change frequently
    const unsigned long CONFIG_UPDATE_INTERVAL_MS = 30000; // 30 seconds
    if (force || (currentTime - _lastInactivityTimerDelayPublishTime >= CONFIG_UPDATE_INTERVAL_MS)) {
        ConfigManagerAccessor config;
        _inactivityTimerDelay.setState(config->getInactivityTimerDelay());
        _lastInactivityTimerDelayPublishTime = currentTime;
    }
}

void HomeAssistantManager::updateLogLevelState(AppLogLevel level) {
    _logLevelSelect.setState(static_cast<int8_t>(level));
}

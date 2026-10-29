#include "HomeAssistantManager.h"
#include "Esp32Architecture.h"
#include "NanoCommands.h"
#include "RebootManager.h"
#include "SensorDefines.h"
#include "SensorDataAccessor.h"
#include "SystemStatusAccessor.h"
#include "MainTaskEvents.h"
#include "TaskPriorities.h"
#include <WiFi.h>
#include <math.h>

// Set sensor availability and expiration
#define SENSOR_EXPIRE_TIMEOUT_S 300
#define RANDOM_SUFFIX ""
#define FORCE_PUBLISH_INTERVAL_MS 120000

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
constexpr int GEIGER_CPM_SPIKE_THRESHOLD = 10;
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
constexpr float CO2_FAST_DELTA_PPM = 25.0f;
constexpr float CO2_STEADY_DELTA_PPM = 10.0f;
constexpr float CO_STEADY_DELTA_PPM = 1.0f;
// Above this, CO is published with every SensorStack packet (safety alert path)
constexpr float CO_ALERT_THRESHOLD_PPM = 2.0f;
constexpr int32_t VOC_FAST_DELTA = 15;
constexpr int32_t VOC_STEADY_DELTA = 5;
constexpr int32_t NOX_FAST_DELTA = 5;
constexpr int32_t NOX_STEADY_DELTA = 1;

// Particulate Matter
constexpr float PM_FAST_DELTA = 10.0f;
constexpr float PM_STEADY_DELTA = 1.0f;

// ZMOD4510 Gas & AQI
constexpr float O3_FAST_DELTA_UG_M3 = 5.0f;
constexpr float O3_STEADY_DELTA_UG_M3 = 2.0f;
constexpr float NO2_FAST_DELTA_UG_M3 = 5.0f;
constexpr float NO2_STEADY_DELTA_UG_M3 = 2.0f;
constexpr int FAST_AQI_FAST_DELTA = 10;
constexpr int EPA_AQI_FAST_DELTA = 10;

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

// MQTT: TCP connect timeout to the broker
constexpr uint32_t MQTT_CONNECT_TIMEOUT_MS = 2000;
} // namespace

HomeAssistantManager *HomeAssistantManager::_instance = nullptr;

HomeAssistantManager::HomeAssistantManager()
    : _device(MQTT_DEVICE_ID), _mqtt(_wifiClient, _device, 64),
      _pressureSensor("pressure" RANDOM_SUFFIX, HASensorNumber::PrecisionP1),
      _filterPressureSensor("filter_pressure" RANDOM_SUFFIX,
                            HASensorNumber::PrecisionP1),
      _backlight("backlight" RANDOM_SUFFIX, HALight::BrightnessFeature),
      _wifi_rssi("wifi_rssi" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _wifi_ssid("wifi_ssid" RANDOM_SUFFIX), _wifi_ip("wifi_ip" RANDOM_SUFFIX),
      _geiger_cpm("geiger_cpm" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _geiger_dose("geiger_dose" RANDOM_SUFFIX, HASensorNumber::PrecisionP2),
      _temperatureSensor("temperature" RANDOM_SUFFIX,
                         HASensorNumber::PrecisionP1),
      _humiditySensor("humidity" RANDOM_SUFFIX, HASensorNumber::PrecisionP1),
#ifdef BMP280_ENABLED
      _bmp280PressureSensor("bmp280_pressure" RANDOM_SUFFIX,
                            HASensorNumber::PrecisionP1),
      _bmp280TemperatureSensor("bmp280_temperature" RANDOM_SUFFIX,
                               HASensorNumber::PrecisionP1),
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
      _pm1_0_sensor("pm1_0" RANDOM_SUFFIX, HASensorNumber::PrecisionP1),
      _pm2_5_sensor("pm2_5" RANDOM_SUFFIX, HASensorNumber::PrecisionP1),
      _pm4_0_sensor("pm4_0" RANDOM_SUFFIX, HASensorNumber::PrecisionP1),
      _pm10_0_sensor("pm10_0" RANDOM_SUFFIX, HASensorNumber::PrecisionP1),
      _o3_sensor("o3" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _no2_sensor("no2" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _fast_aqi_sensor("fast_aqi" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _epa_aqi_sensor("epa_aqi" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _co2_sensor("co2" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _co_sensor("co" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _sensorStackUptimeSensor("sensorStack_uptime" RANDOM_SUFFIX,
                               HASensorNumber::PrecisionP0),
      _sensorStackVersionSensor("sensorStack_version" RANDOM_SUFFIX),
      _sensorStackFreeRamSensor("sensorStack_freeRam" RANDOM_SUFFIX,
                                HASensorNumber::PrecisionP0),
      _voc_index_sensor("voc_index" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _nox_index_sensor("nox_index" RANDOM_SUFFIX, HASensorNumber::PrecisionP0),
      _currentSensor("current" RANDOM_SUFFIX, HASensorNumber::PrecisionP2),
      _compressorCurrentSensor("compressor_current" RANDOM_SUFFIX,
                               HASensorNumber::PrecisionP2),
      _geothermalPumpCurrentSensor("geothermal_pump_current" RANDOM_SUFFIX,
                                   HASensorNumber::PrecisionP2),
      _liquidLevelSensor("liquid_level" RANDOM_SUFFIX),
      _sensorStackResetCauseSensor("sensorStack_reset_cause" RANDOM_SUFFIX),
      _inactivityTimerDelay("inactivity_timer_delay" RANDOM_SUFFIX,
                            HANumber::PrecisionP0),
      _getSps30InfoButton("get_sps30_info" RANDOM_SUFFIX),
      _Sps30ManualCleanButton("sps30_manual_clean" RANDOM_SUFFIX),
      _getSgp41SelftestButton("get_sgp41_selftest" RANDOM_SUFFIX),
      _logLevelSelect("log_level" RANDOM_SUFFIX),
      _scd30AutoCalSwitch("scd30_auto_cal" RANDOM_SUFFIX),
      _scd30ForceCalNumber("scd30_force_cal" RANDOM_SUFFIX,
                           HANumber::PrecisionP0),
      _esp32FreeRamSensor("esp32_free_ram" RANDOM_SUFFIX,
                          HASensorNumber::PrecisionP0),
      _esp32UptimeSensor("esp32_uptime" RANDOM_SUFFIX,
                         HASensorNumber::PrecisionP0) {
  _instance = this;
  _configurationUrl[0] = '\0';
  _firmwareVersion[0] = '\0';
  _logQueue = nullptr;
}

HomeAssistantManager::~HomeAssistantManager() {
  if (_taskHandle) {
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
  }
  if (_logQueue) {
    vQueueDelete(_logQueue);
    _logQueue = nullptr;
  }
  if (_instance == this) {
    _instance = nullptr;
  }
}

void HomeAssistantManager::init(const char *firmware_version) {
  if (firmware_version != nullptr) {
    strncpy(_firmwareVersion, firmware_version, sizeof(_firmwareVersion) - 1);
    _firmwareVersion[sizeof(_firmwareVersion) - 1] = '\0';
  }

  if (_logQueue == nullptr) {
    _logQueue = xQueueCreate(8, sizeof(LogQueueEntry));
  }

  if (_taskHandle == nullptr) {
    BaseType_t result = xTaskCreatePinnedToCore(
        taskFunction, "MqttTask", STACK_SIZE_MQTT, this, TASK_PRIORITY_MQTT,
        &_taskHandle, tskNO_AFFINITY);
    if (result != pdPASS) {
      logger.errorf("Failed to create MqttTask. Free heap: %u bytes",
                    ESP.getFreeHeap());
      _taskHandle = nullptr;
    } else {
      logger.info("MqttTask started successfully");
    }
  }
}

void HomeAssistantManager::initMqtt() {
  const char *const entity_category_diagnostic = "diagnostic";
  const char *const entity_category_config = "config";

  ConfigManagerAccessor config;

  _device.setManufacturer("Guition");
  _device.setModel("ESP32-2432S022C");

  const char *deviceName = config->getMqttDeviceName();
  if (deviceName != nullptr && strlen(deviceName) > 0) {
    logger.infof("Setting device name: %s", deviceName);
    _device.setName(deviceName);
  } else {
    logger.warningf("Invalid device name, using default");
    _device.setName("HVAC Sensor Display");
  }

  if (_firmwareVersion[0] != '\0') {
    _device.setSoftwareVersion(_firmwareVersion);
  }

  if (WiFi.status() == WL_CONNECTED) {
    snprintf(_configurationUrl, sizeof(_configurationUrl), "http://%s",
             WiFi.localIP().toString().c_str());
    _device.setConfigurationUrl(_configurationUrl);
    logger.infof("Setting device configuration URL: %s", _configurationUrl);
  }

  const char *deviceId = config->getMqttDeviceId();
  if (deviceId != nullptr && strlen(deviceId) > 0) {
    logger.infof("Setting device ID: %s", deviceId);
    char mqtt_device_id[32];
    snprintf(mqtt_device_id, sizeof(mqtt_device_id), "%s", deviceId);
    _device.setUniqueId(reinterpret_cast<const byte *>(mqtt_device_id),
                        strlen(mqtt_device_id));
  } else {
    logger.warningf("Invalid device ID, using default");
    _device.setUniqueId(
        reinterpret_cast<const byte *>("hvac_diff_pressure_sensor_01"),
        strlen("hvac_diff_pressure_sensor_01"));
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

  _liquidLevelSensor.setName("Condensate Overflow Sensor");
  _liquidLevelSensor.setDeviceClass("problem");
  _liquidLevelSensor.setIcon("mdi:cup-water");
  _liquidLevelSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _sensorStackResetCauseSensor.setName("Sensor Stack Reset Cause");
  _sensorStackResetCauseSensor.setIcon("mdi:restart-alert");
  _sensorStackResetCauseSensor.setEntityCategory(entity_category_diagnostic);

  _backlight.setName("Screen Backlight");
  _backlight.setIcon("mdi:brightness-6");
  _backlight.onBrightnessCommand(onBrightnessCommand);
  _backlight.onStateCommand(onStateCommand);

  _wifi_rssi.setName("WiFi RSSI");
  _wifi_rssi.setDeviceClass("signal_strength");
  _wifi_rssi.setUnitOfMeasurement("dBm");
  _wifi_rssi.setIcon("mdi:wifi");
  _wifi_rssi.setEntityCategory(entity_category_diagnostic);

  _wifi_ssid.setName("WiFi SSID");
  _wifi_ssid.setIcon("mdi:wifi-cog");
  _wifi_ssid.setEntityCategory(entity_category_diagnostic);

  _wifi_ip.setName("WiFi IP");
  _wifi_ip.setIcon("mdi:ip-network");
  _wifi_ip.setEntityCategory(entity_category_diagnostic);

  _geiger_cpm.setName("Geiger CPM");
  _geiger_cpm.setUnitOfMeasurement("cpm");
  _geiger_cpm.setIcon("mdi:radioactive");
  _geiger_cpm.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _geiger_dose.setName("Geiger Radiation Dose");
  _geiger_dose.setUnitOfMeasurement("µSv/h");
  _geiger_dose.setIcon("mdi:radioactive");
  _geiger_dose.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _pm1_0_sensor.setName("Particulate Matter 1.0");
  _pm1_0_sensor.setDeviceClass("pm1");
  _pm1_0_sensor.setUnitOfMeasurement("µg/m³");
  _pm1_0_sensor.setIcon("mdi:blur");
  _pm1_0_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _pm2_5_sensor.setName("Particulate Matter 2.5");
  _pm2_5_sensor.setDeviceClass("pm25");
  _pm2_5_sensor.setUnitOfMeasurement("µg/m³");
  _pm2_5_sensor.setIcon("mdi:blur");
  _pm2_5_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _pm4_0_sensor.setName("Particulate Matter 4.0");
  _pm4_0_sensor.setUnitOfMeasurement("µg/m³");
  _pm4_0_sensor.setIcon("mdi:blur");
  _pm4_0_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _pm10_0_sensor.setName("Particulate Matter 10.0");
  _pm10_0_sensor.setDeviceClass("pm10");
  _pm10_0_sensor.setUnitOfMeasurement("µg/m³");
  _pm10_0_sensor.setIcon("mdi:blur");
  _pm10_0_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _o3_sensor.setName("Ozone (O3)");
  _o3_sensor.setDeviceClass("ozone");
  _o3_sensor.setUnitOfMeasurement("µg/m³");
  _o3_sensor.setIcon("mdi:chemical-weapon");
  _o3_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _no2_sensor.setName("Nitrogen Dioxide (NO2)");
  _no2_sensor.setDeviceClass("nitrogen_dioxide");
  _no2_sensor.setUnitOfMeasurement("µg/m³");
  _no2_sensor.setIcon("mdi:chemical-weapon");
  _no2_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _fast_aqi_sensor.setName("Fast AQI");
  _fast_aqi_sensor.setDeviceClass("aqi");
  _fast_aqi_sensor.setStateClass("measurement");
  _fast_aqi_sensor.setIcon("mdi:air-filter");
  _fast_aqi_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _epa_aqi_sensor.setName("EPA AQI");
  _epa_aqi_sensor.setDeviceClass("aqi");
  _epa_aqi_sensor.setStateClass("measurement");
  _epa_aqi_sensor.setIcon("mdi:air-filter");
  _epa_aqi_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _co2_sensor.setName("Carbon Dioxide (CO2)");
  _co2_sensor.setDeviceClass("carbon_dioxide");
  _co2_sensor.setUnitOfMeasurement("ppm");
  _co2_sensor.setIcon("mdi:molecule-co2");
  _co2_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _co_sensor.setName("Carbon Monoxide (CO)");
  _co_sensor.setDeviceClass("carbon_monoxide");
  _co_sensor.setUnitOfMeasurement("ppm");
  _co_sensor.setIcon("mdi:molecule-co");
  _co_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _voc_index_sensor.setName("VOC Index");
  _voc_index_sensor.setDeviceClass("aqi");
  _voc_index_sensor.setStateClass("measurement");
  _voc_index_sensor.setIcon("mdi:lungs");
  _voc_index_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _nox_index_sensor.setName("NOx Index");
  _nox_index_sensor.setDeviceClass("aqi");
  _nox_index_sensor.setStateClass("measurement");
  _nox_index_sensor.setIcon("mdi:smog");
  _nox_index_sensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _sensorStatus.setName("Sensor Connection");
  _sensorStatus.setDeviceClass("connectivity");
  _sensorStatus.setIcon("mdi:connection");
  _sensorStatus.setEntityCategory(entity_category_diagnostic);

  _sensorStackVersionSensor.setName("Sensor Stack Version");
  _sensorStackVersionSensor.setIcon("mdi:chip");
  _sensorStackVersionSensor.setEntityCategory(entity_category_diagnostic);

  _sensorStackUptimeSensor.setName("Sensor Stack Uptime");
  _sensorStackUptimeSensor.setDeviceClass("duration");
  _sensorStackUptimeSensor.setUnitOfMeasurement("s");
  _sensorStackUptimeSensor.setIcon("mdi:timer-outline");
  _sensorStackUptimeSensor.setEntityCategory(entity_category_diagnostic);

  _sensorStackFreeRamSensor.setName("Sensor Stack Free RAM");
  _sensorStackFreeRamSensor.setDeviceClass("data_size");
  _sensorStackFreeRamSensor.setUnitOfMeasurement("B");
  _sensorStackFreeRamSensor.setIcon("mdi:memory");
  _sensorStackFreeRamSensor.setEntityCategory(entity_category_diagnostic);

  _highPressureSensor.setName("High Pressure Status");
  _highPressureSensor.setDeviceClass("problem");
  _highPressureSensor.setIcon("mdi:alert");
  _highPressureSensor.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _fanStatus.setName("Fan Status");
  _fanStatus.setDeviceClass("running");
  _fanStatus.setIcon("mdi:fan");
  _fanStatus.setExpireAfter(SENSOR_EXPIRE_TIMEOUT_S);

  _rebootButton.setName("Reboot ESP32");
  _rebootButton.setIcon("mdi:restart");
  _rebootButton.setEntityCategory(entity_category_config);
  _rebootButton.onCommand(onRebootCommand);

  _rebootSensorStackButton.setName("Reboot Sensor Stack");
  _rebootSensorStackButton.setIcon("mdi:restart");
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
  _logLevelSelect.setOptions("Debug;Info;Warning;Error");
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
  _scd30ForceCalNumber.setMin(SensorDefines::SCD30_FORCECAL_MIN_PPM);
  _scd30ForceCalNumber.setMax(SensorDefines::SCD30_FORCECAL_MAX_PPM);
  _scd30ForceCalNumber.setStep(1);
  _scd30ForceCalNumber.onCommand(onScd30ForceCalCommand);

  _inactivityTimerDelay.setName("Inactivity Timer Delay");
  _inactivityTimerDelay.setIcon("mdi:timer");
  _inactivityTimerDelay.setEntityCategory(entity_category_config);
  _inactivityTimerDelay.setUnitOfMeasurement("s");
  _inactivityTimerDelay.setMin(ConfigLimits::MIN_INACTIVITY_DELAY_S);
  _inactivityTimerDelay.setMax(ConfigLimits::MAX_INACTIVITY_DELAY_S);
  _inactivityTimerDelay.setStep(ConfigLimits::INACTIVITY_DELAY_STEP_S);
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

  _wifiClient.setConnectionTimeout(MQTT_CONNECT_TIMEOUT_MS);

  const char *mqttHost = config->getMqttHost();
  int mqttPort = config->getMqttPort();
  const char *mqttUser = config->getMqttUser();
  const char *mqttPassword = config->getMqttPassword();

  logger.infof("MQTT Host: %s", mqttHost ? mqttHost : "null");
  logger.infof("MQTT Port: %d", mqttPort);
  logger.infof("MQTT User: %s", mqttUser ? mqttUser : "null");
  logger.infof("MQTT Password: %s", mqttPassword ? "***" : "null");

  if (mqttHost != nullptr && strlen(mqttHost) > 0 && mqttUser != nullptr &&
      strlen(mqttUser) > 0 && mqttPassword != nullptr &&
      strlen(mqttPassword) > 0) {
    logger.infof("Using ConfigManager MQTT settings");
    _mqtt.begin(mqttHost, mqttPort, mqttUser, mqttPassword);
  } else {
    logger.warningf("Invalid MQTT settings, using hardcoded defaults");
    _mqtt.begin(MQTT_HOST, MQTT_USER, MQTT_PASSWORD);
  }
}

bool HomeAssistantManager::isMqttConnected() {
  return WiFi.status() == WL_CONNECTED && _lastMqttConnected;
}

void HomeAssistantManager::taskFunction(void *pvParameters) {
  HomeAssistantManager *instance =
      static_cast<HomeAssistantManager *>(pvParameters);
  if (!instance) {
    vTaskDelete(NULL);
    return;
  }
  instance->initMqtt();
  instance->taskLoop();
  vTaskDelete(NULL);
}

void HomeAssistantManager::taskLoop() {
  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      _mqtt.loop();
    }

    bool connected = (WiFi.status() == WL_CONNECTED && _mqtt.isConnected());
    if (connected != _lastMqttConnected) {
      _lastMqttConnected = connected;
      SystemStatusAccessor status;
      status->mqttConnected = connected;
    }

    if (connected) {
      drainLogQueue();

      SystemStatusData sysState;
      {
        SystemStatusAccessor status;
        sysState = *status;
      }

      SensorStateData sensorState;
      {
        SensorDataAccessor sensor;
        sensorState = *sensor;
      }

      unsigned long currentTime = millis();

      // 1. Process control/polled values from other tasks to HA
      // A new reply from the Nano is published with force, so Home Assistant's
      // control reverts even when a refused request left the value unchanged
      // Nothing is published before the SensorStack reported the actual settings
      bool autoCalReply = sysState.scd30AutoCalSeq != _lastSeenScd30AutoCalSeq;
      if (sysState.scd30CalKnown &&
          (!_hasPublishedScd30AutoCal || autoCalReply ||
           sysState.scd30AutoCalState != _lastPublishedScd30AutoCal)) {
        _scd30AutoCalSwitch.setState(sysState.scd30AutoCalState, autoCalReply);
        _lastPublishedScd30AutoCal = sysState.scd30AutoCalState;
        _lastSeenScd30AutoCalSeq = sysState.scd30AutoCalSeq;
        _hasPublishedScd30AutoCal = true;
      }
      bool forceCalReply = sysState.scd30ForceCalSeq != _lastSeenScd30ForceCalSeq;
      if (sysState.scd30CalKnown &&
          (!_hasPublishedScd30ForceCal || forceCalReply ||
           sysState.scd30ForceCalValue != _lastPublishedScd30ForceCal)) {
        _scd30ForceCalNumber.setState(sysState.scd30ForceCalValue, forceCalReply);
        _lastPublishedScd30ForceCal = sysState.scd30ForceCalValue;
        _lastSeenScd30ForceCalSeq = sysState.scd30ForceCalSeq;
        _hasPublishedScd30ForceCal = true;
      }
      if (!_hasPublishedBacklight || sysState.backlightState != _lastPublishedBacklightState ||
          sysState.backlightBrightness != _lastPublishedBacklightBrightness) {
        _backlight.setState(sysState.backlightState);
        if (sysState.backlightState && sysState.backlightBrightness > 0) {
          _backlight.setBrightness(sysState.backlightBrightness);
        }
        _lastPublishedBacklightState = sysState.backlightState;
        _lastPublishedBacklightBrightness = sysState.backlightBrightness;
        _hasPublishedBacklight = true;
      }
      if (!_hasPublishedInactivityDelay || sysState.inactivityTimerDelay != _lastPublishedInactivityDelay || sysState.inactivityTimerDelayForce) {
        _inactivityTimerDelay.setState(
            static_cast<int32_t>(sysState.inactivityTimerDelay),
            sysState.inactivityTimerDelayForce);
        _lastPublishedInactivityDelay = sysState.inactivityTimerDelay;
        _hasPublishedInactivityDelay = true;
        if (sysState.inactivityTimerDelayForce) {
          SystemStatusAccessor status;
          status->inactivityTimerDelayForce = false;
        }
      }
      if (!_hasPublishedLogLevel || sysState.logLevel != _lastPublishedLogLevel) {
        _logLevelSelect.setState(static_cast<int8_t>(sysState.logLevel));
        _lastPublishedLogLevel = sysState.logLevel;
        _hasPublishedLogLevel = true;
      }

      // 2. Readings that can't be trusted are published as unknown, once
      // The SensorStack is down while disconnected or without a working firmware,
      // and until its first packet after that (its readings were cleared)
      const bool stackUp = sysState.sensorConnectionStatus &&
                           sysState.sensorStackFirmwareState == SensorStackFirmwareState::Ok &&
                           sensorState.nano_updated_ms != 0;
      uint8_t unknown = stackUp ? 0 : GROUP_ALL;
      // Sensors the SensorStack reports as not responding
      const uint8_t faults = sysState.sensorStackSensorFaults;
      if (faults & SensorDefines::SENSOR_FAULT_SCD30) unknown |= GROUP_SCD30;
      if (faults & SensorDefines::SENSOR_FAULT_SPS30) unknown |= GROUP_SPS30;
      if (faults & SensorDefines::SENSOR_FAULT_SGP41) unknown |= GROUP_SGP41;
      // I2C bridge sensors that are not initialized or keep failing
      if (!sensorState.zmod_ok) unknown |= GROUP_ZMOD;
#ifdef BMP280_ENABLED
      if (!sensorState.bmp_ok) unknown |= GROUP_BMP280;
#endif
#ifdef AHT20_ENABLED
      if (!sensorState.aht_ok) unknown |= GROUP_AHT20;
#endif
      updateUnknownGroups(unknown);
      const uint8_t live = GROUP_ALL & ~unknown;

      // 3. Publish SensorStack telemetry
      const bool newNanoPacket = sensorState.nano_updated_ms != _lastSeenNanoUpdateMs;
      _lastSeenNanoUpdateMs = sensorState.nano_updated_ms;
      if (live & GROUP_STACK) {
        // Read once, so the config lock isn't held while publishing
        float fanOffThreshold, compressorOffThreshold, pumpOffThreshold;
        {
          ConfigManagerAccessor config;
          fanOffThreshold = config->getFanOffCurrentThreshold();
          compressorOffThreshold = config->getCompressorOffCurrentThreshold();
          pumpOffThreshold = config->getPumpOffCurrentThreshold();
        }
        publishDifferentialPressure(sensorState.pressure, currentTime);
        publishFilterPressure(sensorState.filter_pressure, currentTime);
        publishGeiger(sensorState.cpm, currentTime);
        publishFanCurrent(sensorState.fan_amps, fanOffThreshold, currentTime);
        publishCO(sensorState.co_ppm, newNanoPacket, currentTime);
        publishCompressorCurrent(sensorState.compressor_amps, compressorOffThreshold, currentTime);
        publishGeothermalPumpCurrent(sensorState.geothermal_pump_amps, pumpOffThreshold, currentTime);
        publishLiquidLevel(sensorState.liquid_level_sensor_state, currentTime);
        publishHighPressure(sensorState.high_pressure);
        publishFan(sensorState.fan_status != FanStatus::Off);
      }
      if (live & GROUP_SCD30) {
        publishTemperature(sensorState.temp, currentTime);
        publishHumidity(sensorState.humi, currentTime);
        publishCO2(sensorState.co2, currentTime);
      }
      if (live & GROUP_SGP41) {
        publishVOC(sensorState.voc_index, currentTime);
        publishNOx(sensorState.nox_index, currentTime);
      }
      if (live & GROUP_SPS30) {
        publishPM(sensorState.pm1, sensorState.pm25, sensorState.pm4, sensorState.pm10, currentTime);
      }

      // I2C bridge sensors
      if (live & GROUP_ZMOD) {
        publishO3(sensorState.o3_conc_ug_per_m3, currentTime);
        publishNO2(sensorState.no2_conc_ug_per_m3, currentTime);
        publishFastAQI(sensorState.fast_aqi, currentTime);
        publishEPAAQI(sensorState.epa_aqi, currentTime);
      }

#ifdef BMP280_ENABLED
      if (live & GROUP_BMP280) {
        if (!isnan(sensorState.bmp_pressure_pa)) {
          publishBMP280Pressure(sensorState.bmp_pressure_pa, currentTime);
        }
        if (!isnan(sensorState.bmp_temp_c)) {
          publishBMP280Temperature(sensorState.bmp_temp_c, currentTime);
        }
      }
#endif

#ifdef AHT20_ENABLED
      if (live & GROUP_AHT20) {
        if (!isnan(sensorState.aht_temp_c)) {
          publishAHT20Temperature(sensorState.aht_temp_c, currentTime);
        }
        if (!isnan(sensorState.aht_humi_pct)) {
          publishAHT20Humidity(sensorState.aht_humi_pct, currentTime);
        }
      }
#endif

      // 4. Publish WiFi
      if (sysState.wifiSsid[0] != '\0') {
        if (_lastWifiStatusPublishTime == 0 ||
            (currentTime - _lastWifiStatusPublishTime >
             WIFI_STATUS_FORCE_INTERVAL_MS) ||
            sysState.wifiConnected != _lastPublishedWifiConnected ||
            abs(sysState.wifiRssi - _lastPublishedWifiRssi) >=
                WIFI_RSSI_DELTA_DBM ||
            strcmp(sysState.wifiSsid, _lastPublishedWifiSSID) != 0 ||
            strcmp(sysState.wifiIp, _lastPublishedWifiIP) != 0) {
          _wifi_rssi.setValue(sysState.wifiRssi);
          _wifi_ssid.setValue(sysState.wifiSsid);
          _wifi_ip.setValue(sysState.wifiIp);
          _lastPublishedWifiRssi = sysState.wifiRssi;
          _lastPublishedWifiConnected = sysState.wifiConnected;
          snprintf(_lastPublishedWifiSSID, sizeof(_lastPublishedWifiSSID), "%s",
                   sysState.wifiSsid);
          snprintf(_lastPublishedWifiIP, sizeof(_lastPublishedWifiIP), "%s",
                   sysState.wifiIp);
          _lastWifiStatusPublishTime = currentTime;
        }
      }

      // 5. Publish SensorStack health
      if (sysState.sensorConnectionStatus != _lastPublishedSensorConnected ||
          _lastSensorStatusPublishTime == 0) {
        _sensorStatus.setState(sysState.sensorConnectionStatus);
        _lastPublishedSensorConnected = sysState.sensorConnectionStatus;
        _lastSensorStatusPublishTime = currentTime;
      }
      if (sysState.sensorStackVersionUnavailable) {
        if (!_lastPublishedSensorStackVersion.equals("unavailable")) {
          _sensorStackVersionSensor.setValue(nullptr);
          _lastPublishedSensorStackVersion = "unavailable";
        }
      } else if (sysState.sensorStackVersion[0] != '\0' &&
                 !_lastPublishedSensorStackVersion.equals(
                     sysState.sensorStackVersion)) {
        _sensorStackVersionSensor.setValue(sysState.sensorStackVersion);
        _lastPublishedSensorStackVersion = sysState.sensorStackVersion;
      }

      publishSensorStackUptimeHelper(sysState.sensorStackUptime,
                                     sysState.sensorStackUptimeForce);
      if (sysState.sensorStackUptimeForce) {
        SystemStatusAccessor status;
        status->sensorStackUptimeForce = false;
      }

      if (_lastSensorStackFreeRamPublishTime == 0 ||
          (currentTime - _lastSensorStackFreeRamPublishTime >=
           STEADY_INTERVAL_60S_MS)) {
        if (sysState.sensorStackFreeRam > 0) {
          _sensorStackFreeRamSensor.setValue(sysState.sensorStackFreeRam, true);
          _lastSensorStackFreeRamPublishTime = currentTime;
        }
      }
      // HASensor::setValue(const char*) publishes every time: only on change
      if (sysState.nanoResetCause[0] != '\0' &&
          strcmp(sysState.nanoResetCause, _lastPublishedResetCause) != 0) {
        _sensorStackResetCauseSensor.setValue(sysState.nanoResetCause);
        snprintf(_lastPublishedResetCause, sizeof(_lastPublishedResetCause),
                 "%s", sysState.nanoResetCause);
      }

      // 6. Publish ESP32 health
      if (sysState.esp32FreeHeap > 0) {
        if (_lastEsp32FreeRamPublishTime == 0 ||
            (currentTime - _lastEsp32FreeRamPublishTime >=
             STEADY_INTERVAL_60S_MS)) {
          _esp32FreeRamSensor.setValue(sysState.esp32FreeHeap, true);
          _lastEsp32FreeRamPublishTime = currentTime;
        }
        if (_lastEsp32UptimePublishTime == 0 ||
            (currentTime - _lastEsp32UptimePublishTime >=
             STEADY_INTERVAL_60S_MS)) {
          _esp32UptimeSensor.setValue(sysState.esp32Uptime);
          _lastEsp32UptimePublishTime = currentTime;
        }
      }
    }

    _taskHeartbeatMs.store(millis() | 1); // Never 0 once running
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void HomeAssistantManager::drainLogQueue() {
  if (!_logQueue || !_mqtt.isConnected())
    return;
  LogQueueEntry entry;
  int count = 0;
  while (count < 5 && xQueueReceive(_logQueue, &entry, 0) == pdTRUE) {
    char payload[ILogPublisher::MAX_LOG_MESSAGE_LENGTH + 128];
    const char *levelStr = "INFO";
    switch (entry.level) {
    case APP_LOG_DEBUG:
      levelStr = "DEBUG";
      break;
    case APP_LOG_INFO:
      levelStr = "INFO";
      break;
    case APP_LOG_WARNING:
      levelStr = "WARNING";
      break;
    case APP_LOG_ERROR:
      levelStr = "ERROR";
      break;
    }
    snprintf(payload, sizeof(payload),
             "{\"level\": \"%s\", \"message\": \"%s\"}", levelStr,
             entry.message);
    _mqtt.publish(MQTT_LOG_TOPIC, payload);
    count++;
  }
}

void HomeAssistantManager::queueLogMessage(AppLogLevel level,
                                           const char *message) {
  if (!_logQueue || !message)
    return;
  LogQueueEntry entry;
  entry.level = level;
  strncpy(entry.message, message, sizeof(entry.message) - 1);
  entry.message[sizeof(entry.message) - 1] = '\0';
  xQueueSend(_logQueue, &entry, 0);
}

void HomeAssistantManager::resetAllPublishTimes() {
  _lastPressurePublishTime = 0;
  _lastFilterPressurePublishTime = 0;
  _lastCpmPublishTime = 0;
  _lastTempPublishTime = 0;
  _lastHumiPublishTime = 0;
  _LastO3PublishTime = 0;
  _lastNO2PublishTime = 0;
  _lastFastAQIPublishTime = 0;
  _lastEPAAQIPublishTime = 0;
  _lastWifiStatusPublishTime = 0;
  _lastSensorStatusPublishTime = 0;
  _lastHighPressurePublishTime = 0;
  _lastPmPublishTime = 0;
  _lastCo2PublishTime = 0;
  _lastVocPublishTime = 0;
  _lastNOxPublishTime = 0;
  _lastCoPublishTime = 0;
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
  _lastSensorStackUptimePublishTime = 0;
  _lastSensorStackFreeRamPublishTime = 0;
  _lastEsp32FreeRamPublishTime = 0;
  _lastEsp32UptimePublishTime = 0;
  _lastCompressorAmpsPublishTime = 0;
  _lastGeothermalPumpAmpsPublishTime = 0;
  _lastLiquidLevelPublishTime = 0;
  _lastInactivityTimerDelayPublishTime = 0;
  _lastPublishedResetCause[0] = '\0';
  _unknownGroups = 0;
  _geigerUnavailablePublished = false;
  _vocUnavailablePublished = false;
  _noxUnavailablePublished = false;
  _o3UnavailablePublished = false;
  _no2UnavailablePublished = false;
  _fastAqiUnavailablePublished = false;
  _epaAqiUnavailablePublished = false;
  _zmodUnavailablePublished = false;
  _hasPublishedScd30AutoCal = false;
  _hasPublishedScd30ForceCal = false;
  _hasPublishedBacklight = false;
  _hasPublishedInactivityDelay = false;
  _hasPublishedLogLevel = false;
}

void HomeAssistantManager::updateUnknownGroups(uint8_t unknown) {
  uint8_t newly_unknown = unknown & ~_unknownGroups;
  uint8_t recovered = _unknownGroups & ~unknown;
  if (newly_unknown) {
    publishGroupsUnknown(newly_unknown);
  }
  if (recovered) {
    resetGroupPublishTimes(recovered);
  }
  _unknownGroups = unknown;
}

void HomeAssistantManager::publishGroupsUnknown(uint8_t groups) {
  if (groups & GROUP_STACK) {
    _pressureSensor.HASensor::setValue(nullptr);
    _filterPressureSensor.HASensor::setValue(nullptr);
    _geiger_cpm.HASensor::setValue(nullptr);
    _geiger_dose.HASensor::setValue(nullptr);
    _currentSensor.HASensor::setValue(nullptr);
    _co_sensor.HASensor::setValue(nullptr);
    _compressorCurrentSensor.HASensor::setValue(nullptr);
    _geothermalPumpCurrentSensor.HASensor::setValue(nullptr);
    _liquidLevelSensor.setUnknown();
    _highPressureSensor.setUnknown();
    _fanStatus.setUnknown();
  }
  if (groups & GROUP_SCD30) {
    _temperatureSensor.HASensor::setValue(nullptr);
    _humiditySensor.HASensor::setValue(nullptr);
    _co2_sensor.HASensor::setValue(nullptr);
  }
  if (groups & GROUP_SPS30) {
    _pm1_0_sensor.HASensor::setValue(nullptr);
    _pm2_5_sensor.HASensor::setValue(nullptr);
    _pm4_0_sensor.HASensor::setValue(nullptr);
    _pm10_0_sensor.HASensor::setValue(nullptr);
  }
  if (groups & GROUP_SGP41) {
    _voc_index_sensor.HASensor::setValue(nullptr);
    _nox_index_sensor.HASensor::setValue(nullptr);
  }
  if (groups & GROUP_ZMOD) {
    _zmodUnavailablePublished = false;
    publishZmodUnavailable(millis());
  }
#ifdef BMP280_ENABLED
  if (groups & GROUP_BMP280) {
    _bmp280PressureSensor.HASensor::setValue(nullptr);
    _bmp280TemperatureSensor.HASensor::setValue(nullptr);
  }
#endif
#ifdef AHT20_ENABLED
  if (groups & GROUP_AHT20) {
    _aht20TemperatureSensor.HASensor::setValue(nullptr);
    _aht20HumiditySensor.HASensor::setValue(nullptr);
  }
#endif
}

// A recovered group publishes its first valid readings at once, with force
void HomeAssistantManager::resetGroupPublishTimes(uint8_t groups) {
  if (groups & GROUP_STACK) {
    _lastPressurePublishTime = 0;
    _lastFilterPressurePublishTime = 0;
    _lastCpmPublishTime = 0;
    _geigerUnavailablePublished = false;
    _lastAmpsPublishTime = 0;
    _lastCoPublishTime = 0;
    _lastCompressorAmpsPublishTime = 0;
    _lastGeothermalPumpAmpsPublishTime = 0;
    _lastLiquidLevelPublishTime = 0;
    _lastHighPressurePublishTime = 0;
    _lastFanStatusPublishTime = 0;
  }
  if (groups & GROUP_SCD30) {
    _lastTempPublishTime = 0;
    _lastHumiPublishTime = 0;
    _lastCo2PublishTime = 0;
  }
  if (groups & GROUP_SPS30) {
    _lastPmPublishTime = 0;
  }
  if (groups & GROUP_SGP41) {
    _lastVocPublishTime = 0;
    _lastNOxPublishTime = 0;
    _vocUnavailablePublished = false;
    _noxUnavailablePublished = false;
  }
  if (groups & GROUP_ZMOD) {
    _LastO3PublishTime = 0;
    _lastNO2PublishTime = 0;
    _lastFastAQIPublishTime = 0;
    _lastEPAAQIPublishTime = 0;
    _o3UnavailablePublished = false;
    _no2UnavailablePublished = false;
    _fastAqiUnavailablePublished = false;
    _epaAqiUnavailablePublished = false;
  }
#ifdef BMP280_ENABLED
  if (groups & GROUP_BMP280) {
    _lastBMP280PressurePublishTime = 0;
    _lastBMP280TemperaturePublishTime = 0;
  }
#endif
#ifdef AHT20_ENABLED
  if (groups & GROUP_AHT20) {
    _lastAHT20TemperaturePublishTime = 0;
    _lastAHT20HumidityPublishTime = 0;
  }
#endif
}

void HomeAssistantManager::onMqttStateChanged(HAMqtt::ConnectionState state) {
  if (!_instance)
    return;
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
    const char *reason;
    switch (state) {
    case HAMqtt::StateConnectionTimeout:
      reason = "no answer from broker";
      break;
    case HAMqtt::StateConnectionLost:
      reason = "connection lost";
      break;
    case HAMqtt::StateConnectionFailed:
      reason = "connection refused or broker unreachable";
      break;
    case HAMqtt::StateBadProtocol:
      reason = "protocol not supported";
      break;
    case HAMqtt::StateBadClientId:
      reason = "client ID rejected";
      break;
    case HAMqtt::StateUnavailable:
      reason = "broker unavailable";
      break;
    case HAMqtt::StateBadCredentials:
      reason = "bad username or password";
      break;
    case HAMqtt::StateUnauthorized:
      reason = "not authorized";
      break;
    default:
      reason = "unknown error";
      break;
    }
    ConfigManagerAccessor config;
    logger.warningf("MQTT connection to %s:%d failed (%s, state %d); retrying "
                    "every 10 s (attempt %lu).",
                    config->getMqttHost(), config->getMqttPort(), reason,
                    (int)state, (unsigned long)attempts);
  }
}

void HomeAssistantManager::onMqttConnected() {
  if (!_instance)
    return;
  logger.info("MQTT connected.");
  _instance->_lastMqttConnected = true;
  SystemStatusAccessor()->mqttConnected = true;

  if (_instance->_configurationUrl[0] == '\0' &&
      WiFi.status() == WL_CONNECTED) {
    snprintf(_instance->_configurationUrl, sizeof(_instance->_configurationUrl),
             "http://%s", WiFi.localIP().toString().c_str());
    _instance->_device.setConfigurationUrl(_instance->_configurationUrl);
    logger.infof("Setting device configuration URL on MQTT connect: %s",
                 _instance->_configurationUrl);
  }

  _instance->_wifi_ssid.setValue(WIFI_SSID);
  _instance->_wifi_ip.setValue(WiFi.localIP().toString().c_str());

  uint8_t current_brightness = 0;
  {
    SystemStatusAccessor status;
    current_brightness = status->backlightBrightness;
    status->backlightState = (current_brightness > 0);
    status->backlightBrightness = current_brightness;
  }
  _instance->_backlight.setState(current_brightness > 0, true);
  _instance->_backlight.setBrightness(current_brightness, true);

  uint32_t inactivity_delay;
  {
    ConfigManagerAccessor config;
    inactivity_delay = config->getInactivityTimerDelay();
  }
  _instance->_inactivityTimerDelay.setState(inactivity_delay);

  _instance->resetAllPublishTimes();
}

void HomeAssistantManager::onMqttDisconnected() {
  if (!_instance)
    return;
  logger.warning("MQTT disconnected.");
  _instance->_lastMqttConnected = false;
  SystemStatusAccessor()->mqttConnected = false;
}

void HomeAssistantManager::publishDifferentialPressure(
    float pressure, unsigned long currentTime) {
  if (isnan(pressure))
    return;
  bool is_first = (_lastPressurePublishTime == 0);
  bool diff_pressure_fast =
      fabs(pressure - _lastPublishedPressure) >= DIFF_PRESSURE_FAST_DELTA_PA;
  bool diff_pressure_steady =
      (currentTime - _lastPressurePublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(pressure - _lastPublishedPressure) >=
           DIFF_PRESSURE_STEADY_DELTA_PA);
  if (is_first || diff_pressure_fast || diff_pressure_steady ||
      (currentTime - _lastPressurePublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _pressureSensor.setValue(pressure, true);
    _lastPublishedPressure = pressure;
    _lastPressurePublishTime = currentTime;
  }
}

void HomeAssistantManager::publishFilterPressure(float filter_pressure,
                                                 unsigned long currentTime) {
  if (filter_pressure <= 0.0f)
    return;
  bool is_first = (_lastFilterPressurePublishTime == 0);
  bool filter_pressure_fast =
      fabs(filter_pressure - _lastPublishedFilterPressure) >=
      FILTER_PRESSURE_FAST_DELTA_PA;
  bool filter_pressure_steady =
      (currentTime - _lastFilterPressurePublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(filter_pressure - _lastPublishedFilterPressure) >=
           FILTER_PRESSURE_STEADY_DELTA_PA);
  if (is_first || filter_pressure_fast || filter_pressure_steady ||
      (currentTime - _lastFilterPressurePublishTime >
       FORCE_PUBLISH_INTERVAL_MS)) {
    _filterPressureSensor.setValue(filter_pressure, true);
    _lastPublishedFilterPressure = filter_pressure;
    _lastFilterPressurePublishTime = currentTime;
  }
}

void HomeAssistantManager::publishGeiger(int cpm, unsigned long currentTime) {
  if (cpm < 0) {
    if (!_geigerUnavailablePublished) {
      _geiger_cpm.HASensor::setValue(nullptr);
      _geiger_dose.HASensor::setValue(nullptr);
      _geigerUnavailablePublished = true;
      _lastPublishedCpm = UNPUBLISHED_INT;
    }
    return;
  }
  _geigerUnavailablePublished = false;
  bool is_first = (_lastCpmPublishTime == 0);
  bool geiger_cpm_steady =
      (currentTime - _lastCpmPublishTime >= STEADY_INTERVAL_30S_MS &&
       cpm != _lastPublishedCpm);
  // Published at once when the count crosses the threshold, then at the usual pace
  bool geiger_cpm_spike =
      (cpm >= GEIGER_CPM_SPIKE_THRESHOLD && _lastPublishedCpm < GEIGER_CPM_SPIKE_THRESHOLD);
  if (is_first || geiger_cpm_steady || geiger_cpm_spike ||
      (currentTime - _lastCpmPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _geiger_cpm.setValue(static_cast<uint16_t>(cpm), true);
    float usv_h = cpm * GEIGER_CPM_TO_USV_H;
    _geiger_dose.setValue(usv_h, true);
    _lastPublishedCpm = cpm;
    _lastCpmPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishTemperature(float temp,
                                              unsigned long currentTime) {
  if (!SensorDefines::isValidTemperature(temp))
    return;
  bool is_first = (_lastTempPublishTime == 0);
  bool temp_fast = (currentTime - _lastTempPublishTime >= FAST_INTERVAL_5S_MS &&
                    fabs(temp - _lastPublishedTemp) >= TEMP_FAST_DELTA_DEG_C);
  bool temp_steady =
      (currentTime - _lastTempPublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(temp - _lastPublishedTemp) >= TEMP_STEADY_DELTA_DEG_C);
  if (is_first || temp_fast || temp_steady ||
      (currentTime - _lastTempPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _temperatureSensor.setValue(temp, true);
    _lastPublishedTemp = temp;
    _lastTempPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishHumidity(float humi,
                                           unsigned long currentTime) {
  if (!SensorDefines::isValidHumidity(humi))
    return;
  bool is_first = (_lastHumiPublishTime == 0);
  bool humi_fast = (currentTime - _lastHumiPublishTime >= FAST_INTERVAL_5S_MS &&
                    fabs(humi - _lastPublishedHumi) >= HUMI_FAST_DELTA_PCT);
  bool humi_steady =
      (currentTime - _lastHumiPublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(humi - _lastPublishedHumi) >= HUMI_STEADY_DELTA_PCT);
  if (is_first || humi_fast || humi_steady ||
      (currentTime - _lastHumiPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _humiditySensor.setValue(humi, true);
    _lastPublishedHumi = humi;
    _lastHumiPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishFanCurrent(float amps, float fan_thresh,
                                             unsigned long currentTime) {
  if (isnan(amps))
    return;
  bool is_first = (_lastAmpsPublishTime == 0);
  bool fan_state_changed =
      !is_first && ((amps > fan_thresh) != (_lastPublishedAmps > fan_thresh));
  bool fan_amps_steady =
      (currentTime - _lastAmpsPublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(amps - _lastPublishedAmps) >= FAN_AMPS_STEADY_DELTA_A);
  if (is_first || fan_state_changed || fan_amps_steady ||
      (currentTime - _lastAmpsPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _currentSensor.setValue(amps, true);
    _lastPublishedAmps = amps;
    _lastAmpsPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishCO2(float co2, unsigned long currentTime) {
  if (!SensorDefines::isValidCO2(co2))
    return;
  bool is_first = (_lastCo2PublishTime == 0);
  bool co2_fast = (currentTime - _lastCo2PublishTime >= FAST_INTERVAL_10S_MS &&
                   fabs(co2 - _lastPublishedCo2) >= CO2_FAST_DELTA_PPM);
  bool co2_steady =
      (currentTime - _lastCo2PublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(co2 - _lastPublishedCo2) >= CO2_STEADY_DELTA_PPM);
  if (is_first || co2_fast || co2_steady ||
      (currentTime - _lastCo2PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _co2_sensor.setValue(static_cast<uint16_t>(co2), true);
    _lastPublishedCo2 = co2;
    _lastCo2PublishTime = currentTime;
  }
}

void HomeAssistantManager::publishCO(float co_ppm, bool new_packet,
                                     unsigned long currentTime) {
  if (isnan(co_ppm) || co_ppm < 0.0f)
    return;
  bool is_first = (_lastCoPublishTime == 0);
  // Every packet while CO is above the alert threshold, and once more when it
  // drops back below, so Home Assistant sees the return to normal
  bool co_alert = new_packet && (co_ppm > CO_ALERT_THRESHOLD_PPM ||
                                 _lastPublishedCo > CO_ALERT_THRESHOLD_PPM);
  bool co_steady =
      (currentTime - _lastCoPublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(co_ppm - _lastPublishedCo) >= CO_STEADY_DELTA_PPM);
  if (is_first || co_alert || co_steady ||
      (currentTime - _lastCoPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _co_sensor.setValue(static_cast<uint16_t>(co_ppm), true);
    _lastPublishedCo = co_ppm;
    _lastCoPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishVOC(int32_t voc_index,
                                      unsigned long currentTime) {
  if (!SensorDefines::isValidGasIndex(voc_index)) {
    if (!_vocUnavailablePublished) {
      _voc_index_sensor.HASensor::setValue(nullptr);
      _vocUnavailablePublished = true;
      _lastPublishedVocIndex = UNPUBLISHED_INT32;
    }
    return;
  }
  _vocUnavailablePublished = false;
  bool is_first = (_lastVocPublishTime == 0);
  bool voc_fast = (currentTime - _lastVocPublishTime >= FAST_INTERVAL_10S_MS &&
                   abs(voc_index - _lastPublishedVocIndex) >= VOC_FAST_DELTA);
  bool voc_steady =
      (currentTime - _lastVocPublishTime >= STEADY_INTERVAL_60S_MS &&
       abs(voc_index - _lastPublishedVocIndex) >= VOC_STEADY_DELTA);
  if (is_first || voc_fast || voc_steady ||
      (currentTime - _lastVocPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _voc_index_sensor.setValue(voc_index, true);
    _lastPublishedVocIndex = voc_index;
    _lastVocPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishNOx(int32_t nox_index,
                                      unsigned long currentTime) {
  if (!SensorDefines::isValidGasIndex(nox_index)) {
    if (!_noxUnavailablePublished) {
      _nox_index_sensor.HASensor::setValue(nullptr);
      _noxUnavailablePublished = true;
      _lastPublishedNOxIndex = UNPUBLISHED_INT32;
    }
    return;
  }
  _noxUnavailablePublished = false;
  bool is_first = (_lastNOxPublishTime == 0);
  bool nox_fast = (currentTime - _lastNOxPublishTime >= FAST_INTERVAL_10S_MS &&
                   abs(nox_index - _lastPublishedNOxIndex) >= NOX_FAST_DELTA);
  bool nox_steady =
      (currentTime - _lastNOxPublishTime >= STEADY_INTERVAL_60S_MS &&
       abs(nox_index - _lastPublishedNOxIndex) >= NOX_STEADY_DELTA);
  if (is_first || nox_fast || nox_steady ||
      (currentTime - _lastNOxPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _nox_index_sensor.setValue(nox_index, true);
    _lastPublishedNOxIndex = nox_index;
    _lastNOxPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishPM(float pm1, float pm25, float pm4,
                                     float pm10, unsigned long currentTime) {
  if (isnan(pm1) || isnan(pm25) || isnan(pm4) || isnan(pm10))
    return;
  bool is_first = (_lastPmPublishTime == 0);
  bool pm_fast = (currentTime - _lastPmPublishTime >= FAST_INTERVAL_10S_MS &&
                  (fabs(pm1 - _lastPublishedPm1_0) >= PM_FAST_DELTA ||
                   fabs(pm25 - _lastPublishedPm2_5) >= PM_FAST_DELTA ||
                   fabs(pm4 - _lastPublishedPm4_0) >= PM_FAST_DELTA ||
                   fabs(pm10 - _lastPublishedPm10_0) >= PM_FAST_DELTA));
  bool pm_steady =
      (currentTime - _lastPmPublishTime >= STEADY_INTERVAL_60S_MS &&
       (fabs(pm1 - _lastPublishedPm1_0) >= PM_STEADY_DELTA ||
        fabs(pm25 - _lastPublishedPm2_5) >= PM_STEADY_DELTA ||
        fabs(pm4 - _lastPublishedPm4_0) >= PM_STEADY_DELTA ||
        fabs(pm10 - _lastPublishedPm10_0) >= PM_STEADY_DELTA));
  if (is_first || pm_fast || pm_steady ||
      (currentTime - _lastPmPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _pm1_0_sensor.setValue(pm1, true);
    _pm2_5_sensor.setValue(pm25, true);
    _pm4_0_sensor.setValue(pm4, true);
    _pm10_0_sensor.setValue(pm10, true);
    _lastPublishedPm1_0 = pm1;
    _lastPublishedPm2_5 = pm25;
    _lastPublishedPm4_0 = pm4;
    _lastPublishedPm10_0 = pm10;
    _lastPmPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishCompressorCurrent(float compressor_amps,
                                                    float thresh,
                                                    unsigned long currentTime) {
  if (isnan(compressor_amps))
    return;
  bool is_first = (_lastCompressorAmpsPublishTime == 0);
  bool state_changed = !is_first && ((compressor_amps > thresh) !=
                                     (_lastPublishedCompressorAmps > thresh));
  bool steady =
      (currentTime - _lastCompressorAmpsPublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(compressor_amps - _lastPublishedCompressorAmps) >=
           COMPRESSOR_AMPS_STEADY_DELTA_A);
  if (is_first || state_changed || steady ||
      (currentTime - _lastCompressorAmpsPublishTime >
       FORCE_PUBLISH_INTERVAL_MS)) {
    _compressorCurrentSensor.setValue(compressor_amps, true);
    _lastPublishedCompressorAmps = compressor_amps;
    _lastCompressorAmpsPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishGeothermalPumpCurrent(
    float geothermal_pump_amps, float thresh, unsigned long currentTime) {
  if (isnan(geothermal_pump_amps))
    return;
  bool is_first = (_lastGeothermalPumpAmpsPublishTime == 0);
  bool state_changed =
      !is_first && ((geothermal_pump_amps > thresh) !=
                    (_lastPublishedGeothermalPumpAmps > thresh));
  bool steady =
      (currentTime - _lastGeothermalPumpAmpsPublishTime >=
           STEADY_INTERVAL_60S_MS &&
       fabs(geothermal_pump_amps - _lastPublishedGeothermalPumpAmps) >=
           PUMP_AMPS_STEADY_DELTA_A);
  if (is_first || state_changed || steady ||
      (currentTime - _lastGeothermalPumpAmpsPublishTime >
       FORCE_PUBLISH_INTERVAL_MS)) {
    _geothermalPumpCurrentSensor.setValue(geothermal_pump_amps, true);
    _lastPublishedGeothermalPumpAmps = geothermal_pump_amps;
    _lastGeothermalPumpAmpsPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishLiquidLevel(bool liquid_level_sensor_state,
                                              unsigned long currentTime) {
  bool is_first = (_lastLiquidLevelPublishTime == 0);
  if (is_first || liquid_level_sensor_state != _lastPublishedLiquidLevelState ||
      (currentTime - _lastLiquidLevelPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _liquidLevelSensor.setState(liquid_level_sensor_state, true);
    _lastPublishedLiquidLevelState = liquid_level_sensor_state;
    _lastLiquidLevelPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishHighPressure(bool is_high) {
  unsigned long currentTime = millis();
  bool is_first = (_lastHighPressurePublishTime == 0);
  if (is_first || is_high != _lastPublishedHighPressure ||
      (currentTime - _lastHighPressurePublishTime >
       FORCE_PUBLISH_INTERVAL_MS)) {
    _highPressureSensor.setState(is_high, true);
    _lastPublishedHighPressure = is_high;
    _lastHighPressurePublishTime = currentTime;
  }
}

void HomeAssistantManager::publishFan(bool is_on) {
  unsigned long currentTime = millis();
  bool is_first = (_lastFanStatusPublishTime == 0);
  if (is_first || is_on != _lastPublishedFanStatus ||
      (currentTime - _lastFanStatusPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _fanStatus.setState(is_on, true);
    _lastPublishedFanStatus = is_on;
    _lastFanStatusPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishSensorStackUptimeHelper(
    uint32_t uptime_seconds, bool force) {
  unsigned long currentTime = millis();
  bool is_first = (_lastSensorStackUptimePublishTime == 0);
  if (force || is_first ||
      (currentTime - _lastSensorStackUptimePublishTime >=
       STEADY_INTERVAL_60S_MS)) {
    _sensorStackUptimeSensor.setValue(uptime_seconds);
    _lastSensorStackUptimePublishTime = currentTime;
    if (force) {
      logger.debugf("Forced publish of SensorStack Uptime: %lu s",
                    (unsigned long)uptime_seconds);
    }
  }
}

void HomeAssistantManager::publishO3(float o3, unsigned long currentTime) {
  if (isnan(o3) || o3 < 0.0f) {
    if (!_o3UnavailablePublished) {
      _o3_sensor.HASensor::setValue(nullptr);
      _o3UnavailablePublished = true;
      _lastPublishedO3 = UNPUBLISHED_FLOAT;
    }
    return;
  }
  _o3UnavailablePublished = false;
  _zmodUnavailablePublished = false;
  bool o3_first = (_LastO3PublishTime == 0);
  bool o3_fast = (fabs(o3 - _lastPublishedO3) >= O3_FAST_DELTA_UG_M3);
  bool o3_steady =
      (currentTime - _LastO3PublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(o3 - _lastPublishedO3) >= O3_STEADY_DELTA_UG_M3);
  if (o3_first || o3_fast || o3_steady ||
      (currentTime - _LastO3PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _o3_sensor.setValue(o3, true);
    _lastPublishedO3 = o3;
    _LastO3PublishTime = currentTime;
  }
}

void HomeAssistantManager::publishNO2(float no2, unsigned long currentTime) {
  if (isnan(no2) || no2 < 0.0f) {
    if (!_no2UnavailablePublished) {
      _no2_sensor.HASensor::setValue(nullptr);
      _no2UnavailablePublished = true;
      _lastPublishedNO2 = UNPUBLISHED_FLOAT;
    }
    return;
  }
  _no2UnavailablePublished = false;
  _zmodUnavailablePublished = false;
  bool no2_first = (_lastNO2PublishTime == 0);
  bool no2_fast = (fabs(no2 - _lastPublishedNO2) >= NO2_FAST_DELTA_UG_M3);
  bool no2_steady =
      (currentTime - _lastNO2PublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(no2 - _lastPublishedNO2) >= NO2_STEADY_DELTA_UG_M3);
  if (no2_first || no2_fast || no2_steady ||
      (currentTime - _lastNO2PublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _no2_sensor.setValue(no2, true);
    _lastPublishedNO2 = no2;
    _lastNO2PublishTime = currentTime;
  }
}

void HomeAssistantManager::publishFastAQI(uint16_t fast_aqi,
                                          unsigned long currentTime) {
  if (fast_aqi == UNPUBLISHED_AQI) {
    if (!_fastAqiUnavailablePublished) {
      _fast_aqi_sensor.HASensor::setValue(nullptr);
      _fastAqiUnavailablePublished = true;
      _lastPublishedFastAQI = UNPUBLISHED_AQI;
    }
    return;
  }
  if (fast_aqi > SensorDefines::EPA_AQI_MAX) {
    fast_aqi = SensorDefines::EPA_AQI_MAX;
  }
  _fastAqiUnavailablePublished = false;
  _zmodUnavailablePublished = false;
  bool fast_aqi_first = (_lastFastAQIPublishTime == 0);
  bool fast_aqi_fast =
      (abs((int)fast_aqi - (int)_lastPublishedFastAQI) >= FAST_AQI_FAST_DELTA);
  bool fast_aqi_steady =
      (currentTime - _lastFastAQIPublishTime >= STEADY_INTERVAL_60S_MS &&
       fast_aqi != _lastPublishedFastAQI);
  if (fast_aqi_first || fast_aqi_fast || fast_aqi_steady ||
      (currentTime - _lastFastAQIPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _fast_aqi_sensor.setValue(fast_aqi, true);
    _lastPublishedFastAQI = fast_aqi;
    _lastFastAQIPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishEPAAQI(uint16_t epa_aqi,
                                         unsigned long currentTime) {
  if (epa_aqi == UNPUBLISHED_AQI) {
    if (!_epaAqiUnavailablePublished) {
      _epa_aqi_sensor.HASensor::setValue(nullptr);
      _epaAqiUnavailablePublished = true;
      _lastPublishedEPAAQI = UNPUBLISHED_AQI;
    }
    return;
  }
  if (epa_aqi > SensorDefines::EPA_AQI_MAX) {
    epa_aqi = SensorDefines::EPA_AQI_MAX;
  }
  _epaAqiUnavailablePublished = false;
  _zmodUnavailablePublished = false;
  bool epa_aqi_first = (_lastEPAAQIPublishTime == 0);
  bool epa_aqi_fast =
      (abs((int)epa_aqi - (int)_lastPublishedEPAAQI) >= EPA_AQI_FAST_DELTA);
  bool epa_aqi_steady =
      (currentTime - _lastEPAAQIPublishTime >= STEADY_INTERVAL_60S_MS &&
       epa_aqi != _lastPublishedEPAAQI);
  if (epa_aqi_first || epa_aqi_fast || epa_aqi_steady ||
      (currentTime - _lastEPAAQIPublishTime > FORCE_PUBLISH_INTERVAL_MS)) {
    _epa_aqi_sensor.setValue(epa_aqi, true);
    _lastPublishedEPAAQI = epa_aqi;
    _lastEPAAQIPublishTime = currentTime;
  }
}

void HomeAssistantManager::publishZmodUnavailable(unsigned long currentTime) {
  if (!_zmodUnavailablePublished) {
    _o3_sensor.HASensor::setValue(nullptr);
    _no2_sensor.HASensor::setValue(nullptr);
    _fast_aqi_sensor.HASensor::setValue(nullptr);
    _epa_aqi_sensor.HASensor::setValue(nullptr);
    _zmodUnavailablePublished = true;
    _o3UnavailablePublished = true;
    _no2UnavailablePublished = true;
    _fastAqiUnavailablePublished = true;
    _epaAqiUnavailablePublished = true;
    _lastPublishedO3 = UNPUBLISHED_FLOAT;
    _lastPublishedNO2 = UNPUBLISHED_FLOAT;
    _lastPublishedFastAQI = UNPUBLISHED_AQI;
    _lastPublishedEPAAQI = UNPUBLISHED_AQI;
  }
}

#ifdef BMP280_ENABLED
void HomeAssistantManager::publishBMP280Pressure(float pressure_pa,
                                                 unsigned long currentTime) {
  bool pressure_first = (_lastBMP280PressurePublishTime == 0);
  bool pressure_fast = fabs(pressure_pa - _lastPublishedBMP280Pressure) >=
                       BMP280_PRESSURE_FAST_DELTA_PA;
  bool pressure_steady =
      (currentTime - _lastBMP280PressurePublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(pressure_pa - _lastPublishedBMP280Pressure) >=
           BMP280_PRESSURE_STEADY_DELTA_PA);
  if (pressure_first || pressure_fast || pressure_steady ||
      (currentTime - _lastBMP280PressurePublishTime >
       FORCE_PUBLISH_INTERVAL_MS)) {
    _bmp280PressureSensor.setValue(pressure_pa, true);
    _lastPublishedBMP280Pressure = pressure_pa;
    _lastBMP280PressurePublishTime = currentTime;
  }
}

void HomeAssistantManager::publishBMP280Temperature(float temp_c,
                                                    unsigned long currentTime) {
  bool temp_first = (_lastBMP280TemperaturePublishTime == 0);
  bool temp_fast =
      (currentTime - _lastBMP280TemperaturePublishTime >= FAST_INTERVAL_5S_MS &&
       fabs(temp_c - _lastPublishedBMP280Temperature) >=
           BMP280_TEMP_FAST_DELTA_DEG_C);
  bool temp_steady = (currentTime - _lastBMP280TemperaturePublishTime >=
                          STEADY_INTERVAL_60S_MS &&
                      fabs(temp_c - _lastPublishedBMP280Temperature) >=
                          BMP280_TEMP_STEADY_DELTA_DEG_C);
  if (temp_first || temp_fast || temp_steady ||
      (currentTime - _lastBMP280TemperaturePublishTime >
       FORCE_PUBLISH_INTERVAL_MS)) {
    _bmp280TemperatureSensor.setValue(temp_c, true);
    _lastPublishedBMP280Temperature = temp_c;
    _lastBMP280TemperaturePublishTime = currentTime;
  }
}
#endif

#ifdef AHT20_ENABLED
void HomeAssistantManager::publishAHT20Temperature(float temp_c,
                                                   unsigned long currentTime) {
  bool temp_first = (_lastAHT20TemperaturePublishTime == 0);
  bool temp_fast =
      (currentTime - _lastAHT20TemperaturePublishTime >= FAST_INTERVAL_5S_MS &&
       fabs(temp_c - _lastPublishedAHT20Temperature) >=
           AHT20_TEMP_FAST_DELTA_DEG_C);
  bool temp_steady = (currentTime - _lastAHT20TemperaturePublishTime >=
                          STEADY_INTERVAL_60S_MS &&
                      fabs(temp_c - _lastPublishedAHT20Temperature) >=
                          AHT20_TEMP_STEADY_DELTA_DEG_C);
  if (temp_first || temp_fast || temp_steady ||
      (currentTime - _lastAHT20TemperaturePublishTime >
       FORCE_PUBLISH_INTERVAL_MS)) {
    _aht20TemperatureSensor.setValue(temp_c, true);
    _lastPublishedAHT20Temperature = temp_c;
    _lastAHT20TemperaturePublishTime = currentTime;
  }
}

void HomeAssistantManager::publishAHT20Humidity(float humi_pct,
                                                unsigned long currentTime) {
  bool humi_first = (_lastAHT20HumidityPublishTime == 0);
  bool humi_fast =
      (currentTime - _lastAHT20HumidityPublishTime >= FAST_INTERVAL_5S_MS &&
       fabs(humi_pct - _lastPublishedAHT20Humidity) >=
           AHT20_HUMI_FAST_DELTA_PCT);
  bool humi_steady =
      (currentTime - _lastAHT20HumidityPublishTime >= STEADY_INTERVAL_60S_MS &&
       fabs(humi_pct - _lastPublishedAHT20Humidity) >=
           AHT20_HUMI_STEADY_DELTA_PCT);
  if (humi_first || humi_fast || humi_steady ||
      (currentTime - _lastAHT20HumidityPublishTime >
       FORCE_PUBLISH_INTERVAL_MS)) {
    _aht20HumiditySensor.setValue(humi_pct, true);
    _lastPublishedAHT20Humidity = humi_pct;
    _lastAHT20HumidityPublishTime = currentTime;
  }
}
#endif

void HomeAssistantManager::onRebootCommand(HAButton *sender) {
  sender->setAvailability(false);
  MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_REBOOT_ESP32);
}

void HomeAssistantManager::onRebootSensorStackCommand(HAButton *sender) {
  logger.warning("Reboot command sent to SensorStack from Home Assistant.");
  MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_REBOOT_NANO);
}

void HomeAssistantManager::onGetSps30InfoCommand(HAButton *sender) {
  logger.info(
      "Get SPS30 Info command sent to SensorStack from Home Assistant.");
  MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_SPS30_GET_INFO);
}

void HomeAssistantManager::onGetSps30ManualCleanCommand(HAButton *sender) {
  logger.info("Get SPS30 Manual Fan Cleaning request sent to SensorStack from "
              "Home Assistant.");
  MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_SPS30_CLEAN);
}

void HomeAssistantManager::onGetSgp41SelftestCommand(HAButton *sender) {
  logger.info(
      "SGP41 Self-Test command sent to SensorStack from Home Assistant.");
  MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_SGP41_TEST);
}

void HomeAssistantManager::onLogLevelCommand(int8_t index, HASelect *sender) {
  if (!_instance)
    return;
  if (index < 0 || index > 3) {
    logger.warningf(
        "Rejected invalid log level from Home Assistant: %d (allowed 0-3)",
        index);
    sender->setState(sender->getCurrentState(), true);
    return;
  }
  AppLogLevel level = static_cast<AppLogLevel>(index);
  {
    ConfigManagerAccessor config;
    config->setLogLevel(level);
  }
  logger.setLogLevel(level);
  SystemStatusAccessor()->logLevel = level;
  sender->setState(index);
}

void HomeAssistantManager::onBrightnessCommand(uint8_t brightness,
                                               HALight *sender) {
  if (!_instance)
    return;
  logger.infof("Backlight brightness %u command from Home Assistant.",
               brightness);
  {
    SystemStatusAccessor status;
    status->backlightState = (brightness > 0);
    status->backlightBrightness = brightness;
  }
  sender->setBrightness(brightness);
  if (brightness > 0 && !sender->getCurrentState()) {
    sender->setState(true);
  }
}

void HomeAssistantManager::onStateCommand(bool state, HALight *sender) {
  if (!_instance)
    return;
  logger.infof("Backlight state %s command from Home Assistant.",
               state ? "ON" : "OFF");
  {
    SystemStatusAccessor status;
    uint8_t curB = status->backlightBrightness;
    status->backlightState = state;
    status->backlightBrightness = state ? (curB == 0 ? 255 : curB) : 0;
  }
  // Publishing can block on the network: never with a shared lock held
  sender->setState(state);
}

void HomeAssistantManager::onScd30AutoCalCommand(bool state, HASwitch *sender) {
  if (!_instance)
    return;
  logger.infof("SCD30 AutoCalibration %s command from Home Assistant.",
               state ? "ON" : "OFF");
  // scd30AutoCalState only ever holds what the SensorStack confirmed: it is set
  // from the Nano's reply, and published from there
  MainTaskEventNotifier::getInstance().requestScd30AutoCal(state);
}

void HomeAssistantManager::onScd30ForceCalCommand(HANumeric number,
                                                  HANumber *sender) {
  int32_t ppm_raw = number.toInt32();
  if (ppm_raw < SensorDefines::SCD30_FORCECAL_MIN_PPM ||
      ppm_raw > SensorDefines::SCD30_FORCECAL_MAX_PPM) {
    logger.warningf("Rejected out-of-range SCD30 Force Cal from Home "
                    "Assistant: %d ppm (allowed %u-%u)",
                    ppm_raw, SensorDefines::SCD30_FORCECAL_MIN_PPM,
                    SensorDefines::SCD30_FORCECAL_MAX_PPM);
    sender->setState(sender->getCurrentState(), true);
    return;
  }
  uint16_t ppm_value = static_cast<uint16_t>(ppm_raw);
  logger.infof("SCD30 Force Calibration to %u ppm command from Home Assistant.",
               ppm_value);
  MainTaskEventNotifier::getInstance().requestScd30ForceCal(ppm_value);
}

void HomeAssistantManager::onInactivityTimerDelayCommand(HANumeric number,
                                                         HANumber *sender) {
  if (_instance) {
    int32_t delay = number.toInt32();
    if (delay < ConfigLimits::MIN_INACTIVITY_DELAY_S ||
        delay > ConfigLimits::MAX_INACTIVITY_DELAY_S) {
      logger.warningf("Rejected out-of-range inactivity timer delay from Home "
                      "Assistant: %d (allowed %d-%d)",
                      delay, ConfigLimits::MIN_INACTIVITY_DELAY_S,
                      ConfigLimits::MAX_INACTIVITY_DELAY_S);
      uint32_t current_delay;
      {
        ConfigManagerAccessor config;
        current_delay = config->getInactivityTimerDelay();
      }
      sender->setState(static_cast<int32_t>(current_delay), true);
      return;
    }
    {
      ConfigManagerAccessor config;
      config->setInactivityTimerDelay(delay);
    }
    {
      SystemStatusAccessor status;
      status->inactivityTimerDelay = delay;
      status->inactivityTimerDelayForce = true;
    }
    // Locks released first: publishing can block on the network
    sender->setState(delay);
    logger.infof(
        "Inactivity timer delay updated to %d seconds via Home Assistant",
        delay);
  }
}

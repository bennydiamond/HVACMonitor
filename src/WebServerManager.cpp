#include "WebServerManager.h"
#include <WiFi.h>
#include <Update.h>
#include "Logger.h"
#include "ConfigManager.h"
#include "HomeAssistantManager.h"

WebServer server(80);
String WebServerManager::_firmware_version;

const char* config_page_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>HVAC Sensor Configuration</title>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial, sans-serif; background-color: #f4f4f4; padding: 20px; }
.container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
h1 { color: #333; text-align: center; margin-bottom: 30px; }
.config-section { margin-bottom: 30px; }
.config-section h2 { color: #007bff; border-bottom: 2px solid #007bff; padding-bottom: 5px; }
.config-row { display: flex; align-items: center; margin-bottom: 15px; padding: 10px; background: #f8f9fa; border-radius: 5px; }
.config-label { flex: 1; font-weight: bold; color: #333; }
.config-input { flex: 1; margin: 0 10px; }
.config-input input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }
.config-button { background: #007bff; color: white; padding: 8px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
.config-button:hover { background: #0056b3; }
.config-button:disabled { background: #ccc; cursor: not-allowed; }
.status { margin-top: 10px; padding: 10px; border-radius: 4px; display: none; }
.status.success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
.status.error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
.nav { text-align: center; margin-bottom: 20px; }
.nav a { display: inline-block; margin: 0 10px; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 5px; }
.nav a:hover { background: #0056b3; }
.tab-buttons { display: flex; margin-bottom: 20px; border-bottom: 2px solid #007bff; }
.tab-button { flex: 1; padding: 10px 20px; background: #f8f9fa; border: none; border-bottom: 2px solid transparent; cursor: pointer; font-size: 14px; }
.tab-button.active { background: #007bff; color: white; border-bottom-color: #007bff; }
.tab-button:hover { background: #e9ecef; }
.tab-button.active:hover { background: #0056b3; }
.tab-content { display: none; }
.tab-content.active { display: block; }
.save-button { background: #28a745; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 20px; }
.save-button:hover { background: #218838; }
.save-button:disabled { background: #ccc; cursor: not-allowed; }
.reboot-message { background: #fff3cd; color: #856404; border: 1px solid #ffeaa7; padding: 15px; border-radius: 4px; margin-top: 20px; text-align: center; font-weight: bold; }
</style>
<script>
function updateThreshold(section, type) {
    const input = document.getElementById(section + '_' + type + '_input');
    const button = document.getElementById(section + '_' + type + '_button');
    const status = document.getElementById(section + '_' + type + '_status');
    
    button.disabled = true;
    button.textContent = 'Saving...';
    
    const formData = new FormData();
    formData.append('section', section);
    formData.append('type', type);
    formData.append('value', input.value);
    
    fetch('/config/update', {
        method: 'POST',
        body: formData
    })
    .then(response => response.text())
    .then(result => {
        status.textContent = result;
        status.className = 'status success';
        status.style.display = 'block';
        button.textContent = 'Set';
        button.disabled = false;
        
        setTimeout(() => {
            status.style.display = 'none';
        }, 3000);
    })
    .catch(error => {
        status.textContent = 'Error: ' + error;
        status.className = 'status error';
        status.style.display = 'block';
        button.textContent = 'Set';
        button.disabled = false;
    });
}

function showTab(tabName) {
    // Hide all tab contents
    const tabContents = document.querySelectorAll('.tab-content');
    tabContents.forEach(content => {
        content.classList.remove('active');
    });
    
    // Remove active class from all tab buttons
    const tabButtons = document.querySelectorAll('.tab-button');
    tabButtons.forEach(button => {
        button.classList.remove('active');
    });
    
    // Show selected tab content
    document.getElementById(tabName + '-tab').classList.add('active');
    
    // Add active class to clicked button
    event.target.classList.add('active');
}

function saveMqttConfig() {
    const saveButton = document.getElementById('mqtt-save-button');
    const status = document.getElementById('mqtt-save-status');
    const rebootMessage = document.getElementById('reboot-message');
    
    saveButton.disabled = true;
    saveButton.textContent = 'Saving...';
    
    const formData = new FormData();
    formData.append('section', 'mqtt_device');
    formData.append('action', 'save');
    formData.append('host', document.getElementById('mqtt_host_input').value);
    formData.append('port', document.getElementById('mqtt_port_input').value);
    formData.append('user', document.getElementById('mqtt_user_input').value);
    formData.append('password', document.getElementById('mqtt_password_input').value);
    formData.append('device_id', document.getElementById('mqtt_device_id_input').value);
    formData.append('device_name', document.getElementById('mqtt_device_name_input').value);
    
    fetch('/config/update', {
        method: 'POST',
        body: formData
    })
    .then(response => {
        if (!response.ok) {
            return response.text().then(text => {
                throw new Error(text);
            });
        }
        return response.text();
    })
    .then(result => {
        if (result.includes('Rebooting')) {
            status.textContent = 'Configuration saved successfully. Device will reboot in 3 seconds...';
            status.className = 'status success';
            status.style.display = 'block';
            rebootMessage.style.display = 'block';
            
            setTimeout(() => {
                rebootMessage.textContent = 'Device is rebooting... Please wait.';
            }, 1000);
            
            setTimeout(() => {
                rebootMessage.textContent = 'Device is rebooting... Please wait..';
            }, 2000);
            
            setTimeout(() => {
                rebootMessage.textContent = 'Device is rebooting... Please wait...';
            }, 3000);
        } else {
            status.textContent = result;
            status.className = 'status success';
            status.style.display = 'block';
            saveButton.textContent = 'Save Configuration';
            saveButton.disabled = false;
        }
    })
    .catch(error => {
        status.textContent = 'Error: ' + error.message;
        status.className = 'status error';
        status.style.display = 'block';
        saveButton.textContent = 'Save Configuration';
        saveButton.disabled = false;
    });
}
</script>
</head>
<body>
    <div class="container">
        <h1>HVAC Sensor Configuration</h1>
        
        <div class="nav">
            <a href="/">OTA Update</a>
            <a href="/config">Configuration</a>
        </div>
        
        <div class="tab-buttons">
            <button class="tab-button active" onclick="showTab('sensors')">Sensor Thresholds</button>
            <button class="tab-button" onclick="showTab('system')">System</button>
            <button class="tab-button" onclick="showTab('mqtt')">MQTT Device</button>
        </div>
        
        <!-- Sensor Thresholds Tab -->
        <div id="sensors-tab" class="tab-content active">
            <div class="config-section">
                <h2>Gas Sensor Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">NO2 Warning Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="no2_warn_input" value="%NO2_WARN%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('no2', 'warn')" id="no2_warn_button">Set</button>
                </div>
                <div id="no2_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">NO2 Danger Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="no2_danger_input" value="%NO2_DANGER%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('no2', 'danger')" id="no2_danger_button">Set</button>
                </div>
                <div id="no2_danger_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">O3 Warning Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="o3_warn_input" value="%O3_WARN%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('o3', 'warn')" id="o3_warn_button">Set</button>
                </div>
                <div id="o3_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">O3 Danger Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="o3_danger_input" value="%O3_DANGER%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('o3', 'danger')" id="o3_danger_button">Set</button>
                </div>
                <div id="o3_danger_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">NOx Warning Threshold (Index):</div>
                    <div class="config-input">
                        <input type="number" id="nox_warn_input" value="%NOX_WARN%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('nox', 'warn')" id="nox_warn_button">Set</button>
                </div>
                <div id="nox_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">NOx Danger Threshold (Index):</div>
                    <div class="config-input">
                        <input type="number" id="nox_danger_input" value="%NOX_DANGER%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('nox', 'danger')" id="nox_danger_button">Set</button>
                </div>
                <div id="nox_danger_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">CO Warning Threshold (ppm):</div>
                    <div class="config-input">
                        <input type="number" id="co_warn_input" value="%CO_WARN%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('co', 'warn')" id="co_warn_button">Set</button>
                </div>
                <div id="co_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">CO Danger Threshold (ppm):</div>
                    <div class="config-input">
                        <input type="number" id="co_danger_input" value="%CO_DANGER%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('co', 'danger')" id="co_danger_button">Set</button>
                </div>
                <div id="co_danger_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>PM Sensor Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">PM1.0 Warning Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="pm1_warn_input" value="%PM1_WARN%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pm1', 'warn')" id="pm1_warn_button">Set</button>
                </div>
                <div id="pm1_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">PM1.0 Danger Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="pm1_danger_input" value="%PM1_DANGER%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pm1', 'danger')" id="pm1_danger_button">Set</button>
                </div>
                <div id="pm1_danger_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">PM2.5 Warning Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="pm25_warn_input" value="%PM25_WARN%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pm25', 'warn')" id="pm25_warn_button">Set</button>
                </div>
                <div id="pm25_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">PM2.5 Danger Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="pm25_danger_input" value="%PM25_DANGER%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pm25', 'danger')" id="pm25_danger_button">Set</button>
                </div>
                <div id="pm25_danger_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">PM4.0 Warning Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="pm4_warn_input" value="%PM4_WARN%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pm4', 'warn')" id="pm4_warn_button">Set</button>
                </div>
                <div id="pm4_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">PM4.0 Danger Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="pm4_danger_input" value="%PM4_DANGER%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pm4', 'danger')" id="pm4_danger_button">Set</button>
                </div>
                <div id="pm4_danger_status" class="status"></div>
                
                <div class="config-row">
                <div class="config-label">PM10 Warning Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="pm10_warn_input" value="%PM10_WARN%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pm10', 'warn')" id="pm10_warn_button">Set</button>
                </div>
                <div id="pm10_warn_status" class="status"></div>
                
                <div class="config-row">
                <div class="config-label">PM10 Danger Threshold (µg/m³):</div>
                    <div class="config-input">
                        <input type="number" id="pm10_danger_input" value="%PM10_DANGER%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pm10', 'danger')" id="pm10_danger_button">Set</button>
                </div>
                <div id="pm10_danger_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>Differential Pressure Sensor Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">Pressure Low Threshold (Pa):</div>
                    <div class="config-input">
                        <input type="number" id="pressure_low_input" value="%PRESSURE_LOW%" min="0" max="1000" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pressure', 'low')" id="pressure_low_button">Set</button>
                </div>
                <div id="pressure_low_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Pressure Mid Threshold (Pa):</div>
                    <div class="config-input">
                        <input type="number" id="pressure_mid_input" value="%PRESSURE_MID%" min="0" max="1000" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pressure', 'mid')" id="pressure_mid_button">Set</button>
                </div>
                <div id="pressure_mid_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Pressure High Threshold (Pa):</div>
                    <div class="config-input">
                        <input type="number" id="pressure_high_input" value="%PRESSURE_HIGH%" min="0" max="1000" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pressure', 'high')" id="pressure_high_button">Set</button>
                </div>
                <div id="pressure_high_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>Fan Current Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">Fan On Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="fan_on_input" value="%FAN_ON%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('fan', 'on')" id="fan_on_button">Set</button>
                </div>
                <div id="fan_on_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Fan Off Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="fan_off_input" value="%FAN_OFF%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('fan', 'off')" id="fan_off_button">Set</button>
                </div>
                <div id="fan_off_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Fan High Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="fan_high_input" value="%FAN_HIGH%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('fan', 'high')" id="fan_high_button">Set</button>
                </div>
                <div id="fan_high_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>Compressor Current Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">Compressor On Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="compressor_on_input" value="%COMPRESSOR_ON%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('compressor', 'on')" id="compressor_on_button">Set</button>
                </div>
                <div id="compressor_on_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Compressor Off Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="compressor_off_input" value="%COMPRESSOR_OFF%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('compressor', 'off')" id="compressor_off_button">Set</button>
                </div>
                <div id="compressor_off_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Compressor High Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="compressor_high_input" value="%COMPRESSOR_HIGH%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('compressor', 'high')" id="compressor_high_button">Set</button>
                </div>
                <div id="compressor_high_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>Pump Current Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">Pump On Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="pump_on_input" value="%PUMP_ON%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pump', 'on')" id="pump_on_button">Set</button>
                </div>
                <div id="pump_on_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Pump Off Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="pump_off_input" value="%PUMP_OFF%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pump', 'off')" id="pump_off_button">Set</button>
                </div>
                <div id="pump_off_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Pump High Current Threshold (A):</div>
                    <div class="config-input">
                        <input type="number" id="pump_high_input" value="%PUMP_HIGH%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('pump', 'high')" id="pump_high_button">Set</button>
                </div>
                <div id="pump_high_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>Geiger Counter Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">Geiger Abnormal Low Threshold (µSv/h):</div>
                    <div class="config-input">
                        <input type="number" id="geiger_abnormal_low_input" value="%GEIGER_ABNORMAL_LOW%" min="0" max="10" step="0.01">
                    </div>
                    <button class="config-button" onclick="updateThreshold('geiger', 'abnormal_low')" id="geiger_abnormal_low_button">Set</button>
                </div>
                <div id="geiger_abnormal_low_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Geiger Abnormal High Threshold (µSv/h):</div>
                    <div class="config-input">
                        <input type="number" id="geiger_abnormal_high_input" value="%GEIGER_ABNORMAL_HIGH%" min="0" max="10" step="0.01">
                    </div>
                    <button class="config-button" onclick="updateThreshold('geiger', 'abnormal_high')" id="geiger_abnormal_high_button">Set</button>
                </div>
                <div id="geiger_abnormal_high_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Geiger Danger High Threshold (µSv/h):</div>
                    <div class="config-input">
                        <input type="number" id="geiger_danger_high_input" value="%GEIGER_DANGER_HIGH%" min="0" max="10" step="0.01">
                    </div>
                    <button class="config-button" onclick="updateThreshold('geiger', 'danger_high')" id="geiger_danger_high_button">Set</button>
                </div>
                <div id="geiger_danger_high_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>Temperature Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">Temperature Comfortable Low (°C):</div>
                    <div class="config-input">
                        <input type="number" id="temp_comfortable_low_input" value="%TEMP_COMFORTABLE_LOW%" min="-50" max="50" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('temp', 'comfortable_low')" id="temp_comfortable_low_button">Set</button>
                </div>
                <div id="temp_comfortable_low_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Temperature Comfortable High (°C):</div>
                    <div class="config-input">
                        <input type="number" id="temp_comfortable_high_input" value="%TEMP_COMFORTABLE_HIGH%" min="-50" max="50" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('temp', 'comfortable_high')" id="temp_comfortable_high_button">Set</button>
                </div>
                <div id="temp_comfortable_high_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Temperature Acceptable Low (°C):</div>
                    <div class="config-input">
                        <input type="number" id="temp_acceptable_low_input" value="%TEMP_ACCEPTABLE_LOW%" min="-50" max="50" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('temp', 'acceptable_low')" id="temp_acceptable_low_button">Set</button>
                </div>
                <div id="temp_acceptable_low_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Temperature Acceptable High (°C):</div>
                    <div class="config-input">
                        <input type="number" id="temp_acceptable_high_input" value="%TEMP_ACCEPTABLE_HIGH%" min="-50" max="50" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('temp', 'acceptable_high')" id="temp_acceptable_high_button">Set</button>
                </div>
                <div id="temp_acceptable_high_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>Humidity Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">Humidity Comfortable Low (%):</div>
                    <div class="config-input">
                        <input type="number" id="humi_comfortable_low_input" value="%HUMI_COMFORTABLE_LOW%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('humi', 'comfortable_low')" id="humi_comfortable_low_button">Set</button>
                </div>
                <div id="humi_comfortable_low_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Humidity Comfortable High (%):</div>
                    <div class="config-input">
                        <input type="number" id="humi_comfortable_high_input" value="%HUMI_COMFORTABLE_HIGH%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('humi', 'comfortable_high')" id="humi_comfortable_high_button">Set</button>
                </div>
                <div id="humi_comfortable_high_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Humidity Acceptable Low (%):</div>
                    <div class="config-input">
                        <input type="number" id="humi_acceptable_low_input" value="%HUMI_ACCEPTABLE_LOW%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('humi', 'acceptable_low')" id="humi_acceptable_low_button">Set</button>
                </div>
                <div id="humi_acceptable_low_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">Humidity Acceptable High (%):</div>
                    <div class="config-input">
                        <input type="number" id="humi_acceptable_high_input" value="%HUMI_ACCEPTABLE_HIGH%" min="0" max="100" step="0.1">
                    </div>
                    <button class="config-button" onclick="updateThreshold('humi', 'acceptable_high')" id="humi_acceptable_high_button">Set</button>
                </div>
                <div id="humi_acceptable_high_status" class="status"></div>
            </div>
            
            <div class="config-section">
                <h2>Other Sensor Thresholds</h2>
                
                <div class="config-row">
                    <div class="config-label">CO2 Warning Threshold (ppm):</div>
                    <div class="config-input">
                        <input type="number" id="co2_warn_input" value="%CO2_WARN%" min="0" max="10000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('co2', 'warn')" id="co2_warn_button">Set</button>
                </div>
                <div id="co2_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">CO2 Danger Threshold (ppm):</div>
                    <div class="config-input">
                        <input type="number" id="co2_danger_input" value="%CO2_DANGER%" min="0" max="10000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('co2', 'danger')" id="co2_danger_button">Set</button>
                </div>
                <div id="co2_danger_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">VOC Warning Threshold (Index):</div>
                    <div class="config-input">
                        <input type="number" id="voc_warn_input" value="%VOC_WARN%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('voc', 'warn')" id="voc_warn_button">Set</button>
                </div>
                <div id="voc_warn_status" class="status"></div>
                
                <div class="config-row">
                    <div class="config-label">VOC Danger Threshold (Index):</div>
                    <div class="config-input">
                        <input type="number" id="voc_danger_input" value="%VOC_DANGER%" min="0" max="1000">
                    </div>
                    <button class="config-button" onclick="updateThreshold('voc', 'danger')" id="voc_danger_button">Set</button>
                </div>
                <div id="voc_danger_status" class="status"></div>
            </div>
        </div>
        
        <!-- System Tab -->
        <div id="system-tab" class="tab-content">
            <div class="config-section">
                <h2>System Configuration</h2>
                
                <div class="config-row">
                    <div class="config-label">Inactivity Timer Delay (seconds):</div>
                    <div class="config-input">
                        <input type="number" id="inactivity_timer_delay_input" value="%INACTIVITY_TIMER_DELAY%" min="5" max="120" step="5">
                    </div>
                    <button class="config-button" onclick="updateThreshold('inactivity', 'timer_delay')" id="inactivity_timer_delay_button">Set</button>
                </div>
                <div id="inactivity_timer_delay_status" class="status"></div>
            </div>
        </div>
        
        <!-- MQTT Device Tab -->
        <div id="mqtt-tab" class="tab-content">
            <div class="config-section">
                <h2>MQTT Device Configuration</h2>
                <p style="color: #666; margin-bottom: 20px;">Changes to these settings will require a device reboot to take effect.</p>
                
                <div class="config-row">
                    <div class="config-label">MQTT Host:</div>
                    <div class="config-input">
                        <input type="text" id="mqtt_host_input" value="%MQTT_HOST%" maxlength="63">
                    </div>
                </div>
                
                <div class="config-row">
                    <div class="config-label">MQTT Port:</div>
                    <div class="config-input">
                        <input type="number" id="mqtt_port_input" value="%MQTT_PORT%" min="1" max="65535">
                    </div>
                </div>
                
                <div class="config-row">
                    <div class="config-label">MQTT Username:</div>
                    <div class="config-input">
                        <input type="text" id="mqtt_user_input" value="%MQTT_USER%" maxlength="31">
                    </div>
                </div>
                
                <div class="config-row">
                    <div class="config-label">MQTT Password:</div>
                    <div class="config-input">
                        <input type="password" id="mqtt_password_input" value="%MQTT_PASSWORD%" maxlength="63">
                    </div>
                </div>
                
                <div class="config-row">
                    <div class="config-label">MQTT Device ID:</div>
                    <div class="config-input">
                        <input type="text" id="mqtt_device_id_input" value="%MQTT_DEVICE_ID%" maxlength="31">
                    </div>
                </div>
                
                <div class="config-row">
                    <div class="config-label">MQTT Device Name:</div>
                    <div class="config-input">
                        <input type="text" id="mqtt_device_name_input" value="%MQTT_DEVICE_NAME%" maxlength="63">
                    </div>
                </div>
                
                <button class="save-button" onclick="saveMqttConfig()" id="mqtt-save-button">Save Configuration</button>
                <div id="mqtt-save-status" class="status"></div>
                <div id="reboot-message" class="reboot-message" style="display: none;">Device will reboot in 3 seconds...</div>
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";

const char* update_page_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>HVAC Sensor OTA Update</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial, sans-serif; background-color: #f4f4f4; text-align: center; padding: 50px; }
form { background: white; padding: 20px; border-radius: 8px; display: inline-block; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
input[type=file] { margin-bottom: 20px; }
input[type=submit] { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
input[type=submit]:hover { background: #0056b3; }
.footer { margin-top: 20px; font-size: 0.8em; color: #666; }
.nav { margin-bottom: 20px; }
.nav a { display: inline-block; margin: 0 10px; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 5px; }
.nav a:hover { background: #0056b3; }
</style>
</head>
<body>
    <div class="nav">
        <a href="/">OTA Update</a>
        <a href="/config">Configuration</a>
    </div>
    
    <h2>HVAC Sensor OTA Firmware Update</h2>
    <form method='POST' action='/upload' enctype='multipart/form-data'>
        <input type='file' name='update' accept='.bin'>
        <input type='submit' value='Upload Firmware'>
    </form>
    <div class='footer'>
        <p>Device: HVAC Sensor Display<br>Version: %FIRMWARE_VERSION%<br>IP: %IP_ADDRESS%</p>
    </div>
</body>
</html>
)rawliteral";


void WebServerManager::init(const char* firmware_version) {
    _firmware_version = firmware_version;

    server.on("/", HTTP_GET, handleRoot);
    server.on("/config", HTTP_GET, handleConfig);
    server.on("/config/update", HTTP_POST, handleConfigUpdate);
    
    server.on("/upload", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "Update complete! Rebooting...");
        delay(500);
        ESP.restart();
    }, handleUpload);

    server.onNotFound(handleNotFound);

    server.begin();
    logger.info("HTTP server for OTA started.");
}

void WebServerManager::handle() {
    server.handleClient();
}

void WebServerManager::handleRoot() {
    String ip = WiFi.localIP().toString();
    String html = String(update_page_html);
    html.replace("%IP_ADDRESS%", ip);
    html.replace("%FIRMWARE_VERSION%", _firmware_version);
    server.send(200, "text/html", html);
    logger.debug("Served OTA update web page.");
}

void WebServerManager::handleConfig() {
    String ip = WiFi.localIP().toString();
    String html = String(config_page_html);
    html.replace("%IP_ADDRESS%", ip);
    
    // Get thread-safe access to ConfigManager
    ConfigManagerAccessor config;
    
    // Gas thresholds
    html.replace("%NO2_WARN%", String(config->getNo2WarnThreshold()));
    html.replace("%NO2_DANGER%", String(config->getNo2DangerThreshold()));
    html.replace("%O3_WARN%", String(config->getO3WarnThreshold()));
    html.replace("%O3_DANGER%", String(config->getO3DangerThreshold()));
    html.replace("%NOX_WARN%", String(config->getNoxWarnThreshold()));
    html.replace("%NOX_DANGER%", String(config->getNoxDangerThreshold()));
    html.replace("%CO_WARN%", String(config->getCoWarnThreshold()));
    html.replace("%CO_DANGER%", String(config->getCoDangerThreshold()));
    
    // PM thresholds
    html.replace("%PM1_WARN%", String(config->getPm1WarnThreshold()));
    html.replace("%PM1_DANGER%", String(config->getPm1DangerThreshold()));
    html.replace("%PM25_WARN%", String(config->getPm25WarnThreshold()));
    html.replace("%PM25_DANGER%", String(config->getPm25DangerThreshold()));
    html.replace("%PM4_WARN%", String(config->getPm4WarnThreshold()));
    html.replace("%PM4_DANGER%", String(config->getPm4DangerThreshold()));
    html.replace("%PM10_WARN%", String(config->getPm10WarnThreshold()));
    html.replace("%PM10_DANGER%", String(config->getPm10DangerThreshold()));
    
    // Pressure thresholds
    html.replace("%PRESSURE_LOW%", String(config->getPressureLowThreshold()));
    html.replace("%PRESSURE_MID%", String(config->getPressureMidThreshold()));
    html.replace("%PRESSURE_HIGH%", String(config->getHighPressureThreshold()));
    
    // Fan current thresholds
    html.replace("%FAN_ON%", String(config->getFanOnCurrentThreshold()));
    html.replace("%FAN_OFF%", String(config->getFanOffCurrentThreshold()));
    html.replace("%FAN_HIGH%", String(config->getFanHighCurrentThreshold()));
    
    // Compressor current thresholds
    html.replace("%COMPRESSOR_ON%", String(config->getCompressorOnCurrentThreshold()));
    html.replace("%COMPRESSOR_OFF%", String(config->getCompressorOffCurrentThreshold()));
    html.replace("%COMPRESSOR_HIGH%", String(config->getCompressorHighCurrentThreshold()));
    
    // Pump current thresholds
    html.replace("%PUMP_ON%", String(config->getPumpOnCurrentThreshold()));
    html.replace("%PUMP_OFF%", String(config->getPumpOffCurrentThreshold()));
    html.replace("%PUMP_HIGH%", String(config->getPumpHighCurrentThreshold()));
    
    // Geiger counter thresholds
    html.replace("%GEIGER_ABNORMAL_LOW%", String(config->getGeigerAbnormalLowThreshold()));
    html.replace("%GEIGER_ABNORMAL_HIGH%", String(config->getGeigerAbnormalHighThreshold()));
    html.replace("%GEIGER_DANGER_HIGH%", String(config->getGeigerDangerHighThreshold()));
    
    // Temperature thresholds
    html.replace("%TEMP_COMFORTABLE_LOW%", String(config->getTempComfortableLow()));
    html.replace("%TEMP_COMFORTABLE_HIGH%", String(config->getTempComfortableHigh()));
    html.replace("%TEMP_ACCEPTABLE_LOW%", String(config->getTempAcceptableLow()));
    html.replace("%TEMP_ACCEPTABLE_HIGH%", String(config->getTempAcceptableHigh()));
    
    // Humidity thresholds
    html.replace("%HUMI_COMFORTABLE_LOW%", String(config->getHumiComfortableLow()));
    html.replace("%HUMI_COMFORTABLE_HIGH%", String(config->getHumiComfortableHigh()));
    html.replace("%HUMI_ACCEPTABLE_LOW%", String(config->getHumiAcceptableLow()));
    html.replace("%HUMI_ACCEPTABLE_HIGH%", String(config->getHumiAcceptableHigh()));
    
    // System configuration
    html.replace("%INACTIVITY_TIMER_DELAY%", String(config->getInactivityTimerDelay()));
    
    // MQTT configuration
    html.replace("%MQTT_HOST%", String(config->getMqttHost()));
    html.replace("%MQTT_PORT%", String(config->getMqttPort()));
    html.replace("%MQTT_USER%", String(config->getMqttUser()));
    html.replace("%MQTT_PASSWORD%", String(config->getMqttPassword()));
    html.replace("%MQTT_DEVICE_ID%", String(config->getMqttDeviceId()));
    html.replace("%MQTT_DEVICE_NAME%", String(config->getMqttDeviceName()));
    
    // Other thresholds
    html.replace("%CO2_WARN%", String(config->getCo2WarnThreshold()));
    html.replace("%CO2_DANGER%", String(config->getCo2DangerThreshold()));
    html.replace("%VOC_WARN%", String(config->getVocWarnThreshold()));
    html.replace("%VOC_DANGER%", String(config->getVocDangerThreshold()));
    
    server.send(200, "text/html", html);
    logger.info("Served configuration web page.");
}

void WebServerManager::handleConfigUpdate() {
    String section = server.arg("section");
    
    if (section == "mqtt_device" && server.hasArg("action") && server.arg("action") == "save") {
        // Validate MQTT settings before saving
        String host = server.arg("host");
        String port = server.arg("port");
        String user = server.arg("user");
        String password = server.arg("password");
        String deviceId = server.arg("device_id");
        String deviceName = server.arg("device_name");
        
        // Check for required fields
        if (host.length() == 0 || user.length() == 0 || password.length() == 0 || 
            deviceId.length() == 0 || deviceName.length() == 0) {
            server.send(400, "text/plain", "All MQTT fields are required");
            return;
        }
        
        // Validate port number
        int portNum = port.toInt();
        if (portNum <= 0 || portNum > 65535) {
            server.send(400, "text/plain", "Invalid port number");
            return;
        }
        
        // Get thread-safe access to ConfigManager
        ConfigManagerAccessor config;
        
        // Save all MQTT configuration at once
        config->setMqttHost(host.c_str());
        config->setMqttPort(portNum);
        config->setMqttUser(user.c_str());
        config->setMqttPassword(password.c_str());
        config->setMqttDeviceId(deviceId.c_str());
        config->setMqttDeviceName(deviceName.c_str());
        
        logger.info("MQTT device configuration saved. Rebooting device...");
        server.send(200, "text/plain", "Configuration saved successfully. Rebooting device...");
        
        // Delay to allow response to be sent before reboot
        delay(1000);
        ESP.restart();
        return;
    }
    
    // Regular parameter updates (require section, type, and value)
    if (server.hasArg("section") && server.hasArg("type") && server.hasArg("value")) {
        String type = server.arg("type");
        String valueStr = server.arg("value");
        
        int value = valueStr.toInt();
        
        // Get thread-safe access to ConfigManager
        ConfigManagerAccessor config;
        
        if (section == "no2") {
            if (type == "warn") {
                config->setNo2WarnThreshold(value);
                server.send(200, "text/plain", "NO2 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setNo2DangerThreshold(value);
                server.send(200, "text/plain", "NO2 Danger threshold updated successfully");
            }
        } else if (section == "o3") {
            if (type == "warn") {
                config->setO3WarnThreshold(value);
                server.send(200, "text/plain", "O3 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setO3DangerThreshold(value);
                server.send(200, "text/plain", "O3 Danger threshold updated successfully");
            }
        } else if (section == "nox") {
            if (type == "warn") {
                config->setNoxWarnThreshold(value);
                server.send(200, "text/plain", "NOx Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setNoxDangerThreshold(value);
                server.send(200, "text/plain", "NOx Danger threshold updated successfully");
            }
        } else if (section == "co") {
            if (type == "warn") {
                config->setCoWarnThreshold(value);
                server.send(200, "text/plain", "CO Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setCoDangerThreshold(value);
                server.send(200, "text/plain", "CO Danger threshold updated successfully");
            }
        } else if (section == "pm1") {
            if (type == "warn") {
                config->setPm1WarnThreshold(value);
                server.send(200, "text/plain", "PM1.0 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setPm1DangerThreshold(value);
                server.send(200, "text/plain", "PM1.0 Danger threshold updated successfully");
            }
        } else if (section == "pm25") {
            if (type == "warn") {
                config->setPm25WarnThreshold(value);
                server.send(200, "text/plain", "PM2.5 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setPm25DangerThreshold(value);
                server.send(200, "text/plain", "PM2.5 Danger threshold updated successfully");
            }
        } else if (section == "pm4") {
            if (type == "warn") {
                config->setPm4WarnThreshold(value);
                server.send(200, "text/plain", "PM4.0 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setPm4DangerThreshold(value);
                server.send(200, "text/plain", "PM4.0 Danger threshold updated successfully");
            }
        } else if (section == "pm10") {
            if (type == "warn") {
                config->setPm10WarnThreshold(value);
                server.send(200, "text/plain", "PM10 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setPm10DangerThreshold(value);
                server.send(200, "text/plain", "PM10 Danger threshold updated successfully");
            }
        } else if (section == "co2") {
            if (type == "warn") {
                config->setCo2WarnThreshold(value);
                server.send(200, "text/plain", "CO2 Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setCo2DangerThreshold(value);
                server.send(200, "text/plain", "CO2 Danger threshold updated successfully");
            }
        } else if (section == "pressure") {
            float floatValue = valueStr.toFloat();
            if (type == "low") {
                config->setPressureLowThreshold(floatValue);
                server.send(200, "text/plain", "Pressure Low threshold updated successfully");
            } else if (type == "mid") {
                config->setPressureMidThreshold(floatValue);
                server.send(200, "text/plain", "Pressure Mid threshold updated successfully");
            } else if (type == "high") {
                config->setHighPressureThreshold(floatValue);
                server.send(200, "text/plain", "High Pressure threshold updated successfully");
            }
        } else if (section == "fan") {
            float floatValue = valueStr.toFloat();
            if (type == "on") {
                config->setFanOnCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Fan On Current threshold updated successfully");
            } else if (type == "off") {
                config->setFanOffCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Fan Off Current threshold updated successfully");
            } else if (type == "high") {
                config->setFanHighCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Fan High Current threshold updated successfully");
            }
        } else if (section == "compressor") {
            float floatValue = valueStr.toFloat();
            if (type == "on") {
                config->setCompressorOnCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Compressor On Current threshold updated successfully");
            } else if (type == "off") {
                config->setCompressorOffCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Compressor Off Current threshold updated successfully");
            } else if (type == "high") {
                config->setCompressorHighCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Compressor High Current threshold updated successfully");
            }
        } else if (section == "pump") {
            float floatValue = valueStr.toFloat();
            if (type == "on") {
                config->setPumpOnCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Pump On Current threshold updated successfully");
            } else if (type == "off") {
                config->setPumpOffCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Pump Off Current threshold updated successfully");
            } else if (type == "high") {
                config->setPumpHighCurrentThreshold(floatValue);
                server.send(200, "text/plain", "Pump High Current threshold updated successfully");
            }
        } else if (section == "geiger") {
            float floatValue = valueStr.toFloat();
            if (type == "abnormal_low") {
                config->setGeigerAbnormalLowThreshold(floatValue);
                server.send(200, "text/plain", "Geiger Abnormal Low threshold updated successfully");
            } else if (type == "abnormal_high") {
                config->setGeigerAbnormalHighThreshold(floatValue);
                server.send(200, "text/plain", "Geiger Abnormal High threshold updated successfully");
            } else if (type == "danger_high") {
                config->setGeigerDangerHighThreshold(floatValue);
                server.send(200, "text/plain", "Geiger Danger High threshold updated successfully");
            }
        } else if (section == "temp") {
            float floatValue = valueStr.toFloat();
            if (type == "comfortable_low") {
                config->setTempComfortableLow(floatValue);
                server.send(200, "text/plain", "Temperature Comfortable Low threshold updated successfully");
            } else if (type == "comfortable_high") {
                config->setTempComfortableHigh(floatValue);
                server.send(200, "text/plain", "Temperature Comfortable High threshold updated successfully");
            } else if (type == "acceptable_low") {
                config->setTempAcceptableLow(floatValue);
                server.send(200, "text/plain", "Temperature Acceptable Low threshold updated successfully");
            } else if (type == "acceptable_high") {
                config->setTempAcceptableHigh(floatValue);
                server.send(200, "text/plain", "Temperature Acceptable High threshold updated successfully");
            }
        } else if (section == "humi") {
            float floatValue = valueStr.toFloat();
            if (type == "comfortable_low") {
                config->setHumiComfortableLow(floatValue);
                server.send(200, "text/plain", "Humidity Comfortable Low threshold updated successfully");
            } else if (type == "comfortable_high") {
                config->setHumiComfortableHigh(floatValue);
                server.send(200, "text/plain", "Humidity Comfortable High threshold updated successfully");
            } else if (type == "acceptable_low") {
                config->setHumiAcceptableLow(floatValue);
                server.send(200, "text/plain", "Humidity Acceptable Low threshold updated successfully");
            } else if (type == "acceptable_high") {
                config->setHumiAcceptableHigh(floatValue);
                server.send(200, "text/plain", "Humidity Acceptable High threshold updated successfully");
            }
        } else if (section == "inactivity") {
            int intValue = valueStr.toInt();
            if (type == "timer_delay") {
                config->setInactivityTimerDelay(intValue);
                server.send(200, "text/plain", "Inactivity timer delay updated successfully");
            }
        } else if (section == "mqtt") {
            if (type == "host") {
                config->setMqttHost(valueStr.c_str());
                server.send(200, "text/plain", "MQTT host updated successfully");
            } else if (type == "port") {
                int intValue = valueStr.toInt();
                config->setMqttPort(intValue);
                server.send(200, "text/plain", "MQTT port updated successfully");
            } else if (type == "user") {
                config->setMqttUser(valueStr.c_str());
                server.send(200, "text/plain", "MQTT username updated successfully");
            } else if (type == "password") {
                config->setMqttPassword(valueStr.c_str());
                server.send(200, "text/plain", "MQTT password updated successfully");
            } else if (type == "device_id") {
                config->setMqttDeviceId(valueStr.c_str());
                server.send(200, "text/plain", "MQTT device ID updated successfully");
            } else if (type == "device_name") {
                config->setMqttDeviceName(valueStr.c_str());
                server.send(200, "text/plain", "MQTT device name updated successfully");
            }
        } else if (section == "voc") {
            if (type == "warn") {
                config->setVocWarnThreshold(value);
                server.send(200, "text/plain", "VOC Warning threshold updated successfully");
            } else if (type == "danger") {
                config->setVocDangerThreshold(value);
                server.send(200, "text/plain", "VOC Danger threshold updated successfully");
            }
        } else {
            server.send(400, "text/plain", "Invalid section or type");
        }
        
        logger.infof("Threshold updated: %s_%s = %d", section.c_str(), type.c_str(), value);
    } else {
        server.send(400, "text/plain", "Missing required parameters");
    }
}

void log_update_error() {
    logger.errorf("Firmware update failed. Error code: %d", Update.getError());
}

void WebServerManager::handleUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        logger.infof("Web OTA Update Started: %s", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            log_update_error();
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            log_update_error();
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            logger.infof("Web OTA Update Success: %u bytes", upload.totalSize);
        } else {
            log_update_error();
        }
    }
}

void WebServerManager::handleNotFound(){
  server.send(404, "text/plain", "404: Not found");
  logger.warningf("HTTP 404 Not Found for request to: %s", server.uri().c_str());
}

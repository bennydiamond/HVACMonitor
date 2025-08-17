#pragma once

// PM sensors configuration page
const char* config_pm_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>PM Sensor Configuration</title>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial, sans-serif; background-color: #f4f4f4; padding: 20px; }
.container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
h1 { color: #333; text-align: center; margin-bottom: 30px; }
.nav { text-align: center; margin-bottom: 20px; }
.nav a { display: inline-block; margin: 0 10px; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 5px; }
.nav a:hover { background: #0056b3; }
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
</script>
</head>
<body>
    <div class="container">
        <h1>PM Sensor Configuration</h1>
        
        <div class="nav">
            <a href="/">OTA Update</a>
            <a href="/config">← Back to Config</a>
        </div>
        
        <div class="config-section">
            <h2>Particulate Matter Thresholds</h2>
            
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
    </div>
</body>
</html>
)rawliteral";

// HVAC system configuration page
const char* config_hvac_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>HVAC Monitor Configuration</title>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial, sans-serif; background-color: #f4f4f4; padding: 20px; }
.container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
h1 { color: #333; text-align: center; margin-bottom: 30px; }
.nav { text-align: center; margin-bottom: 20px; }
.nav a { display: inline-block; margin: 0 10px; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 5px; }
.nav a:hover { background: #0056b3; }
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
</script>
</head>
<body>
    <div class="container">
        <h1>HVAC Monitor Configuration</h1>
        
        <div class="nav">
            <a href="/">OTA Update</a>
            <a href="/config">← Back to Config</a>
        </div>
        
        <div class="config-section">
            <h2>Pressure Thresholds</h2>
            
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
    </div>
</body>
</html>
)rawliteral";
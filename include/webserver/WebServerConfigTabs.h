#pragma once

// Main config page with navigation
const char* config_main_html = R"rawliteral(
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
.nav { text-align: center; margin-bottom: 20px; }
.nav a { display: inline-block; margin: 0 10px; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 5px; }
.nav a:hover { background: #0056b3; }
.config-tabs { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; margin-top: 30px; }
.tab-card { background: #f8f9fa; padding: 20px; border-radius: 8px; border-left: 4px solid #007bff; }
.tab-card h3 { margin-top: 0; color: #007bff; }
.tab-card a { display: inline-block; margin-top: 10px; padding: 8px 16px; background: #007bff; color: white; text-decoration: none; border-radius: 4px; }
.tab-card a:hover { background: #0056b3; }
.memory-info { background: #d1ecf1; color: #0c5460; border: 1px solid #bee5eb; padding: 10px; border-radius: 4px; margin-bottom: 20px; font-size: 0.9em; }
</style>
</head>
<body>
    <div class="container">
        <h1>HVAC Sensor Configuration</h1>
        
        <div class="nav">
            <a href="/">OTA Update</a>
            <a href="/config">Configuration</a>
        </div>
        
        <div class="memory-info">
            <strong>System Status:</strong> Free Memory: %FREE_HEAP% bytes | IP: %IP_ADDRESS%
        </div>
        
        <div class="config-tabs">
            <div class="tab-card">
                <h3>Gas Sensors</h3>
                <p>Configure thresholds for NO2, O3, NOx, CO, CO2, and VOC sensors.</p>
                <a href="/config/gas">Configure Gas Sensors</a>
            </div>
            
            <div class="tab-card">
                <h3>Particulate Matter</h3>
                <p>Set warning and danger levels for PM1.0, PM2.5, PM4.0, and PM10 sensors.</p>
                <a href="/config/pm">Configure PM Sensors</a>
            </div>
            
            <div class="tab-card">
                <h3>HVAC Monitor</h3>
                <p>Configure pressure, fan current, compressor, pump, and geiger counter thresholds.</p>
                <a href="/config/hvac">Configure HVAC</a>
            </div>
            
            <div class="tab-card">
                <h3>Climate</h3>
                <p>Configure temperature and humidity comfort ranges for optimal indoor climate.</p>
                <a href="/config/climate">Configure Climate</a>
            </div>
            
            <div class="tab-card">
                <h3>System & MQTT</h3>
                <p>System settings and MQTT device configuration.</p>
                <a href="/config/system">Configure System</a>
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";

// Gas sensors configuration page
const char* config_gas_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Gas Sensor Configuration</title>
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
        <h1>Gas Sensor Configuration</h1>
        
        <div class="nav">
            <a href="/">OTA Update</a>
            <a href="/config">← Back to Config</a>
        </div>
        
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
</body>
</html>
)rawliteral";
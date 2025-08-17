#pragma once

// Climate configuration page
const char* config_climate_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Climate Configuration</title>
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
        <h1>Climate Configuration</h1>
        
        <div class="nav">
            <a href="/">OTA Update</a>
            <a href="/config">← Back to Config</a>
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
    </div>
</body>
</html>
)rawliteral";
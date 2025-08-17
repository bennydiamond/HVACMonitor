#pragma once

// System and MQTT configuration page
const char* config_system_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>System & MQTT Configuration</title>
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
.save-button { background: #28a745; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 20px; }
.save-button:hover { background: #218838; }
.save-button:disabled { background: #ccc; cursor: not-allowed; }
.status { margin-top: 10px; padding: 10px; border-radius: 4px; display: none; }
.status.success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
.status.error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
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
    .then(response => response.text())
    .then(result => {
        status.textContent = 'Configuration saved! Device will reboot in 3 seconds...';
        status.className = 'status success';
        status.style.display = 'block';
        rebootMessage.style.display = 'block';
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
        <h1>System & MQTT Configuration</h1>
        
        <div class="nav">
            <a href="/">OTA Update</a>
            <a href="/config">← Back to Config</a>
        </div>
        
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
</body>
</html>
)rawliteral";
#pragma once

// System, Wi-Fi and MQTT configuration page
const char *config_system_html =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>System & MQTT Configuration</title><link rel="stylesheet" href="/style.css?v=%FIRMWARE_VERSION%"><script src="/app.js?v=%FIRMWARE_VERSION%"></script></head><body><div class="container"><h1>System, Wi-Fi & MQTT Configuration</h1><div class="nav"><a href="/">OTA Update</a><a href="/config">← Back to Config</a></div>
<div class="config-section"><h2>System Configuration</h2>
<div class="config-row"><div class="config-label">Inactivity Timer Delay (seconds):</div><div class="config-input"><input type="number" id="inactivity_timer_delay_input" value="%INACTIVITY_TIMER_DELAY%" min="5" max="120" step="5"></div><button class="config-button" onclick="updateThreshold('inactivity','timer_delay')" id="inactivity_timer_delay_button">Set</button></div><div id="inactivity_timer_delay_status" class="status"></div>
</div>
<div class="config-section"><h2>Wi-Fi Configuration</h2><p style="color:#666;margin-bottom:15px">Updating Wi-Fi credentials will save them to NVS and reboot the device to connect to the new network.</p>
<div class="config-row"><div class="config-label">Wi-Fi SSID:</div><div class="config-input"><input type="text" id="wifi_ssid_input" value="%WIFI_SSID%" maxlength="32"></div></div>
<div class="config-row"><div class="config-label">Wi-Fi Password:</div><div class="config-input"><input type="password" id="wifi_password_input" value="%WIFI_PASSWORD%" maxlength="64"></div></div>
<button class="save-button" onclick="saveWifiConfig()" id="wifi-save-button">Save Wi-Fi Configuration</button>
<div id="wifi-save-status" class="status"></div>
<div id="wifi-reboot-message" class="reboot-message" style="display:none">Device will reboot in 3 seconds to connect to the new network...</div>
</div>
<div class="config-section"><h2>MQTT Device Configuration</h2><p style="color:#666;margin-bottom:15px">Changes to these settings will require a device reboot to take effect.</p>
<div class="config-row"><div class="config-label">MQTT Host:</div><div class="config-input"><input type="text" id="mqtt_host_input" value="%MQTT_HOST%" maxlength="63"></div></div>
<div class="config-row"><div class="config-label">MQTT Port:</div><div class="config-input"><input type="number" id="mqtt_port_input" value="%MQTT_PORT%" min="1" max="65535"></div></div>
<div class="config-row"><div class="config-label">MQTT Username:</div><div class="config-input"><input type="text" id="mqtt_user_input" value="%MQTT_USER%" maxlength="31"></div></div>
<div class="config-row"><div class="config-label">MQTT Password:</div><div class="config-input"><input type="password" id="mqtt_password_input" value="%MQTT_PASSWORD%" maxlength="63"></div></div>
<div class="config-row"><div class="config-label">MQTT Device ID:</div><div class="config-input"><input type="text" id="mqtt_device_id_input" value="%MQTT_DEVICE_ID%" maxlength="31"></div></div>
<div class="config-row"><div class="config-label">MQTT Device Name:</div><div class="config-input"><input type="text" id="mqtt_device_name_input" value="%MQTT_DEVICE_NAME%" maxlength="63"></div></div>
<button class="save-button" onclick="saveMqttConfig()" id="mqtt-save-button">Save Configuration</button>
<div id="mqtt-save-status" class="status"></div>
<div id="reboot-message" class="reboot-message" style="display:none">Device will reboot in 3 seconds...</div>
</div>
</div></body></html>)rawliteral";
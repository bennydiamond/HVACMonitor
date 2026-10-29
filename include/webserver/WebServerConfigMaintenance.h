#pragma once

// Sensor Maintenance and Calibration configuration page
const char *config_maintenance_html =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Sensor Maintenance</title><link rel="stylesheet" href="/style.css?v=%FIRMWARE_VERSION%"><script src="/app.js?v=%FIRMWARE_VERSION%"></script></head><body><div class="container"><div class="header"><div class="brand">HVAC & IAQ Monitor</div><div class="nav"><a href="/">Dashboard</a><a href="/config">← Settings Hub</a><a href="/ota">OTA Update</a></div></div>
<div class="config-section"><h2>SCD30 CO₂ Calibration</h2>
<p style="color:var(--text-muted);margin-bottom:15px">Configure SCD30 Automatic Self-Calibration (ASC) or trigger a Forced Recalibration (FRC) to a known reference (e.g. 400 ppm for outdoor fresh air).</p>
<div class="config-row"><div class="config-label">Auto-Calibration (ASC):</div><div class="config-input"><select id="scd30_autocal_input"><option value="1"%SCD30_AUTOCAL_ON_SEL%>Enabled</option><option value="0"%SCD30_AUTOCAL_OFF_SEL%>Disabled</option></select></div><button class="config-button" onclick="updateThreshold('scd30','autocal')" id="scd30_autocal_button">Set</button></div><div id="scd30_autocal_status" class="status"></div>
<div class="config-row"><div class="config-label">Forced Recalibration Target (ppm):</div><div class="config-input"><input type="number" id="scd30_forcecal_input" value="%SCD30_FORCECAL%" min="400" max="2000" step="1"></div><button class="config-button" onclick="updateThreshold('scd30','forcecal')" id="scd30_forcecal_button">Calibrate</button></div><div id="scd30_forcecal_status" class="status"></div>
</div>
<div class="config-section"><h2>SPS30 Particulate Sensor Maintenance</h2>
<p style="color:var(--text-muted);margin-bottom:15px">Trigger manual fan high-speed acceleration cycle to blow out accumulated dust, or query sensor hardware diagnostics.</p>
<div class="config-row"><div class="config-label">Manual Fan Clean:</div><div class="config-input" style="color:var(--text-muted);font-size:14px">Runs fan at max speed for 10 seconds</div><button class="config-button" onclick="triggerAction('sps30_clean','sps30_clean_btn','sps30_clean_status')" id="sps30_clean_btn">Start Clean</button></div><div id="sps30_clean_status" class="status"></div>
<div class="config-row"><div class="config-label">SPS30 Hardware Info:</div><div class="config-input" id="sps30_info_val" style="font-size:14px">%SPS30_INFO%</div><button class="config-button" onclick="triggerAction('sps30_info','sps30_info_btn','sps30_info_status','sps30_info_val')" id="sps30_info_btn">Query Info</button></div><div id="sps30_info_status" class="status"></div>
</div>
<div class="config-section"><h2>SGP41 Gas Sensor Diagnostics</h2>
<p style="color:var(--text-muted);margin-bottom:15px">Run sensor element conditioning and internal circuit self-test on the SGP41 sensor.</p>
<div class="config-row"><div class="config-label">SGP41 Self-Test:</div><div class="config-input" id="sgp41_test_val" style="font-size:14px">%SGP41_RESULT%</div><button class="config-button" onclick="triggerAction('sgp41_test','sgp41_test_btn','sgp41_test_status','sgp41_test_val')" id="sgp41_test_btn">Run Test</button></div><div id="sgp41_test_status" class="status"></div>
</div>
<div class="config-section"><h2>Sensor Hub Stack Management</h2>
<p style="color:var(--text-muted);margin-bottom:15px">Reboot the Arduino Nano sensor co-processor stack over the UART communication bridge.</p>
<button class="save-button" style="background:#dc3545" onclick="triggerAction('reboot_nano','reboot_nano_btn','reboot_nano_status')" id="reboot_nano_btn">Reboot Sensor Hub</button>
<div id="reboot_nano_status" class="status"></div>
</div>
</div></body></html>)rawliteral";

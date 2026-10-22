#pragma once

// Full-featured live dashboard landing page
const char *dashboard_page_html =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HVAC & IAQ Live Dashboard</title><link rel="stylesheet" href="/style.css?v=%FIRMWARE_VERSION%"><script src="/app.js?v=%FIRMWARE_VERSION%"></script></head><body onload="startDashboardPolling()"><div class="container">
<div class="header">
<div class="brand">HVAC & IAQ Monitor</div>
<div class="nav"><a href="/" class="active">Dashboard</a><a href="/config">Settings</a><a href="/ota">OTA Update</a></div>
<div class="live-pill"><span class="live-dot" id="live_dot"></span><span id="live_text" style="font-size:12px;font-weight:bold;color:var(--text-muted)">LIVE (2s)</span><button class="small-btn" id="poll_toggle_btn" onclick="togglePolling()">Pause</button></div>
</div>

<div class="summary-bar">
<div class="summary-item"><span class="summary-label">Air Quality:</span><span class="pill pill-warmup" id="sum_aqi">Warmup</span></div>
<div class="summary-item"><span class="summary-label">HVAC Blower:</span><span class="pill pill-info" id="sum_fan">Checking...</span></div>
<div class="summary-item"><span class="summary-label">Filter:</span><span class="pill pill-warmup" id="sum_filter">Warmup</span></div>
<div class="summary-item"><span class="summary-label">Sensor Hub:</span><span class="pill pill-good" id="sum_nano">Online</span></div>
</div>

<div class="section-card">
<div class="section-hdr" onclick="toggleSection('sec_iaq')">
<div class="section-title"><span>🌿 Indoor Air Quality & Climate</span><span class="sub-label">Multi-Gas, Particulates & Climate Telemetry</span></div>
<span class="chevron" id="chev_sec_iaq">▼</span>
</div>
<div class="section-body" id="sec_iaq">
<div class="grid">

<div class="metric-card">
<div class="card-hdr"><span>EPA Air Quality Index</span><span class="pill pill-warmup" id="badge_epa_aqi">Warmup</span></div>
<div class="card-val"><span id="val_epa_aqi">--</span><span class="unit">AQI</span></div>
<div class="card-sub">Fast AQI: <strong id="val_fast_aqi">--</strong></div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Carbon Dioxide (CO₂)</span><span class="pill pill-warmup" id="badge_co2">Warmup</span></div>
<div class="card-val"><span id="val_co2">--</span><span class="unit">ppm</span></div>
<div class="card-sub">Optical NDIR Sensor</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Sensirion VOC Index</span><span class="pill pill-warmup" id="badge_voc">Warmup</span></div>
<div class="card-val"><span id="val_voc">--</span><span class="unit">/ 500</span></div>
<div class="card-sub">Gas Index Algorithm</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Sensirion NOx Index</span><span class="pill pill-warmup" id="badge_nox">Warmup</span></div>
<div class="card-val"><span id="val_nox">--</span><span class="unit">/ 500</span></div>
<div class="card-sub">Gas Index Algorithm</div>
</div>

<div class="metric-card" style="grid-column:1/-1">
<div class="card-hdr"><span>Particulate Matter Concentration</span><span class="sub-label">Laser Optical Particle Counter</span></div>
<div class="pm-grid">
<div class="pm-item"><span class="pm-label">PM 1.0</span><span class="pm-val" id="val_pm1">--</span><span class="unit">µg/m³</span><span class="pill pill-warmup" id="badge_pm1">Warmup</span></div>
<div class="pm-item"><span class="pm-label">PM 2.5</span><span class="pm-val" id="val_pm25">--</span><span class="unit">µg/m³</span><span class="pill pill-warmup" id="badge_pm25">Warmup</span></div>
<div class="pm-item"><span class="pm-label">PM 4.0</span><span class="pm-val" id="val_pm4">--</span><span class="unit">µg/m³</span><span class="pill pill-warmup" id="badge_pm4">Warmup</span></div>
<div class="pm-item"><span class="pm-label">PM 10</span><span class="pm-val" id="val_pm10">--</span><span class="unit">µg/m³</span><span class="pill pill-warmup" id="badge_pm10">Warmup</span></div>
</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Ambient Climate</span></div>
<div class="climate-row">
<div><span class="climate-lbl">Temp:</span> <strong id="val_temp">--</strong> <span class="sub-badge pill-warmup" id="badge_temp">Warmup</span></div>
<div><span class="climate-lbl">Humidity:</span> <span><strong id="val_humi">--</strong> <span class="unit">% RH</span></span> <span class="sub-badge pill-warmup" id="badge_humi">Warmup</span></div>
<div><span class="climate-lbl">Baro:</span> <span><strong id="val_baro">--</strong> <span class="unit">hPa</span></span></div>
</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Trace & Toxic Gases</span><span class="sub-label">Electrochemical & MOx</span></div>
<div class="trace-grid">
<div><span class="trace-lbl">CO:</span> <span><strong id="val_co">--</strong> <span class="unit">ppm</span></span> <span class="sub-badge pill-warmup" id="badge_co">Warmup</span></div>
<div><span class="trace-lbl">O₃:</span> <span><strong id="val_o3">--</strong> <span class="unit">µg/m³</span></span> <span class="sub-badge pill-warmup" id="badge_o3">Warmup</span></div>
<div><span class="trace-lbl">NO₂:</span> <span><strong id="val_no2">--</strong> <span class="unit">µg/m³</span></span> <span class="sub-badge pill-warmup" id="badge_no2">Warmup</span></div>
</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Ionizing Radiation</span><span class="pill pill-warmup" id="badge_geiger">Warmup</span></div>
<div class="card-val"><span id="val_geiger_usv">--</span><span class="unit">µSv/h</span></div>
<div class="card-sub">Pulse Count: <strong id="val_geiger_cpm">--</strong> CPM</div>
</div>

</div>
</div>
</div>

<div class="section-card">
<div class="section-hdr" onclick="toggleSection('sec_hvac')">
<div class="section-title"><span>⚙️ HVAC & Equipment Operations</span><span class="sub-label">Pressure, CT Clamps, Drain Sensor</span></div>
<span class="chevron" id="chev_sec_hvac">▼</span>
</div>
<div class="section-body" id="sec_hvac">
<div class="grid">

<div class="metric-card">
<div class="card-hdr"><span>Filter Static Pressure</span><span class="pill pill-warmup" id="badge_filter_pres">Warmup</span></div>
<div class="card-val"><span id="val_filter_pres">--</span><span class="unit">Pa</span></div>
<div class="card-sub">Raw Duct Differential: <strong id="val_diff_pres">--</strong> Pa</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Blower Fan Current</span><span class="pill pill-info" id="badge_fan_status">OFF</span></div>
<div class="card-val"><span id="val_fan_amps">--</span><span class="unit">A</span></div>
<div class="card-sub">Fan CT Sensor on Co-processor</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Compressor Current</span><span class="pill pill-info" id="badge_compressor">OFF</span></div>
<div class="card-val"><span id="val_compressor_amps">--</span><span class="unit">A</span></div>
<div class="card-sub">Heat Pump Primary Stage</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Geothermal Loop Pump</span><span class="pill pill-info" id="badge_pump">OFF</span></div>
<div class="card-val"><span id="val_pump_amps">--</span><span class="unit">A</span></div>
<div class="card-sub">Ground Loop Circulation</div>
</div>

<div class="metric-card" style="grid-column:1/-1">
<div class="card-hdr"><span>Condensate Drain Pan Safety</span><span class="pill pill-good" id="badge_drain_pan">Clear / Normal</span></div>
<div class="drain-status" id="desc_drain_pan">Liquid level probe shows no condensate backup in evaporator coil drain pan.</div>
</div>

</div>
</div>
</div>

<div class="section-card">
<div class="section-hdr" onclick="toggleSection('sec_integ')">
<div class="section-title"><span>🔌 System Integration & Network</span><span class="sub-label">Integrator Diagnostic View</span></div>
<span class="chevron" id="chev_sec_integ">▼</span>
</div>
<div class="section-body" id="sec_integ">
<div class="grid">

<div class="metric-card">
<div class="card-hdr"><span>Wi-Fi Network</span><span class="pill pill-good" id="badge_wifi">Connected</span></div>
<div class="integ-list">
<div><span>SSID:</span> <strong id="val_wifi_ssid">--</strong></div>
<div><span>IP Address:</span> <strong id="val_wifi_ip">--</strong></div>
<div><span>Signal (RSSI):</span> <strong id="val_wifi_rssi">--</strong></div>
<div><span>MAC Address:</span> <strong id="val_wifi_mac">--</strong></div>
</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Home Assistant / MQTT</span><span class="pill pill-good" id="badge_mqtt">Connected</span></div>
<div class="integ-list">
<div><span>Broker:</span> <span><strong id="val_mqtt_host">--</strong>:<strong id="val_mqtt_port">--</strong></span></div>
<div><span>Integration:</span> <strong>ArduinoHA Native</strong></div>
<div><span>Auto-Discovery:</span> <strong>Enabled</strong></div>
</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Sensor Stack Co-processor</span><span class="pill pill-good" id="badge_nano_link">Online</span></div>
<div class="integ-list">
<div><span>Hardware:</span> <strong>Arduino Nano (ATmega328P)</strong></div>
<div><span>Firmware:</span> <strong id="val_nano_version">--</strong></div>
<div><span>Last Packet:</span> <span><strong id="val_last_packet">--</strong> s ago</span></div>
<div><span>Stack Uptime:</span> <strong id="val_nano_uptime">--</strong></div>
</div>
</div>

</div>
</div>
</div>

<div class="section-card">
<div class="section-hdr" onclick="toggleSection('sec_dev')">
<div class="section-title"><span>🛠️ Runtime & Developer Diagnostics</span><span class="sub-label">Memory, Uptime, Hardware Resets</span></div>
<span class="chevron" id="chev_sec_dev">▼</span>
</div>
<div class="section-body" id="sec_dev">
<div class="grid">

<div class="metric-card">
<div class="card-hdr"><span>ESP32 Memory Health</span><span class="sub-label">Internal SRAM</span></div>
<div class="integ-list">
<div><span>Free Heap:</span> <span><strong id="val_free_heap">--</strong> bytes</span></div>
<div><span>Min Free Heap:</span> <span><strong id="val_min_heap">--</strong> bytes</span></div>
<div><span>Max Alloc Block:</span> <span><strong id="val_max_heap">--</strong> bytes</span></div>
</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>Hardware Reset Diagnostics</span><span class="sub-label">Watchdog & Brownout</span></div>
<div class="integ-list">
<div><span>ESP32 Reset Reason:</span> <strong id="val_esp_reset">--</strong></div>
<div><span>Nano Reset Cause:</span> <strong id="val_nano_reset">--</strong></div>
<div><span>Nano Free RAM:</span> <span><strong id="val_nano_ram">--</strong> bytes</span></div>
</div>
</div>

<div class="metric-card">
<div class="card-hdr"><span>System Uptime & Firmware</span><span class="sub-label">Active Stack</span></div>
<div class="integ-list">
<div><span>ESP32 Controller Uptime:</span> <strong id="val_esp_uptime">--</strong></div>
<div><span>Firmware Version:</span> <strong id="val_firmware_ver">%FIRMWARE_VERSION%</strong></div>
<div><span>Architecture:</span> <strong>ESP32 Xtensa Dual-Core</strong></div>
</div>
</div>

</div>
</div>
</div>

</div></body></html>)rawliteral";

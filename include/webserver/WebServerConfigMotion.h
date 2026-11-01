#pragma once

// Motion Sensing (ESPectre) configuration and real-time diagnostics page
const char *config_motion_html =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Motion Sensing (ESPectre) - HVAC Monitor</title><link rel="stylesheet" href="/style.css?v=%FIRMWARE_VERSION%"><script src="/app.js?v=%FIRMWARE_VERSION%"></script></head><body><div class="container"><div class="header"><div class="brand">HVAC & IAQ Monitor</div><div class="nav"><a href="/">Dashboard</a><a href="/config">← Settings Hub</a><a href="/ota">OTA Update</a></div></div>

<div class="config-section"><h2>Real-Time Motion Status</h2>
<p style="color:var(--text-muted);margin-bottom:15px">Live Wi-Fi Channel State Information (CSI) motion sensing state and signal metrics.</p>
<div class="grid" style="margin-bottom:15px">
  <div class="metric-card">
    <div class="card-hdr"><span>MOTION DETECTION</span><span id="motion_state_badge" class="pill pill-warmup">--</span></div>
    <div class="card-val" id="motion_state_val">--</div>
    <div class="card-sub" id="motion_state_sub">Readiness: --</div>
  </div>
  <div class="metric-card">
    <div class="card-hdr"><span>MOTION INTENSITY</span><span id="motion_cal_badge" class="pill pill-info">Idle</span></div>
    <div class="card-val"><span id="motion_intensity_val">0.0</span><span class="unit">%</span></div>
    <div class="card-sub" id="motion_threshold_sub">Threshold: %THRESHOLD%</div>
  </div>
</div>
</div>

<div class="config-section"><h2>Motion Sensing Configuration</h2>
<p style="color:var(--text-muted);margin-bottom:15px">Configure ESPectre sensing engine parameters, traffic generation mode, and detection algorithms.</p>
<div class="config-row"><div class="config-label">Sensing Services:</div><div class="config-input"><select id="motion_enabled_input"><option value="1"%MOTION_ENABLED_ON_SEL%>Enabled</option><option value="0"%MOTION_ENABLED_OFF_SEL%>Disabled</option></select></div><button class="config-button" onclick="updateThreshold('motion','enabled')" id="motion_enabled_button">Set</button></div><div id="motion_enabled_status" class="status"></div>

<div class="config-row"><div class="config-label">Detection Profile:</div><div class="config-input"><select id="motion_profile_input"><option value="lightweight"%PROFILE_LW_SEL%>Lightweight</option><option value="high_accuracy"%PROFILE_HA_SEL%>High-Accuracy (ML)</option></select></div><button class="config-button" onclick="updateThreshold('motion','profile')" id="motion_profile_button">Set</button></div><div id="motion_profile_status" class="status"></div>

<div class="config-row"><div class="config-label">CSI Traffic Source:</div><div class="config-input"><select id="motion_traffic_mode_input"><option value="ping"%TRAFFIC_PING_SEL%>Ping (ICMP)</option><option value="dns"%TRAFFIC_DNS_SEL%>DNS (UDP)</option><option value="dns_tcp"%TRAFFIC_DNS_TCP_SEL%>DNS (TCP)</option><option value="wifi_raw"%TRAFFIC_RAW_SEL%>Raw Wi-Fi Frames</option><option value="external"%TRAFFIC_EXT_SEL%>External Host</option></select></div><button class="config-button" onclick="updateThreshold('motion','traffic_mode')" id="motion_traffic_mode_button">Set</button></div><div id="motion_traffic_mode_status" class="status"></div>

<div class="config-row"><div class="config-label">Detection Threshold (0.001 - 1.0):</div><div class="config-input"><input type="number" id="motion_threshold_input" value="%THRESHOLD%" min="0.001" max="1.0" step="0.001"></div><button class="config-button" onclick="updateThreshold('motion','threshold')" id="motion_threshold_button">Set</button></div><div id="motion_threshold_status" class="status"></div>

<div class="config-row"><div class="config-label">Motion On Hits (consecutive evaluations):</div><div class="config-input"><input type="number" id="motion_on_hits_input" value="%ON_HITS%" min="1" max="20" step="1"></div><button class="config-button" onclick="updateThreshold('motion','on_hits')" id="motion_on_hits_button">Set</button></div><div id="motion_on_hits_status" class="status"></div>

<div class="config-row"><div class="config-label">Motion Off Hits (consecutive evaluations):</div><div class="config-input"><input type="number" id="motion_off_hits_input" value="%OFF_HITS%" min="1" max="20" step="1"></div><button class="config-button" onclick="updateThreshold('motion','off_hits')" id="motion_off_hits_button">Set</button></div><div id="motion_off_hits_status" class="status"></div>

<div class="config-row"><div class="config-label">Recalibrate Detector:</div><div class="config-input" style="color:var(--text-muted);font-size:13px">Re-runs background noise baseline calibration</div><button class="config-button" onclick="triggerAction('motion_recalibrate','motion_recal_btn','motion_recal_status')" id="motion_recal_btn">Recalibrate</button></div><div id="motion_recal_status" class="status"></div>
</div>

<div class="config-section"><h2>CSI Diagnostic & Rate Statistics</h2>
<p style="color:var(--text-muted);margin-bottom:15px">Real-time packet rates and quality indicators reported by ESPectre CSI pipeline.</p>
<div class="integ-list" style="font-size:13px">
  <div><span>Calibration Active:</span><span id="diag_cal_active" style="font-weight:600">--</span></div>
  <div><span>CSI Callback Rate:</span><span id="diag_csi_cb" style="font-weight:600">-- pps</span></div>
  <div><span>CSI Accepted Rate:</span><span id="diag_csi_acc" style="font-weight:600">-- pps</span></div>
  <div><span>CSI Admitted Rate:</span><span id="diag_csi_adm" style="font-weight:600">-- pps</span></div>
  <div><span>CSI Filtered Rate:</span><span id="diag_csi_filt" style="font-weight:600">-- pps</span></div>
  <div><span>CSI Missing Slot Rate:</span><span id="diag_csi_miss" style="font-weight:600">-- pps</span></div>
  <div><span>CSI Excess Rate:</span><span id="diag_csi_excess" style="font-weight:600">-- pps</span></div>
  <div><span>CSI Stale Rate:</span><span id="diag_csi_stale" style="font-weight:600">-- pps</span></div>
  <div><span>CSI Out-of-Order Rate:</span><span id="diag_csi_ooo" style="font-weight:600">-- pps</span></div>
  <div><span>CSI Temporal Occupancy:</span><span id="diag_csi_occ" style="font-weight:600">-- %</span></div>
  <div><span>Generator Rate:</span><span id="diag_gen_rate" style="font-weight:600">-- pps</span></div>
  <div><span>Wi-Fi Link / RSSI:</span><span id="diag_wifi_link" style="font-weight:600">--</span></div>
</div>
</div>

</div>
<script>
window.addEventListener('DOMContentLoaded', () => {
  if (typeof startMotionPolling === 'function') startMotionPolling();
  if (typeof initRangeHints === 'function') initRangeHints();
});
</script>
</body></html>)rawliteral";


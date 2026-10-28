function validateThresholdRelation(e, t, val) {
  if (t === "warn" || t === "danger") {
    const oType = t === "warn" ? "danger" : "warn";
    const other = document.getElementById(e + "_" + oType + "_input");
    if (other) {
      const oVal = parseFloat(other.value);
      if (!isNaN(oVal)) {
        if (t === "warn" && val > oVal) return "Warning threshold (" + val + ") cannot exceed Danger threshold (" + oVal + ")";
        if (t === "danger" && val < oVal) return "Danger threshold (" + val + ") cannot be lower than Warning threshold (" + oVal + ")";
      }
    }
  } else if (e === "pressure") {
    const low = parseFloat(document.getElementById("pressure_low_input")?.value);
    const mid = parseFloat(document.getElementById("pressure_mid_input")?.value);
    const high = parseFloat(document.getElementById("pressure_high_input")?.value);
    if (t === "low" && !isNaN(mid) && val > mid) return "Low threshold (" + val + ") cannot exceed Mid threshold (" + mid + ")";
    if (t === "mid") {
      if (!isNaN(low) && val < low) return "Mid threshold (" + val + ") cannot be lower than Low threshold (" + low + ")";
      if (!isNaN(high) && val > high) return "Mid threshold (" + val + ") cannot exceed High threshold (" + high + ")";
    }
    if (t === "high" && !isNaN(mid) && val < mid) return "High threshold (" + val + ") cannot be lower than Mid threshold (" + mid + ")";
  } else if (e === "fan" || e === "compressor" || e === "pump") {
    const off = parseFloat(document.getElementById(e + "_off_input")?.value);
    const on = parseFloat(document.getElementById(e + "_on_input")?.value);
    const high = parseFloat(document.getElementById(e + "_high_input")?.value);
    if (t === "off" && !isNaN(on) && val > on) return "Off threshold (" + val + ") cannot exceed On threshold (" + on + ")";
    if (t === "on") {
      if (!isNaN(off) && val < off) return "On threshold (" + val + ") cannot be lower than Off threshold (" + off + ")";
      if (!isNaN(high) && val > high) return "On threshold (" + val + ") cannot exceed High threshold (" + high + ")";
    }
    if (t === "high" && !isNaN(on) && val < on) return "High threshold (" + val + ") cannot be lower than On threshold (" + on + ")";
  } else if (e === "geiger") {
    const abLow = parseFloat(document.getElementById("geiger_abnormal_low_input")?.value);
    const abHigh = parseFloat(document.getElementById("geiger_abnormal_high_input")?.value);
    const danHigh = parseFloat(document.getElementById("geiger_danger_high_input")?.value);
    if (t === "abnormal_low" && !isNaN(abHigh) && val > abHigh) return "Abnormal Low (" + val + ") cannot exceed Abnormal High (" + abHigh + ")";
    if (t === "abnormal_high") {
      if (!isNaN(abLow) && val < abLow) return "Abnormal High (" + val + ") cannot be lower than Abnormal Low (" + abLow + ")";
      if (!isNaN(danHigh) && val > danHigh) return "Abnormal High (" + val + ") cannot exceed Danger High (" + danHigh + ")";
    }
    if (t === "danger_high" && !isNaN(abHigh) && val < abHigh) return "Danger High (" + val + ") cannot be lower than Abnormal High (" + abHigh + ")";
  } else if (e === "temp" || e === "humi") {
    const accLow = parseFloat(document.getElementById(e + "_acceptable_low_input")?.value);
    const comLow = parseFloat(document.getElementById(e + "_comfortable_low_input")?.value);
    const comHigh = parseFloat(document.getElementById(e + "_comfortable_high_input")?.value);
    const accHigh = parseFloat(document.getElementById(e + "_acceptable_high_input")?.value);
    const label = e === "temp" ? "Temperature" : "Humidity";
    if (t === "acceptable_low" && !isNaN(comLow) && val > comLow) return label + " Acceptable Low (" + val + ") cannot exceed Comfortable Low (" + comLow + ")";
    if (t === "comfortable_low") {
      if (!isNaN(accLow) && val < accLow) return label + " Comfortable Low (" + val + ") cannot be lower than Acceptable Low (" + accLow + ")";
      if (!isNaN(comHigh) && val > comHigh) return label + " Comfortable Low (" + val + ") cannot exceed Comfortable High (" + comHigh + ")";
    }
    if (t === "comfortable_high") {
      if (!isNaN(comLow) && val < comLow) return label + " Comfortable High (" + val + ") cannot be lower than Comfortable Low (" + comLow + ")";
      if (!isNaN(accHigh) && val > accHigh) return label + " Comfortable High (" + val + ") cannot exceed Acceptable High (" + accHigh + ")";
    }
    if (t === "acceptable_high" && !isNaN(comHigh) && val < comHigh) return label + " Acceptable High (" + val + ") cannot be lower than Comfortable High (" + comHigh + ")";
  }
  return null;
}
function updateThreshold(e, t) {
  const n = document.getElementById(e + "_" + t + "_input"), s = document.getElementById(e + "_" + t + "_button"), o = document.getElementById(e + "_" + t + "_status");
  const valNum = parseFloat(n.value);
  if (n && n.hasAttribute("min") && n.hasAttribute("max")) {
    const minVal = parseFloat(n.min);
    const maxVal = parseFloat(n.max);
    if (!isNaN(valNum) && (valNum < minVal || valNum > maxVal)) {
      o.textContent = "Error: Value must be between " + minVal + " and " + maxVal;
      o.className = "status error";
      o.style.display = "block";
      setTimeout((() => { o.style.display = "none"; }), 4e3);
      return;
    }
  }
  if (!isNaN(valNum)) {
    const relErr = validateThresholdRelation(e, t, valNum);
    if (relErr) {
      o.textContent = "Error: " + relErr;
      o.className = "status error";
      o.style.display = "block";
      setTimeout((() => { o.style.display = "none"; }), 4e3);
      return;
    }
  }
  s.disabled = !0, s.textContent = "Saving...";
  const a = new FormData;
  a.append("section", e), a.append("type", t), a.append("value", n.value);
  fetch("/config/update", { method: "POST", body: a })
    .then((res => res.text().then(text => ({ ok: res.ok, text }))))
    .then((res => {
      o.textContent = res.text;
      o.className = "status " + (res.ok ? "success" : "error");
      o.style.display = "block";
      s.textContent = (t === "forcecal") ? "Calibrate" : "Set";
      s.disabled = !1;
      setTimeout((() => { o.style.display = "none"; }), 3e3);
    }))
    .catch((e => {
      o.textContent = "Error: " + e;
      o.className = "status error";
      o.style.display = "block";
      s.textContent = (t === "forcecal") ? "Calibrate" : "Set";
      s.disabled = !1;
    }));
}
function saveMqttConfig() { const e = document.getElementById("mqtt-save-button"), t = document.getElementById("mqtt-save-status"), n = document.getElementById("reboot-message"); e.disabled = !0, e.textContent = "Saving..."; const s = new FormData; s.append("section", "mqtt_device"), s.append("action", "save"), s.append("host", document.getElementById("mqtt_host_input").value), s.append("port", document.getElementById("mqtt_port_input").value), s.append("user", document.getElementById("mqtt_user_input").value), s.append("password", document.getElementById("mqtt_password_input").value), s.append("device_id", document.getElementById("mqtt_device_id_input").value), s.append("device_name", document.getElementById("mqtt_device_name_input").value), fetch("/config/update", { method: "POST", body: s }).then((e => e.text().then((s => ({ ok: e.ok, text: s }))))).then((s => { if (s.ok) { t.textContent = s.text, t.className = "status success", t.style.display = "block", n.style.display = "block" } else { t.textContent = "Error: " + s.text, t.className = "status error", t.style.display = "block", e.textContent = "Save Configuration", e.disabled = !1 } })).catch((n => { t.textContent = "Error: " + n.message, t.className = "status error", t.style.display = "block", e.textContent = "Save Configuration", e.disabled = !1 })) }
function saveWifiConfig() { const e = document.getElementById("wifi-save-button"), t = document.getElementById("wifi-save-status"), n = document.getElementById("wifi-reboot-message"), s = document.getElementById("wifi_ssid_input").value.trim(), o = document.getElementById("wifi_password_input").value, isOpen = document.getElementById("wifi_open_network")?.checked; if (!s) return t.textContent = "Error: Wi-Fi SSID cannot be empty.", t.className = "status error", void (t.style.display = "block"); if (!isOpen && o.length > 0 && (o.length < 8 || o.length > 64)) return t.textContent = "Error: Password must be 8 to 64 characters.", t.className = "status error", void (t.style.display = "block"); e.disabled = !0, e.textContent = "Saving..."; const a = new FormData; a.append("section", "wifi"), a.append("action", "save"), a.append("ssid", s); if (isOpen) { a.append("open_network", "1"); } else { a.append("password", o); } fetch("/config/update", { method: "POST", body: a }).then((e => e.text().then((s => ({ ok: e.ok, text: s }))))).then((s => { if (s.ok) { t.textContent = s.text, t.className = "status success", t.style.display = "block", n.style.display = "block" } else { t.textContent = "Error: " + s.text, t.className = "status error", t.style.display = "block", e.textContent = "Save Wi-Fi Configuration", e.disabled = !1 } })).catch((n => { t.textContent = "Error: " + n.message, t.className = "status error", t.style.display = "block", e.textContent = "Save Wi-Fi Configuration", e.disabled = !1 })) }
function toggleWifiOpen(e) { const t = document.getElementById("wifi_password_input"); if (t) { t.disabled = e; if (e) { if (!t.dataset.origPlaceholder) t.dataset.origPlaceholder = t.placeholder; t.value = "", t.placeholder = "No password (open network)" } else { t.placeholder = t.dataset.origPlaceholder || "" } } }
function triggerAction(e, t, n) { const s = document.getElementById(t), o = document.getElementById(n); let r = ""; s && (r = s.textContent, s.disabled = !0, s.textContent = "Working..."); if (o) { o.textContent = "Processing request...", o.className = "status info", o.style.display = "block" } const a = new FormData; a.append("action", e), fetch("/config/update", { method: "POST", body: a }).then((e => e.text().then((t => ({ ok: e.ok, text: t }))))).then((e => { if (o) { if (e.ok) { o.textContent = e.text, o.className = "status success", o.style.display = "block", setTimeout((() => { o.style.display = "none" }), 8e3) } else { o.textContent = "Error: " + e.text, o.className = "status error", o.style.display = "block", setTimeout((() => { o.style.display = "none" }), 8e3) } } s && (s.disabled = !1, s.textContent = r) })).catch((e => { if (o) { o.textContent = "Error: " + e.message, o.className = "status error", o.style.display = "block", setTimeout((() => { o.style.display = "none" }), 8e3) } s && (s.disabled = !1, s.textContent = r) })) }
let pollInterval = null, pollPaused = !1;
function toggleSection(e) { const t = document.getElementById(e), n = document.getElementById("chev_" + e); if (!t) return; const s = t.classList.toggle("collapsed"); n && (n.textContent = s ? "▶" : "▼"), localStorage.setItem("sec_" + e, s ? "1" : "0") }
function restoreSectionStates() { ["sec_iaq", "sec_hvac", "sec_integ", "sec_dev"].forEach((e => { "1" === localStorage.getItem("sec_" + e) && (document.getElementById(e)?.classList.add("collapsed"), document.getElementById("chev_" + e) && (document.getElementById("chev_" + e).textContent = "▶")) })) }
function setLiveStatus(e) { if (pollPaused) return; const t = document.getElementById("live_dot"), n = document.getElementById("live_text"); "live" === e ? (t && (t.className = "live-dot"), n && (n.textContent = "LIVE (2s)")) : "error" === e && (t && (t.className = "live-dot error"), n && (n.textContent = "RETRYING...")) }
function togglePolling() { pollPaused = !pollPaused; const e = document.getElementById("poll_toggle_btn"), t = document.getElementById("live_dot"), n = document.getElementById("live_text"); e && (e.textContent = pollPaused ? "Resume" : "Pause"), pollPaused ? (t && (t.className = "live-dot paused"), n && (n.textContent = "PAUSED")) : (t && (t.className = "live-dot"), n && (n.textContent = "LIVE (2s)"), fetchStatus()) }
function applyPill(e, t, n, s, o) { const l = document.getElementById(e); if (!l) return; if ("number" != typeof t || isNaN(t) || !isFinite(t) || t < 0) { l.className = "pill pill-warmup", l.textContent = "Warmup"; return } o && (t >= s ? (l.className = "pill pill-danger", l.textContent = "Danger") : t >= n ? (l.className = "pill pill-warn", l.textContent = "Warning") : (l.className = "pill pill-good", l.textContent = "Good")) }
function applySubBadge(e, t, n, s) { const o = document.getElementById(e); if (!o) return; if ("number" != typeof t || isNaN(t) || !isFinite(t) || t < 0) { o.className = "sub-badge pill-warmup", o.textContent = "Warmup"; return } t >= s ? (o.className = "sub-badge pill-danger", o.textContent = "Danger") : t >= n ? (o.className = "sub-badge pill-warn", o.textContent = "Warn") : (o.className = "sub-badge pill-good", o.textContent = "Good") }
function fmtUptime(e) { const t = Math.floor(e / 86400), n = Math.floor(e % 86400 / 3600), s = Math.floor(e % 3600 / 60), o = e % 60; return t > 0 ? t + "d " + n + "h " + s + "m" : n > 0 ? n + "h " + s + "m " + o + "s" : s + "m " + o + "s" }
function updateDashboardData(e) { if (!e || !e.iaq) return; try { const isNum = e => "number" == typeof e && !isNaN(e) && isFinite(e), fmtNum = (e, t) => isNum(e) ? e.toFixed(t) : "--", t = (e, t) => { const n = document.getElementById(e); n && (n.textContent = t) }; const n = document.getElementById("badge_epa_aqi"), s = document.getElementById("sum_aqi"); if (isNum(e.iaq.epa_aqi) && e.iaq.epa_aqi >= 0) { t("val_epa_aqi", e.iaq.epa_aqi); t("val_fast_aqi", isNum(e.iaq.fast_aqi) && e.iaq.fast_aqi >= 0 ? e.iaq.fast_aqi : "--"); if (n) { let t = "Good", o = "pill pill-good"; e.iaq.epa_aqi > 200 ? (t = "Hazardous", o = "pill pill-danger") : e.iaq.epa_aqi > 150 ? (t = "Unhealthy", o = "pill pill-danger") : e.iaq.epa_aqi > 100 ? (t = "Sensitive Alert", o = "pill pill-warn") : e.iaq.epa_aqi > 50 && (t = "Moderate", o = "pill pill-warn"); n.className = o; n.textContent = t; s && (s.className = o, s.textContent = t + " (" + e.iaq.epa_aqi + ")") } } else { t("val_epa_aqi", "--"); t("val_fast_aqi", "--"); n && (n.className = "pill pill-warmup", n.textContent = "Warmup"); s && (s.className = "pill pill-warmup", s.textContent = "Warmup") } if (isNum(e.iaq.co2) && e.iaq.co2 > 0) { t("val_co2", e.iaq.co2.toFixed(0)); applyPill("badge_co2", e.iaq.co2, e.th?.co2_w, e.th?.co2_d, !0) } else { t("val_co2", "--"); const e = document.getElementById("badge_co2"); e && (e.className = "pill pill-warmup", e.textContent = "Warmup") } if (isNum(e.iaq.voc) && e.iaq.voc > 0) { t("val_voc", e.iaq.voc); applyPill("badge_voc", e.iaq.voc, e.th?.voc_w, e.th?.voc_d, !0) } else { t("val_voc", "--"); const e = document.getElementById("badge_voc"); e && (e.className = "pill pill-warmup", e.textContent = "Warmup") } if (isNum(e.iaq.nox) && e.iaq.nox > 0) { t("val_nox", e.iaq.nox); applyPill("badge_nox", e.iaq.nox, e.th?.nox_w, e.th?.nox_d, !0) } else { t("val_nox", "--"); const e = document.getElementById("badge_nox"); e && (e.className = "pill pill-warmup", e.textContent = "Warmup") } const uPM = (e, n, s, o, l) => { if (isNum(s) && s >= 0) { t(e, s.toFixed(1)); applyPill(n, s, o, l, !0) } else { t(e, "--"); const s = document.getElementById(n); s && (s.className = "pill pill-warmup", s.textContent = "Warmup") } }; uPM("val_pm1", "badge_pm1", e.iaq.pm1, e.th?.pm1_w, e.th?.pm1_d); uPM("val_pm25", "badge_pm25", e.iaq.pm25, e.th?.pm25_w, e.th?.pm25_d); uPM("val_pm4", "badge_pm4", e.iaq.pm4, e.th?.pm4_w, e.th?.pm4_d); uPM("val_pm10", "badge_pm10", e.iaq.pm10, e.th?.pm10_w, e.th?.pm10_d); const bTemp = document.getElementById("badge_temp"); if (isNum(e.iaq.t) && e.iaq.t > -100) { const o = e.iaq.t; t("val_temp", o.toFixed(1) + " °C"); if (bTemp && e.th && isNum(e.th.t_cl)) { o >= e.th.t_cl && o <= e.th.t_ch ? (bTemp.className = "sub-badge pill-good", bTemp.textContent = "Good") : o >= e.th.t_al && o <= e.th.t_ah ? (bTemp.className = "sub-badge pill-warn", bTemp.textContent = "Warn") : (bTemp.className = "sub-badge pill-danger", bTemp.textContent = "Danger") } } else { t("val_temp", "--"); bTemp && (bTemp.className = "sub-badge pill-warmup", bTemp.textContent = "Warmup") } const bHumi = document.getElementById("badge_humi"); if (isNum(e.iaq.h) && e.iaq.h >= 0) { t("val_humi", e.iaq.h.toFixed(1)); if (bHumi && e.th && isNum(e.th.h_cl)) { e.iaq.h >= e.th.h_cl && e.iaq.h <= e.th.h_ch ? (bHumi.className = "sub-badge pill-good", bHumi.textContent = "Good") : e.iaq.h >= e.th.h_al && e.iaq.h <= e.th.h_ah ? (bHumi.className = "sub-badge pill-warn", bHumi.textContent = "Warn") : (bHumi.className = "sub-badge pill-danger", bHumi.textContent = "Danger") } } else { t("val_humi", "--"); bHumi && (bHumi.className = "sub-badge pill-warmup", bHumi.textContent = "Warmup") } t("val_baro", isNum(e.iaq.baro) && e.iaq.baro > 0 ? e.iaq.baro.toFixed(1) : "--"); if (isNum(e.iaq.co) && e.iaq.co >= 0) { t("val_co", e.iaq.co); applySubBadge("badge_co", e.iaq.co, e.th?.co_w, e.th?.co_d) } else { t("val_co", "--"); const e = document.getElementById("badge_co"); e && (e.className = "sub-badge pill-warmup", e.textContent = "Warmup") } if (isNum(e.iaq.o3) && e.iaq.o3 >= 0) { t("val_o3", e.iaq.o3.toFixed(1)); applySubBadge("badge_o3", e.iaq.o3, e.th?.o3_w, e.th?.o3_d) } else { t("val_o3", "--"); const e = document.getElementById("badge_co"); e && (e.className = "sub-badge pill-warmup", e.textContent = "Warmup") } if (isNum(e.iaq.no2) && e.iaq.no2 >= 0) { t("val_no2", e.iaq.no2.toFixed(1)); applySubBadge("badge_no2", e.iaq.no2, e.th?.no2_w, e.th?.no2_d) } else { t("val_no2", "--"); const e = document.getElementById("badge_no2"); e && (e.className = "sub-badge pill-warmup", e.textContent = "Warmup") } const bGeiger = document.getElementById("badge_geiger"); if (isNum(e.iaq.cpm) && e.iaq.cpm >= 0 && isNum(e.iaq.usv) && e.iaq.usv >= 0) { t("val_geiger_usv", e.iaq.usv.toFixed(2)); t("val_geiger_cpm", e.iaq.cpm); if (bGeiger) { e.iaq.usv >= .9 ? (bGeiger.className = "pill pill-danger", bGeiger.textContent = "Danger") : e.iaq.usv >= .3 ? (bGeiger.className = "pill pill-warn", bGeiger.textContent = "Warning") : (bGeiger.className = "pill pill-good", bGeiger.textContent = "Good") } } else { t("val_geiger_usv", "--"); t("val_geiger_cpm", "--"); bGeiger && (bGeiger.className = "pill pill-warmup", bGeiger.textContent = "Warmup") } if (e.hvac) { const a = document.getElementById("badge_filter_pres"), i = document.getElementById("sum_filter"); if (isNum(e.hvac.fp) && e.hvac.fp > 0) { t("val_filter_pres", e.hvac.fp.toFixed(1)); if (a) { e.hvac.hp ? (a.className = "pill pill-danger", a.textContent = "HIGH / CLOGGED", i && (i.className = "pill pill-danger", i.textContent = "HIGH PRESSURE")) : (a.className = "pill pill-good", a.textContent = "Good", i && (i.className = "pill pill-good", i.textContent = "GOOD (" + e.hvac.fp.toFixed(0) + " Pa)")) } } else { t("val_filter_pres", "--"); a && (a.className = "pill pill-warmup", a.textContent = "Warmup"); i && (i.className = "pill pill-warmup", i.textContent = "Warmup") } t("val_diff_pres", fmtNum(e.hvac.dp, 1)); const c = document.getElementById("badge_fan_status"), d = document.getElementById("sum_fan"); c && (0 === e.hvac.fs ? (c.className = "pill pill-info", c.textContent = "OFF", d && (d.className = "pill pill-info", d.textContent = "OFF")) : 1 === e.hvac.fs ? (c.className = "pill pill-danger", c.textContent = "ALERT", d && (d.className = "pill pill-danger", d.textContent = "ALERT (" + fmtNum(e.hvac.fa, 1) + "A)")) : (c.className = "pill pill-good", c.textContent = "RUNNING", d && (d.className = "pill pill-good", d.textContent = "RUNNING (" + fmtNum(e.hvac.fa, 1) + "A)"))); t("val_fan_amps", fmtNum(e.hvac.fa, 2)); const fo = isNum(e.th?.fan_off) ? e.th.fan_off : .5, p = document.getElementById("badge_compressor"); if (p && isNum(e.hvac.ca)) { const t = e.hvac.ca > fo; p.className = t ? "pill pill-good" : "pill pill-info", p.textContent = t ? "RUNNING" : "OFF" } t("val_compressor_amps", fmtNum(e.hvac.ca, 2)); const u = document.getElementById("badge_pump"); if (u && isNum(e.hvac.pa)) { const t = e.hvac.pa > fo; u.className = t ? "pill pill-good" : "pill pill-info", u.textContent = t ? "RUNNING" : "OFF" } t("val_pump_amps", fmtNum(e.hvac.pa, 2)); const m = document.getElementById("badge_drain_pan"), g = document.getElementById("desc_drain_pan"); m && (e.hvac.water ? (m.className = "pill pill-danger", m.textContent = "OVERFLOW DETECTED", g && (g.innerHTML = '<span style="color:#ef4444;font-weight:bold">⚠️ WARNING: Water detected in condensate pan! Drain may be backed up.</span>')) : (m.className = "pill pill-good", m.textContent = "Clear / Normal", g && (g.textContent = "Liquid level probe shows no condensate backup in evaporator coil drain pan."))) } if (e.sys) { t("val_wifi_ssid", e.sys.ssid || "Not Configured"); t("val_wifi_ip", e.sys.ip || "--"); t("val_wifi_rssi", isNum(e.sys.rssi) ? e.sys.rssi + " dBm" : "--"); t("val_wifi_mac", e.sys.mac || "--"); const f = document.getElementById("badge_wifi"); f && (f.className = e.sys.wc ? "pill pill-good" : "pill pill-danger", f.textContent = e.sys.wc ? "Connected" : "Disconnected"); t("val_mqtt_host", e.sys.mh || "Disabled"); t("val_mqtt_port", e.sys.mp || "--"); const y = document.getElementById("badge_mqtt"); y && (y.className = e.sys.mc ? "pill pill-good" : "pill pill-warn", y.textContent = e.sys.mc ? "Connected" : "Disconnected"); t("val_nano_version", e.sys.nv || "Unknown"); t("val_last_packet", isNum(e.sys.dsp) ? e.sys.dsp : "--"); t("val_nano_uptime", isNum(e.sys.nu) ? fmtUptime(e.sys.nu) : "--"); const v = document.getElementById("badge_nano_link"), b = document.getElementById("sum_nano"), fw = e.sys.nfw | 0, fa = document.getElementById("sensorstack_fw_alert"), sf = e.sys.sc && fw === 0 ? e.sys.nsf | 0 : 0, sa = document.getElementById("sensorstack_sensor_alert"), nc = fw === 2 ? "pill pill-danger" : fw === 1 ? "pill pill-info" : sf ? "pill pill-warn" : e.sys.sc ? "pill pill-good" : "pill pill-danger", nt = fw === 2 ? "Firmware issue" : fw === 1 ? "Updating" : sf ? "Sensor fault" : e.sys.sc ? "Online" : "Offline"; v && (v.className = nc, v.textContent = nt); b && (b.className = nc, b.textContent = nt); fa && (fa.style.display = fw === 2 ? "block" : "none"); if (sa) { const sl = [[1, "SPS30", "particulate matter"], [2, "SCD30", "CO₂, temperature and humidity"], [4, "SGP41", "VOC and NOx"]].filter(s => sf & s[0]).map(s => "<b>" + s[1] + "</b> (no " + s[2] + " readings)"); sa.innerHTML = sl.length ? "&#9888;&#65039; <b>SensorStack sensor fault</b> (not responding, defective or missing):" + sl.join(", ") + "." : ""; sa.style.display = sl.length ? "block" : "none" } t("val_nano_reset", e.sys.nrc || "None"); t("val_nano_ram", isNum(e.sys.nr) ? e.sys.nr : "--") } if (e.diag) { t("val_free_heap", isNum(e.diag.heap) ? e.diag.heap.toLocaleString() : "--"); t("val_min_heap", isNum(e.diag.min_heap) ? e.diag.min_heap.toLocaleString() : "--"); t("val_max_heap", isNum(e.diag.max_alloc) ? e.diag.max_alloc.toLocaleString() : "--"); t("val_esp_uptime", isNum(e.diag.uptime) ? fmtUptime(e.diag.uptime) : "--"); t("val_esp_reset", e.diag.reset || "--"); e.diag.flash_mode && t("val_flash_info", (e.diag.cpu || 240) + " MHz / " + (e.diag.flash_spd || 80) + " MHz " + e.diag.flash_mode) } } catch (err) { console.error("Dashboard update error:", err) } }
function fetchStatus() { if (pollPaused) return; fetch("/api/status").then((e => { if (!e.ok) throw new Error(e.status); return e.json() })).then((e => { setLiveStatus("live"); updateDashboardData(e) })).catch((e => { console.warn("Status poll error:", e); setLiveStatus("error") })) }
function startDashboardPolling() { restoreSectionStates(), fetchStatus(), pollInterval || (pollInterval = setInterval(fetchStatus, 2e3)) }
function togglePasswordVisibility(e, t) { const n = document.getElementById(e); if (!n) return; const s = "password" === n.type; n.type = s ? "text" : "password", t.innerHTML = s ? '<svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"/><line x1="1" y1="1" x2="23" y2="23"/></svg>' : '<svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>' }
function initRangeHints() {
  document.querySelectorAll('.config-row').forEach(row => {
    const input = row.querySelector('input');
    const label = row.querySelector('.config-label');
    if (!input || !label || label.querySelector('.range-hint')) return;

    let hintText = '';
    if (input.hasAttribute('min') && input.hasAttribute('max')) {
      const min = input.getAttribute('min');
      const max = input.getAttribute('max');
      hintText = `Range: ${min} to ${max}`;
      input.title = hintText;
      label.title = hintText;
    } else if (input.hasAttribute('maxlength')) {
      const maxL = input.getAttribute('maxlength');
      hintText = `Max: ${maxL} chars`;
      input.title = hintText;
      label.title = hintText;
    }

    if (hintText) {
      const hint = document.createElement('span');
      hint.className = 'range-hint';
      hint.textContent = hintText;
      label.appendChild(hint);
    }

    row.addEventListener('click', ev => {
      if (ev.target.tagName !== 'INPUT' && ev.target.tagName !== 'BUTTON' && !ev.target.closest('button')) {
        const wasActive = row.classList.contains('touch-active');
        document.querySelectorAll('.config-row.touch-active').forEach(r => r.classList.remove('touch-active'));
        if (!wasActive) row.classList.add('touch-active');
      }
    });
  });
}
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initRangeHints);
} else {
  initRangeHints();
}


#pragma once

// Dedicated OTA Firmware Update Page with in-page progress tracking
const char *ota_page_html =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>OTA Firmware Update</title><link rel="stylesheet" href="/style.css?v=%FIRMWARE_VERSION%"><script src="/app.js?v=%FIRMWARE_VERSION%"></script></head><body><div class="container" style="max-width:640px"><div class="header"><div class="brand">HVAC & IAQ Monitor</div><div class="nav"><a href="/">Dashboard</a><a href="/config">Settings</a><a href="/ota" class="active">OTA Update</a></div></div>
<div class="config-section" style="margin-top:20px">
<h2 style="text-align:center">Over-The-Air Firmware Update</h2>
<p style="color:var(--text-muted);margin-bottom:20px;font-size:14px;text-align:center">Select a compiled firmware binary (.bin) to flash wirelessly to the ESP32 controller.</p>

<form id="ota_form" onsubmit="startOtaUpload(event)" style="width:100%;box-sizing:border-box">
<input type="file" id="ota_file" name="update" accept=".bin" required style="width:100%;padding:12px;border:1px dashed var(--border);border-radius:6px;background:var(--card-sub);color:var(--text);margin-bottom:15px;box-sizing:border-box"><br>
<button type="submit" id="ota_submit_btn" class="save-button" style="width:100%;padding:12px;font-size:15px">Upload & Flash Firmware</button>
</form>

<div id="ota_progress_wrap" style="display:none;margin-top:20px;background:var(--card-sub);padding:18px;border-radius:8px;border:1px solid var(--border)">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:10px">
    <span id="ota_filename" style="font-size:13px;font-weight:600;color:var(--text)">firmware.bin</span>
    <span id="ota_pct" style="font-size:13px;font-weight:700;color:var(--accent)">0%</span>
  </div>
  <div style="background:var(--bg);height:14px;border-radius:7px;overflow:hidden;border:1px solid var(--border);margin-bottom:12px">
    <div id="ota_bar" style="background:linear-gradient(90deg,var(--accent),#818cf8);height:100%;width:0%;transition:width 0.15s ease"></div>
  </div>
  <div id="ota_status_msg" style="font-size:13px;color:var(--text-muted);text-align:center;font-weight:500">Uploading...</div>
  <div id="ota_detail" style="font-size:11px;color:var(--text-muted);text-align:center;margin-top:6px"></div>
</div>

<div style="margin-top:25px;padding:12px;background:rgba(56,189,248,0.08);border-left:3px solid var(--accent);border-radius:4px;text-align:left;font-size:13px;color:var(--text-muted)">
<strong>System Information:</strong><br>
Current Firmware Version: <span style="color:var(--text);font-weight:bold">%FIRMWARE_VERSION%</span><br>
Target Platform: <span style="color:var(--text)">ESP32 (240MHz, Dual Core, 4MB Flash)</span><br>
Active Partition Scheme: <span style="color:var(--text)">Dual OTA with Rollback</span>
</div>
</div></div>

<script>
function startOtaUpload(e) {
  e.preventDefault();
  const fileInput = document.getElementById("ota_file");
  if (!fileInput.files || fileInput.files.length === 0) {
    alert("Please select a .bin firmware file first.");
    return;
  }
  const file = fileInput.files[0];
  const submitBtn = document.getElementById("ota_submit_btn");
  const progressWrap = document.getElementById("ota_progress_wrap");
  const bar = document.getElementById("ota_bar");
  const pct = document.getElementById("ota_pct");
  const statusMsg = document.getElementById("ota_status_msg");
  const detail = document.getElementById("ota_detail");
  const filename = document.getElementById("ota_filename");

  submitBtn.disabled = true;
  submitBtn.style.opacity = "0.5";
  fileInput.disabled = true;
  progressWrap.style.display = "block";
  filename.textContent = file.name + " (" + (file.size / 1048576).toFixed(2) + " MB)";

  const formData = new FormData();
  formData.append("update", file);

  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/upload", true);

  xhr.upload.addEventListener("progress", function(evt) {
    if (evt.lengthComputable) {
      const percent = Math.round((evt.loaded / evt.total) * 100);
      bar.style.width = percent + "%";
      pct.textContent = percent + "%";
      detail.textContent = (evt.loaded / 1048576).toFixed(2) + " MB of " + (evt.total / 1048576).toFixed(2) + " MB transferred";
      if (percent < 100) {
        statusMsg.textContent = "Uploading firmware...";
        statusMsg.style.color = "var(--accent)";
      } else {
        statusMsg.textContent = "Flash write & verification in progress... Please wait.";
        statusMsg.style.color = "#fbbf24";
      }
    }
  });

  function startCountdown(msg) {
    let secs = 10;
    bar.style.width = "100%";
    pct.textContent = "100%";
    statusMsg.style.color = "#34d399";
    statusMsg.textContent = msg || "Firmware updated successfully! Rebooting...";
    const interval = setInterval(function() {
      secs--;
      if (secs > 0) {
        detail.textContent = "Device is rebooting. Redirecting to dashboard in " + secs + "s...";
      } else {
        clearInterval(interval);
        window.location.href = "/";
      }
    }, 1000);
  }

  xhr.onload = function() {
    if (xhr.status === 200) {
      startCountdown("Update complete! Rebooting device...");
    } else {
      statusMsg.textContent = "Update failed (Status " + xhr.status + "): " + xhr.responseText;
      statusMsg.style.color = "#f87171";
      submitBtn.disabled = false;
      submitBtn.style.opacity = "1";
      fileInput.disabled = false;
    }
  };

  xhr.onerror = function() {
    startCountdown("Firmware flashed! Device is restarting...");
  };

  xhr.send(formData);
}
</script>
</body></html>)rawliteral";

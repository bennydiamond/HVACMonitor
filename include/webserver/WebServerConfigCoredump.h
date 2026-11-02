#pragma once

// Core Dump Diagnostics and Flash Retrieval Page
const char *config_coredump_html =
    R"rawliteral(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Crash Dump Diagnostics - HVAC Monitor</title><link rel="stylesheet" href="/style.css?v=%FIRMWARE_VERSION%"><script src="/app.js?v=%FIRMWARE_VERSION%"></script><style>
.code-block{background:#0d1117;border:1px solid var(--border);border-radius:6px;padding:12px;font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;font-size:12px;color:#e6edf3;overflow-x:auto;white-space:pre-wrap;word-break:break-all;margin-top:8px}
.bt-tag{display:inline-block;padding:2px 6px;margin:2px 4px 2px 0;background:rgba(56,189,248,.1);border:1px solid rgba(56,189,248,.25);border-radius:4px;font-family:monospace;font-size:12px;color:var(--accent)}
.action-bar{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin-top:16px}
.btn-dl{background:var(--accent);color:#0f172a;padding:8px 16px;border-radius:5px;font-weight:600;font-size:13px;text-decoration:none;display:inline-flex;align-items:center;gap:6px;border:none;cursor:pointer}
.btn-dl:hover{background:var(--accent-hover)}
.btn-erase{background:#dc3545;color:#fff;padding:8px 16px;border-radius:5px;font-weight:600;font-size:13px;border:none;cursor:pointer}
.btn-erase:hover{background:#bd2130}
.btn-copy{background:var(--card-sub);color:var(--text);padding:4px 10px;border-radius:4px;font-size:11px;border:1px solid var(--border);cursor:pointer}
.btn-copy:hover{background:var(--border)}
</style></head><body><div class="container"><div class="header"><div class="brand">HVAC & IAQ Monitor</div><div class="nav"><a href="/">Dashboard</a><a href="/config">← Settings Hub</a><a href="/ota">OTA Update</a></div></div>

<div class="config-section">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px">
    <h2 style="margin:0;border:none;padding:0">Crash Dump Diagnostics</h2>
    <span id="dump_badge" class="pill pill-info">Scanning...</span>
  </div>
  <p style="color:var(--text-muted);margin-bottom:15px">Diagnostic information captured during fatal panics and saved silently to the dedicated on-board flash partition.</p>

  <div id="dump_state_box" style="margin-bottom:16px">
    <div style="text-align:center;padding:24px;color:var(--text-muted)">Loading crash dump information...</div>
  </div>

  <div id="dump_details" style="display:none">
    <div class="grid" style="margin-bottom:16px">
      <div class="metric-card">
        <div class="card-hdr"><span>CRASHING TASK / PC</span><span id="card_task_badge" class="pill pill-danger">PANIC</span></div>
        <div class="card-val" style="font-size:1.4rem" id="card_task_name">--</div>
        <div class="card-sub" id="card_pc_val">PC: --</div>
      </div>
      <div class="metric-card">
        <div class="card-hdr"><span>FAULT ADDRESS & CAUSE</span><span id="card_cause_pill" class="pill pill-warn">EXCEPTION</span></div>
        <div class="card-val" style="font-size:1.4rem" id="card_vaddr_val">--</div>
        <div class="card-sub" id="card_cause_val">Cause: --</div>
      </div>
      <div class="metric-card">
        <div class="card-hdr"><span>IMAGE STORAGE</span><span class="pill pill-info">FLASH</span></div>
        <div class="card-val" style="font-size:1.4rem" id="card_size_val">--</div>
        <div class="card-sub">Partition: coredump (64 KB)</div>
      </div>
    </div>

    <div class="config-row" style="flex-direction:column;align-items:flex-start">
      <div style="display:flex;justify-content:space-between;width:100%;align-items:center">
        <strong style="color:var(--accent);font-size:13px">Panic Reason:</strong>
      </div>
      <div id="dump_panic_reason" style="margin-top:6px;font-family:monospace;font-size:12px;color:#f87171;word-break:break-all">--</div>
    </div>

    <div class="config-row" style="flex-direction:column;align-items:flex-start">
      <div style="display:flex;justify-content:space-between;width:100%;align-items:center">
        <strong style="color:var(--accent);font-size:13px">Crashing Firmware Origin:</strong>
        <span id="dump_origin_pill" class="pill">--</span>
      </div>
      <div style="font-size:13px;margin-top:6px" id="dump_origin_desc">--</div>
      <div style="font-size:12px;margin-top:4px;color:var(--text-muted)">ELF SHA-256: <code id="dump_elf_sha" style="color:var(--accent)">--</code></div>
    </div>

    <div class="config-row" style="flex-direction:column;align-items:flex-start">
      <div style="display:flex;justify-content:space-between;width:100%;align-items:center">
        <strong style="color:var(--accent);font-size:13px">Call Stack / Backtrace:</strong>
        <button class="btn-copy" onclick="copyBacktrace()" id="btn_copy_bt">Copy Backtrace</button>
      </div>
      <div id="dump_backtrace_tags" style="margin-top:8px">--</div>
      <div class="code-block" id="dump_raw_bt" style="display:none"></div>
    </div>

    <div class="action-bar">
      <a href="/api/coredump/download" class="btn-dl" id="btn_download">⬇ Download Core Dump (.bin)</a>
      <button class="btn-erase" onclick="eraseCoredump()" id="btn_erase">🗑 Erase Crash Dump</button>
    </div>
    <div id="erase_status" class="status"></div>
  </div>
</div>

<div class="config-section">
  <h2>Device Firmware & Partition Identity</h2>
  <p style="color:var(--text-muted);margin-bottom:12px">Comparison hashes to identify which software build generated the crash dump.</p>
  <div class="integ-list" style="font-size:13px">
    <div><span>Current Running Partition:</span><span id="ref_running_part" style="font-weight:600">--</span></div>
    <div><span>Current Firmware Version:</span><span id="ref_running_ver" style="font-weight:600">--</span></div>
    <div><span>Current App ELF SHA-256:</span><code id="ref_running_sha" style="font-size:12px;color:var(--accent)">--</code></div>
    <div><span>Inactive OTA Partition:</span><span id="ref_ota_part" style="font-weight:600">--</span></div>
    <div><span>Inactive Bank Version:</span><span id="ref_ota_ver" style="font-weight:600">--</span></div>
    <div><span>Inactive Bank ELF SHA-256:</span><code id="ref_ota_sha" style="font-size:12px;color:var(--accent)">--</code></div>
  </div>
</div>

<div class="config-section">
  <h2>Offline Analysis Instructions</h2>
  <p style="color:var(--text-muted);margin-bottom:8px">Decode the downloaded <code>coredump.bin</code> using <code>firmware.elf</code>:</p>
  <div class="code-block"># Print panic summary & resolved backtrace symbols:
esp-coredump info_corefile -t raw -c coredump.bin --gdb xtensa-esp32-elf-gdb firmware.elf

# Open interactive GDB session on crashing core dump:
esp-coredump dbg_corefile -t raw -c coredump.bin --gdb xtensa-esp32-elf-gdb firmware.elf

# Or resolve a single PC address:
xtensa-esp32-elf-addr2line -pfia -e firmware.elf &lt;PC_ADDRESS&gt;</div>
</div>

</div>

<script>
let g_rawBtString = '';

async function fetchCoredumpInfo() {
  try {
    const res = await fetch('/api/coredump/info');
    if (!res.ok) throw new Error('Failed to load coredump info (' + res.status + ')');
    const data = await res.json();
    renderCoredump(data);
  } catch (err) {
    document.getElementById('dump_badge').className = 'pill pill-danger';
    document.getElementById('dump_badge').innerText = 'ERROR';
    document.getElementById('dump_state_box').innerHTML = '<div style="color:#f87171;padding:12px">Failed to load crash dump info: ' + err.message + '</div>';
  }
}

function renderCoredump(data) {
  // Device reference hashes
  document.getElementById('ref_running_part').innerText = data.running_part || 'unknown';
  document.getElementById('ref_running_ver').innerText = data.running_version || 'unknown';
  document.getElementById('ref_running_sha').innerText = data.running_sha256 || 'N/A';

  document.getElementById('ref_ota_part').innerText = data.ota_part || 'None';
  document.getElementById('ref_ota_ver').innerText = data.ota_version || 'N/A';
  document.getElementById('ref_ota_sha').innerText = data.ota_sha256 || 'N/A';

  const badge = document.getElementById('dump_badge');
  const stateBox = document.getElementById('dump_state_box');
  const details = document.getElementById('dump_details');

  if (!data.has_dump) {
    badge.className = 'pill pill-good';
    badge.innerText = 'NO CRASH DUMP';
    stateBox.innerHTML = '<div style="background:var(--card-sub);border:1px solid var(--border);border-radius:6px;padding:20px;text-align:center"><div style="color:#34d399;font-weight:600;font-size:16px;margin-bottom:6px">✓ Clean State</div><div style="color:var(--text-muted);font-size:13px">The dedicated flash coredump partition is clean. No system panics or fatal exceptions are recorded.</div></div>';
    details.style.display = 'none';
    return;
  }

  // Has dump
  badge.className = 'pill pill-danger';
  badge.innerText = 'CRASH DUMP RECORDED';
  stateBox.style.display = 'none';
  details.style.display = 'block';

  // Metrics
  document.getElementById('card_task_name').innerText = data.crashing_task || 'Unknown';
  document.getElementById('card_pc_val').innerText = 'PC: ' + (data.pc || 'N/A');
  document.getElementById('card_vaddr_val').innerText = data.excvaddr || 'N/A';
  document.getElementById('card_cause_val').innerText = 'Cause code: ' + (data.exccause !== undefined ? data.exccause : 'N/A');
  const szKb = (data.dump_size / 1024).toFixed(1);
  document.getElementById('card_size_val').innerText = szKb + ' KB';

  // Panic reason
  document.getElementById('dump_panic_reason').innerText = data.panic_reason || 'Unknown panic reason';

  // Origin & SHA-256 analysis
  const originPill = document.getElementById('dump_origin_pill');
  const originDesc = document.getElementById('dump_origin_desc');
  document.getElementById('dump_elf_sha').innerText = data.dump_sha256 || 'N/A';

  if (data.origin === 'current') {
    originPill.className = 'pill pill-good';
    originPill.innerText = 'Current Firmware';
    originDesc.innerHTML = 'This crash dump was generated by the <strong>currently running firmware build (' + (data.firmware_version || 'unknown') + ' on ' + (data.firmware_partition || 'app') + ')</strong>.';
  } else if (data.origin === 'other_bank') {
    originPill.className = 'pill pill-warn';
    originPill.innerText = 'Inactive OTA Bank';
    originDesc.innerHTML = 'This crash dump originated from the <strong>other OTA bank (' + (data.firmware_partition || 'app') + ', version ' + (data.firmware_version || 'unknown') + ')</strong>, not the currently active firmware.';
  } else {
    originPill.className = 'pill pill-danger';
    originPill.innerText = 'Orphaned / Prior Firmware';
    originDesc.innerHTML = 'This crash dump originated from a <strong>previous firmware build that is no longer installed on the device</strong> (hash does not match active partition nor OTA backup partition).';
  }

  // Backtrace
  const btTags = document.getElementById('dump_backtrace_tags');
  const rawBt = document.getElementById('dump_raw_bt');
  btTags.innerHTML = '';
  g_rawBtString = '';

  if (Array.isArray(data.backtrace) && data.backtrace.length > 0) {
    let btStrList = [];
    data.backtrace.forEach((addr, idx) => {
      const span = document.createElement('span');
      span.className = 'bt-tag';
      span.innerText = '#' + idx + ' ' + addr;
      btTags.appendChild(span);
      btStrList.push(addr);
    });
    g_rawBtString = btStrList.join(' ');
    rawBt.innerText = g_rawBtString;
    if (data.backtrace_corrupted) {
      const warn = document.createElement('span');
      warn.className = 'pill pill-warn';
      warn.style.marginLeft = '8px';
      warn.innerText = 'CORRUPTED';
      btTags.appendChild(warn);
    }
  } else {
    btTags.innerHTML = '<span style="color:var(--text-muted);font-size:12px">No backtrace available</span>';
  }
}

async function eraseCoredump() {
  if (!confirm('Are you sure you want to erase this crash dump from the flash partition? This action cannot be undone.')) {
    return;
  }
  const btn = document.getElementById('btn_erase');
  const status = document.getElementById('erase_status');
  btn.disabled = true;
  status.style.display = 'block';
  status.className = 'status info';
  status.innerText = 'Erasing flash coredump partition...';

  try {
    const res = await fetch('/api/coredump/erase', { method: 'POST' });
    const json = await res.json();
    if (res.ok && json.success) {
      status.className = 'status success';
      status.innerText = 'Crash dump erased successfully.';
      setTimeout(() => {
        status.style.display = 'none';
        btn.disabled = false;
        fetchCoredumpInfo();
      }, 1000);
    } else {
      throw new Error(json.error || 'Failed to erase partition');
    }
  } catch (err) {
    status.className = 'status error';
    status.innerText = 'Erase failed: ' + err.message;
    btn.disabled = false;
  }
}

function copyBacktrace() {
  if (!g_rawBtString) return;
  navigator.clipboard.writeText(g_rawBtString).then(() => {
    const btn = document.getElementById('btn_copy_bt');
    const oldText = btn.innerText;
    btn.innerText = 'Copied!';
    setTimeout(() => { btn.innerText = oldText; }, 2000);
  }).catch(() => {
    alert('Backtrace: ' + g_rawBtString);
  });
}

window.addEventListener('DOMContentLoaded', fetchCoredumpInfo);
</script>
</body></html>)rawliteral";


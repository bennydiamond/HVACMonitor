#pragma once

// Shared minified CSS stylesheet for all web server pages
const char style_css[] PROGMEM = R"rawliteral(body{font-family:Arial,sans-serif;background-color:#f4f4f4;margin:0;padding:20px}
.container{max-width:800px;margin:0 auto;background:#fff;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1)}
h1{color:#333;text-align:center;margin-top:0;margin-bottom:20px}
.nav{text-align:center;margin-bottom:20px}
.nav a{display:inline-block;margin:0 5px;padding:8px 16px;background:#007bff;color:#fff;text-decoration:none;border-radius:4px}
.nav a:hover{background:#0056b3}
.memory-info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb;padding:10px;border-radius:4px;margin-bottom:20px;font-size:0.9em}
.config-tabs{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:20px}
.tab-card{background:#f8f9fa;padding:20px;border-radius:8px;border-left:4px solid #007bff}
.tab-card h3{margin-top:0;color:#007bff}
.tab-card a{display:inline-block;margin-top:10px;padding:8px 16px;background:#007bff;color:#fff;text-decoration:none;border-radius:4px}
.tab-card a:hover{background:#0056b3}
.config-section{margin-bottom:25px}
.config-section h2{color:#007bff;border-bottom:2px solid #007bff;padding-bottom:5px;margin-top:0}
.config-row{display:flex;align-items:center;margin-bottom:10px;padding:10px;background:#f8f9fa;border-radius:5px}
.config-label{flex:1;font-weight:bold;color:#333}
.config-input{flex:1;margin:0 10px}
.config-input input{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;font-size:14px;box-sizing:border-box}
.config-button{background:#007bff;color:#fff;padding:8px 15px;border:none;border-radius:4px;cursor:pointer;font-size:14px}
.config-button:hover{background:#0056b3}
.config-button:disabled,.save-button:disabled{background:#ccc;cursor:not-allowed}
.save-button{background:#28a745;color:#fff;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;font-size:16px;margin-top:10px}
.save-button:hover{background:#218838}
.status{margin-top:8px;padding:8px;border-radius:4px;display:none}
.status.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}
.status.error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}
.reboot-message{background:#fff3cd;color:#856404;border:1px solid #ffeaa7;padding:12px;border-radius:4px;margin-top:15px;text-align:center;font-weight:bold}
.footer{margin-top:20px;font-size:0.85em;color:#666;text-align:center}
form{background:#fff;padding:20px;border-radius:8px;display:inline-block}
input[type=file]{margin-bottom:15px}
input[type=submit]{background:#007bff;color:#fff;padding:10px 20px;border:none;border-radius:4px;cursor:pointer}
input[type=submit]:hover{background:#0056b3}
@media(max-width:600px){.config-row{flex-direction:column;align-items:stretch}.config-input{margin:8px 0}.config-button{width:100%}})rawliteral";

// Shared minified JavaScript application logic for configuration pages
const char app_js[] PROGMEM = R"rawliteral(function updateThreshold(e,t){const n=document.getElementById(e+"_"+t+"_input"),s=document.getElementById(e+"_"+t+"_button"),o=document.getElementById(e+"_"+t+"_status");s.disabled=!0,s.textContent="Saving...";const a=new FormData;a.append("section",e),a.append("type",t),a.append("value",n.value),fetch("/config/update",{method:"POST",body:a}).then((e=>e.text())).then((e=>{o.textContent=e,o.className="status success",o.style.display="block",s.textContent="Set",s.disabled=!1,setTimeout((()=>{o.style.display="none"}),3e3)})).catch((e=>{o.textContent="Error: "+e,o.className="status error",o.style.display="block",s.textContent="Set",s.disabled=!1}))}
function saveMqttConfig(){const e=document.getElementById("mqtt-save-button"),t=document.getElementById("mqtt-save-status"),n=document.getElementById("reboot-message");e.disabled=!0,e.textContent="Saving...";const s=new FormData;s.append("section","mqtt_device"),s.append("action","save"),s.append("host",document.getElementById("mqtt_host_input").value),s.append("port",document.getElementById("mqtt_port_input").value),s.append("user",document.getElementById("mqtt_user_input").value),s.append("password",document.getElementById("mqtt_password_input").value),s.append("device_id",document.getElementById("mqtt_device_id_input").value),s.append("device_name",document.getElementById("mqtt_device_name_input").value),fetch("/config/update",{method:"POST",body:s}).then((e=>e.text())).then((s=>{t.textContent=s,t.className="status success",t.style.display="block",n.style.display="block"})).catch((n=>{t.textContent="Error: "+n.message,t.className="status error",t.style.display="block",e.textContent="Save Configuration",e.disabled=!1}))}
function saveWifiConfig(){const e=document.getElementById("wifi-save-button"),t=document.getElementById("wifi-save-status"),n=document.getElementById("wifi-reboot-message"),s=document.getElementById("wifi_ssid_input").value.trim(),o=document.getElementById("wifi_password_input").value;if(!s)return t.textContent="Error: Wi-Fi SSID cannot be empty.",t.className="status error",void(t.style.display="block");e.disabled=!0,e.textContent="Saving...";const a=new FormData;a.append("section","wifi"),a.append("action","save"),a.append("ssid",s),a.append("password",o),fetch("/config/update",{method:"POST",body:a}).then((e=>e.text())).then((s=>{t.textContent=s,t.className="status success",t.style.display="block",n.style.display="block"})).catch((n=>{t.textContent="Error: "+n.message,t.className="status error",t.style.display="block",e.textContent="Save Wi-Fi Configuration",e.disabled=!1}))})rawliteral";

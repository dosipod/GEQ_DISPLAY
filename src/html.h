#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>TTGO Visualizer Dashboard</title>
  <style>
    body { font-family: system-ui, sans-serif; background: #121212; color: #e0e0e0; margin: 0; padding: 20px; display: flex; flex-direction: row; gap: 20px; align-items: flex-start; justify-content: flex-start; }
    .card { background: #1e1e1e; padding: 20px; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.5); width: 100%; max-width: 360px; box-sizing: border-box; }
    h2 { margin-top: 0; color: #00e676; text-align: left; }
    label { display: block; margin: 12px 0 4px; font-weight: bold; font-size: 14px; }
    input, select, button { width: 100%; padding: 10px; border-radius: 6px; border: 1px solid #333; background: #252525; color: #fff; box-sizing: border-box; }
    input[type="range"] { padding: 0; height: 10px; cursor: pointer; background: #333; }
    .slider-val { float: right; color: #00e676; font-weight: bold; }
    button { background: #00e676; color: #121212; font-weight: bold; cursor: pointer; margin-top: 15px; border: none; text-align: center; }
    button:hover { background: #00b55c; }
    .btn-warn { background: #ff9100; color: #121212; }
    .btn-warn:hover { background: #cc7400; }
    .btn-danger { background: #ff5252; color: #fff; }
    .btn-danger:hover { background: #e04444; }
    .btn-info { background: #0288d1; color: #fff; margin-top: 10px; }
    .btn-info:hover { background: #01579b; }
    #canvas { width: 100%; height: 180px; background: #000; border-radius: 6px; display: block; }
    .ota-link { display: block; text-align: left; color: #a0a0a0; text-decoration: none; margin-top: 15px; font-size: 12px; }
    .ota-link:hover { color: #00e676; }
    .row { display: flex; gap: 10px; }
    .version-tag { text-align: left; color: #777; font-size: 12px; margin-top: 10px; font-weight: bold; }
    .diag-box { background: #252525; padding: 10px; border-radius: 6px; border: 1px solid #333; margin-top: 15px; font-size: 13px; font-family: monospace; color: #ffeb3b; }
    .toggle-row { display: flex; justify-content: space-between; align-items: center; background: #252525; padding: 10px; border-radius: 6px; border: 1px solid #333; margin: 12px 0; }
    .toggle-row label { margin: 0; cursor: pointer; }
    .toggle-row input { width: auto; cursor: pointer; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Live Preview</h2>
    <canvas id="canvas" width="240" height="135"></canvas>
    <div class="diag-box" id="liveDiag">STREAM HARDWARE STATISTICS -> PKT: 0</div>
    <button type="button" class="btn-info" onclick="window.location.reload(true)">Refresh Interface View</button>
    <div class="version-tag" id="webVer">Loading Build Manifest...</div>
    <a href="/update" class="ota-link" target="_blank">Firmware Update Dashboard (OTA)</a>
  </div>
  <div class="card">
    <h2>Control Console</h2>
    <form id="cfgForm">
      <div class="toggle-row">
        <label for="isDisplayOn">Master Display Power Switch</label>
        <input type="checkbox" id="isDisplayOn" name="isDisplayOn" checked>
      </div>
      <label>UDP Target Port</label>
      <input type="number" id="port" name="udpPort" min="1" max="65535">
      <label>Multicast Target Core IP</label>
      <input type="text" id="ip" name="multicastIP">
      <label>Audio Squelch Cutoff <span class="slider-val" id="floorVal">25</span></label>
      <input type="range" id="audioFloor" min="0" max="255" value="25" oninput="document.getElementById('floorVal').innerText=this.value">
      <label>Visual Gain Multiplier <span class="slider-val" id="gainVal">1.5</span></label>
      <input type="range" id="audioGain" min="5" max="35" value="15" step="1" oninput="document.getElementById('gainVal').innerText=(this.value/10).toFixed(1)">
      <label>Peak Dot Gravity Speed <span class="slider-val" id="gravVal">3</span></label>
      <input type="range" id="peakGravity" min="1" max="12" value="3" oninput="document.getElementById('gravVal').innerText=this.value">
      <label>Spectrum Render Effect Style</label>
      <select id="effect" name="visualizerMode">
        <option value="0">0. Classical GEQ (Green)</option>
        <option value="1">1. Center-Out Mirror (Cyan)</option>
        <option value="2">2. Rainbow Flow Spectrum</option>
        <option value="3">3. Top-Down Fire Equalizer</option>
        <option value="4">4. Matrix Pulse Waves</option>
        <option value="5">5. Double Mirror Peak (Magenta/Cyan)</option>
        <option value="6">6. Volume Intensity Flash (VU Green-Yellow-Red)</option>
        <option value="7">7. Neon Grid Wave (Blue/White)</option>
        <option value="8">8. Fluid Ocean Wave (Sine Blue)</option>
        <option value="9">9. Infinite Center Pulse (Expanding Box)</option>
        <option value="10">10. Fire & Ice (Split Thermal Spectrum)</option>
        <option value="11">11. Side-to-Center Crush Mirror</option>
        <option value="12">12. Particle Dust (Strobe Peaks Only)</option>
        <option value="13">13. Cyberpunk Matrix Wave</option>
        <option value="14">14. Bass-Driven Strobe Stutter</option>
        <option value="15">15. Peak Decay Snake</option>
      </select>
      <label>Hardware Display Rotation</label>
      <select id="rotation" name="displayRotation">
        <option value="0">0&deg; (Vertical Standard)</option>
        <option value="1">90&deg; (Landscape Right)</option>
        <option value="2">180&deg; (Vertical Flipped)</option>
        <option value="3">270&deg; (Landscape Left)</option>
      </select>
      <button type="button" onclick="submitConfig()">Save Options</button>
    </form>
    <div class="row">
      <button type="button" class="btn-warn" onclick="triggerTest()">Test Display Screen</button>
      <button type="button" class="btn-danger" onclick="triggerReboot()">Reboot Hardware</button>
    </div>
  </div>
  <script>
    var ws;
    var ctx = document.getElementById('canvas').getContext('2d');
    function connectWS() {
      ws = new WebSocket('ws://' + window.location.hostname + ':81/');
      ws.onmessage = function(evt) {
        var data = JSON.parse(evt.data);
        ctx.clearRect(0, 0, 240, 135);
        if (data.isTest) {
          ctx.fillStyle = data.testColor; ctx.fillRect(0, 0, 240, 135); return;
        }
        document.getElementById('liveDiag').innerText = "STREAM HARDWARE STATISTICS -> PKT: " + (data.pktCount || 0);
        var w = (240 / 16) - 2;
        var hasActiveBars = false;
        var avgVol = 0;
        for(var i=0; i<16; i++) avgVol += data.bins[i];
        avgVol = avgVol / 16;
        for(var i=0; i<16; i++) {
          var rawVal = data.bins[i];
          var h = (rawVal / 255) * 135;
          if (rawVal > 0) hasActiveBars = true;
          if (data.mode == 2) ctx.fillStyle = 'hsl(' + (i * 22) + ', 100%, 50%)';
          else if (data.mode == 3) ctx.fillStyle = 'rgb(255, ' + (255 - rawVal) + ', 0)';
          else if (data.mode == 4) ctx.fillStyle = 'rgb(' + rawVal + ', 0, 255)';
          else if (data.mode == 1) ctx.fillStyle = '#00e5ff';
          else if (data.mode == 5) ctx.fillStyle = (i < 8) ? '#ff00ff' : '#00e5ff';
          else if (data.mode == 6) {
            if (rawVal < 100) ctx.fillStyle = '#00e676';
            else if (rawVal < 200) ctx.fillStyle = '#ffeb3b';
            else ctx.fillStyle = '#ff5252';
          }
          else if (data.mode == 7) ctx.fillStyle = 'rgb(0, ' + rawVal + ', 255)';
          else if (data.mode == 8) ctx.fillStyle = 'rgb(0, ' + (100 + Math.floor(rawVal*0.6)) + ', 255)';
          else if (data.mode == 10) ctx.fillStyle = (i < 8) ? '#ff5252' : '#0288d1';
          else if (data.mode == 11) ctx.fillStyle = '#ff9100';
          else if (data.mode == 12 || data.mode == 15) ctx.fillStyle = '#ffffff';
          else if (data.mode == 13) ctx.fillStyle = '#ff00ff';
          else if (data.mode == 14) ctx.fillStyle = (avgVol > 120) ? '#ffffff' : '#00e676';
          else ctx.fillStyle = '#00e676';
          if (data.mode == 1) {
            ctx.fillRect(i * (w + 2), (135/2) - (h/2), w, h);
          } else if (data.mode == 3) {
            ctx.fillRect(i * (w + 2), 0, w, h);
          } else if (data.mode == 5) {
            var qH = h / 2;
            ctx.fillRect(i * (w + 2), 0, w, qH);
            ctx.fillRect(i * (w + 2), 135 - qH, w, qH);
          } else if (data.mode == 7) {
            if (h > 3) {
              ctx.fillStyle = '#ffffff'; ctx.fillRect(i * (w + 2), 135 - h, w, 3);
              ctx.fillStyle = 'rgb(0, ' + rawVal + ', 255)'; ctx.fillRect(i * (w + 2), 135 - h + 3, w, h - 3);
            }
          } else if (data.mode == 9) {
            if (i == 0) {
              var pW = (avgVol / 255) * 100; var pH = (avgVol / 255) * 80;
              ctx.strokeStyle = '#00e676'; ctx.lineWidth = 4;
              ctx.strokeRect((240/2)-(pW/2), (135/2)-(pH/2), pW, pH);
            }
          } else {
            ctx.fillRect(i * (w + 2), 135 - h, w, h);
          }
        }
        if (!hasActiveBars) { ctx.fillStyle = '#000000'; ctx.fillRect(0, 0, 240, 135); }
      };
      ws.onclose = function() { setTimeout(connectWS, 2000); };
    }
    fetch('/get-config').then(r => r.json()).then(data => {
      document.getElementById('port').value = data.udpPort;
      document.getElementById('ip').value = data.multicastIP;
      document.getElementById('audioFloor').value = data.audioFloor;
      document.getElementById('audioGain').value = data.audioGain;
      document.getElementById('peakGravity').value = data.peakGravity || 3;
      document.getElementById('isDisplayOn').checked = (data.isDisplayOn !== false);
      document.getElementById('floorVal').innerText = data.audioFloor;
      document.getElementById('gainVal').innerText = (data.audioGain/10).toFixed(1);
      document.getElementById('gravVal').innerText = data.peakGravity || 3;
      document.getElementById('effect').value = data.visualizerMode;
      document.getElementById('rotation').value = data.displayRotation;
      document.getElementById('webVer').innerText = "FIRMWARE RUNNING: " + data.version;
      connectWS();
    });
    function submitConfig() {
      var payload = {
        udpPort: parseInt(document.getElementById('port').value),
        multicastIP: document.getElementById('ip').value,
        audioFloor: parseInt(document.getElementById('audioFloor').value),
        audioGain: parseInt(document.getElementById('audioGain').value),
        peakGravity: parseInt(document.getElementById('peakGravity').value),
        isDisplayOn: document.getElementById('isDisplayOn').checked,
        visualizerMode: parseInt(document.getElementById('effect').value),
        displayRotation: parseInt(document.getElementById('rotation').value)
      };
      fetch('/save-config', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
    }
    function triggerTest() { fetch('/test-display', { method: 'POST' }); }
    function triggerReboot() { fetch('/reboot'); }
  </script>
</body>
</html>
)rawliteral";

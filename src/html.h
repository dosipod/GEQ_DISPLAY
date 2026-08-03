#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Audio Visualizer Control</title>
  <style>
    body { font-family: Arial, sans-serif; background: #121212; color: #fff; margin: 0; padding: 20px; }
    h2 { color: #00e5ff; margin-top: 0; }
    .container { display: flex; flex-direction: row; gap: 20px; max-width: 1050px; margin: 0 auto; flex-wrap: wrap; }
    .left-panel { flex: 1.2; min-width: 360px; }
    .right-panel { flex: 1; min-width: 300px; }
    .card { background: #1e1e1e; padding: 20px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); margin-bottom: 20px; }
    .btn { background: #00e5ff; color: #000; border: none; padding: 10px 16px; margin: 8px 4px 8px 0; font-weight: bold; border-radius: 6px; cursor: pointer; font-size: 13px; transition: 0.2s; }
    .btn:hover { background: #00b3cc; }
    .btn-secondary { background: #333; color: #00e5ff; border: 1px solid #00e5ff; }
    .btn-secondary:hover { background: #00e5ff; color: #000; }
    .btn-danger { background: #ff5252; color: #fff; }
    .btn-danger:hover { background: #cc0000; }
    .control-group { margin: 15px 0; text-align: left; }
    label { display: block; font-size: 12px; color: #aaa; margin-bottom: 5px; }
    .slider-row { display: flex; align-items: center; gap: 10px; }
    input[type=range] { flex: 1; accent-color: #00e5ff; cursor: pointer; }
    .val-badge { font-family: monospace; font-size: 14px; color: #00e5ff; width: 40px; text-align: right; }
    input[type=number], input[type=text], select { width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #333; background: #2a2a2a; color: #fff; box-sizing: border-box; }
    
    /* Visualizer Canvas Layout with Axis & Grid */
    .viz-wrapper { display: flex; flex-direction: column; margin-bottom: 15px; }
    .viz-main { display: flex; flex-direction: row; align-items: center; }
    
    .y-axis-container { display: flex; flex-direction: column; justify-content: space-between; height: 280px; padding-right: 8px; text-align: right; font-family: monospace; font-size: 10px; color: #777; }
    .y-axis-title { writing-mode: vertical-lr; transform: rotate(180deg); text-align: center; font-size: 11px; color: #00e5ff; font-weight: bold; padding-right: 6px; letter-spacing: 1px; }
    
    .visualizer-box { 
      position: relative;
      flex: 1; 
      display: flex; 
      align-items: flex-end; 
      justify-content: center; 
      gap: 3px; 
      height: 280px; 
      background: #000; 
      padding: 0 10px; 
      border-radius: 6px; 
      border: 1px solid #333;
      overflow: hidden;
    }
    
    /* 32x16 Background Grid */
    .grid-bg {
      position: absolute;
      top: 0; left: 0; right: 0; bottom: 0;
      background-size: calc(100% / 16) calc(100% / 32);
      background-image: 
        linear-gradient(to right, rgba(255, 255, 255, 0.05) 1px, transparent 1px),
        linear-gradient(to bottom, rgba(255, 255, 255, 0.05) 1px, transparent 1px);
      pointer-events: none;
      z-index: 1;
    }

    .bar { flex: 1; background: #00e5ff; transition: height 0.05s ease; min-height: 2px; border-radius: 2px 2px 0 0; z-index: 2; }
    
    .x-axis-container { display: flex; flex-direction: row; justify-content: space-between; margin-left: 55px; padding-top: 6px; font-family: monospace; font-size: 9px; color: #777; }
    .x-axis-title { text-align: center; font-size: 11px; color: #00e5ff; font-weight: bold; margin-top: 4px; margin-left: 55px; letter-spacing: 1px; }

    .stat-text { font-size: 12px; color: #00e5ff; margin-bottom: 10px; font-family: monospace; }
    .version-tag { font-size: 11px; color: #777; font-family: monospace; margin-top: 6px; }
    #saveStatus { font-size: 12px; color: #00e5ff; margin-top: 8px; min-height: 16px; font-family: monospace; }
  </style>
</head>
<body>
  <div class="container">
    <!-- Left Panel: Live Visualizer -->
    <div class="left-panel">
      <div class="card">
        <h2>Live Preview</h2>
        <div class="stat-text" id="pktStat">Packets Received: 0</div>
        
        <div class="viz-wrapper">
          <div class="viz-main">
            <div class="y-axis-title">Amplitude (%)</div>
            <div class="y-axis-container">
              <span>100</span>
              <span>75</span>
              <span>50</span>
              <span>25</span>
              <span>0</span>
            </div>
            
            <div class="visualizer-box" id="vizBox">
              <div class="grid-bg"></div>
            </div>
          </div>
          
          <!-- X-Axis Frequency Scale & Title -->
          <div class="x-axis-container">
            <span>20</span>
            <span>60</span>
            <span>125</span>
            <span>250</span>
            <span>500</span>
            <span>1k</span>
            <span>2k</span>
            <span>4k</span>
            <span>8k</span>
            <span>16k</span>
          </div>
          <div class="x-axis-title">Frequency (Hz)</div>
        </div>

        <div>
          <button class="btn" onclick="triggerTest()">Test Rotation</button>
          <button class="btn btn-secondary" onclick="refreshLiveView()">Refresh Live View</button>
          <button class="btn btn-danger" onclick="rebootDevice()">Reboot</button>
        </div>
        
        <div class="version-tag" id="fwVersion">Firmware: v...</div>
      </div>
    </div>

    <!-- Right Panel: Controls & Settings -->
    <div class="right-panel">
      <div class="card">
        <h2>Settings</h2>
        
        <div class="control-group">
          <label>Display Rotation</label>
          <select id="displayRotation">
            <option value="0">0: Portrait (0&deg;)</option>
            <option value="1">1: Landscape (90&deg;)</option>
            <option value="2">2: Inverted Portrait (180&deg;)</option>
            <option value="3">3: Inverted Landscape (270&deg;)</option>
          </select>
        </div>
        
        <div class="control-group">
          <label>UDP Multicast IP</label>
          <input type="text" id="multicastIP" value="239.0.0.1">
        </div>
        
        <div class="control-group">
          <label>UDP Port</label>
          <input type="number" id="udpPort" value="11980">
        </div>

        <!-- Sliders with instant live updates -->
        <div class="control-group">
          <label>Audio Floor Threshold (0-255)</label>
          <div class="slider-row">
            <input type="range" id="audioFloor" min="0" max="255" value="5" oninput="updateSlider('floor', this.value, 'lblAudioFloor')">
            <span class="val-badge" id="lblAudioFloor">5</span>
          </div>
        </div>

        <div class="control-group">
          <label>Audio Gain Multiplier (0.1 - 10.0)</label>
          <div class="slider-row">
            <input type="range" id="audioGain" min="0.1" max="10.0" step="0.1" value="1.0" oninput="updateSlider('gain', this.value, 'lblAudioGain')">
            <span class="val-badge" id="lblAudioGain">1.0</span>
          </div>
        </div>

        <div class="control-group">
          <label>Peak Gravity / Fall Speed (1-10)</label>
          <div class="slider-row">
            <input type="range" id="peakGravity" min="1" max="10" value="3" oninput="updateSlider('gravity', this.value, 'lblPeakGravity')">
            <span class="val-badge" id="lblPeakGravity">3</span>
          </div>
        </div>

        <div class="control-group">
          <label>Visualizer Mode (Applies Immediately)</label>
          <select id="visualizerMode" onchange="autoApplyMode(this.value)">
            <option value="0">0: Default Spectrum</option>
            <option value="1">1: Centered Expand</option>
            <option value="2">2: Rainbow Shift</option>
            <option value="3">3: Top-Down Fall</option>
            <option value="4">4: Purple/Magenta Solid</option>
            <option value="5">5: Dual Split Ends</option>
            <option value="6">6: Green-Yellow-Red Traffic</option>
            <option value="7">7: Peak Line White Cap</option>
            <option value="8">8: Wave Distortion</option>
            <option value="9">9: Center Square Pulse</option>
            <option value="10">10: Dual Color Split</option>
            <option value="11">11: Horizontal Bars Split</option>
            <option value="12">12: Peak Strobe Only</option>
            <option value="13">13: Magenta Flash High</option>
            <option value="14">14: Invert Flash on Beat</option>
            <option value="15">15: Snake Peak Trail</option>
          </select>
        </div>

        <button class="btn" style="width:100%; margin-top:10px;" onclick="saveConfig()">Save Network Settings</button>
        <div id="saveStatus"></div>
      </div>
    </div>
  </div>

  <script>
    let ws;

    function buildBars() {
      let box = document.getElementById('vizBox');
      // Keep background grid inside box
      box.innerHTML = '<div class="grid-bg"></div>';
      for(let i=0; i<16; i++) {
        let bar = document.createElement('div');
        bar.className = 'bar';
        bar.id = 'bar' + i;
        box.appendChild(bar);
      }
    }

    function initWS() {
      if (ws) { ws.close(); }
      ws = new WebSocket('ws://' + window.location.hostname + ':81');
      ws.onmessage = function(event) {
        let data = JSON.parse(event.data);
        if(data.pktCount !== undefined) {
          document.getElementById('pktStat').innerText = "Packets Received: " + data.pktCount;
        }
        if(data.bins && data.bins.length === 16) {
          for(let i=0; i<16; i++) {
            let bar = document.getElementById('bar' + i);
            if(bar) {
              let pct = (data.bins[i] / 255 * 100);
              bar.style.height = Math.max(pct, 1.0) + '%';
            }
          }
        }
      };
      ws.onclose = function() { setTimeout(initWS, 2000); };
    }

    function refreshLiveView() {
      buildBars();
      initWS();
      setStatus('Live View Refreshed!');
    }

    function setStatus(msg) {
      let el = document.getElementById('saveStatus');
      el.innerText = msg;
      setTimeout(() => { if(el.innerText === msg) el.innerText = ''; }, 3000);
    }

    function loadConfig() {
      fetch('/get-config')
        .then(res => res.json())
        .then(cfg => {
          document.getElementById('visualizerMode').value = cfg.visualizerMode;
          document.getElementById('displayRotation').value = cfg.displayRotation;
          document.getElementById('multicastIP').value = cfg.multicastIP;
          document.getElementById('udpPort').value = cfg.udpPort;
          
          document.getElementById('audioFloor').value = cfg.audioFloor || 0;
          document.getElementById('lblAudioFloor').innerText = cfg.audioFloor || 0;
          
          let g = parseFloat(cfg.audioGain || 1.0).toFixed(1);
          document.getElementById('audioGain').value = g;
          document.getElementById('lblAudioGain').innerText = g;
          
          document.getElementById('peakGravity').value = cfg.peakGravity;
          document.getElementById('lblPeakGravity').innerText = cfg.peakGravity;
          
          if(cfg.version) {
            document.getElementById('fwVersion').innerText = 'Firmware: v' + cfg.version;
          }
        });
    }

    function updateSlider(param, val, labelId) {
      document.getElementById(labelId).innerText = val;
      fetch('/set-slider?' + param + '=' + val);
    }

    function autoApplyMode(modeVal) {
      fetch('/set-mode?value=' + modeVal)
        .then(() => setStatus('Mode Changed!'));
    }

    function saveConfig() {
      let cfg = {
        visualizerMode: parseInt(document.getElementById('visualizerMode').value),
        displayRotation: parseInt(document.getElementById('displayRotation').value),
        multicastIP: document.getElementById('multicastIP').value,
        udpPort: parseInt(document.getElementById('udpPort').value),
        audioFloor: parseInt(document.getElementById('audioFloor').value),
        audioGain: parseFloat(document.getElementById('audioGain').value),
        peakGravity: parseInt(document.getElementById('peakGravity').value),
        isDisplayOn: true
      };
      fetch('/save-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(cfg)
      }).then(() => setStatus('Network Settings Saved!'));
    }

    function triggerTest() { fetch('/test-display'); setStatus('Running Rotation Test...'); }
    function rebootDevice() { if(confirm('Reboot ESP32?')) fetch('/reboot'); }

    window.onload = function() {
      buildBars();
      initWS();
      loadConfig();
    };
  </script>
</body>
</html>
)rawliteral";
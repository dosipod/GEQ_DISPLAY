#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

// Build version definition tracker string configuration footprint
#define FIRMWARE_VERSION "v1.2.4-BETA"

// Clear conflicting structural canvas color definitions
#ifdef BLACK
#undef BLACK
#endif
#ifdef GREEN
#undef GREEN
#endif

// Wi-Fi Access Parameters managed via your secure environmental secret bindings
const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASS;

struct DevConfig {
  uint16_t udpPort = 11988;
  char multicastIP[16] = "239.0.0.1";
  uint8_t visualizerMode = 0; 
};
DevConfig config;

struct WledAudioPacket {
  char header[4]; 
  uint8_t sampleAgc;
  uint8_t volumeSmth;
  uint8_t volumeRaw;
  uint8_t binData[16];
};

WiFiUDP udp;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
WledAudioPacket sharedPacket;
portMUX_TYPE dataMutex = portMUX_INITIALIZER_UNLOCKED;
bool newPacketAvailable = false;

// Hardware target display bus definitions matching your tested panel offsets
Arduino_DataBus *bus = new Arduino_ESP32SPI(16, 5, 18, 19, -1);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 23, 1, true, 135, 240, 52, 40, 53, 40);

uint16_t peakHolds[16] = {0};

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>TTGO Music Visualizer Terminal</title>
  <style>
    body { font-family: system-ui, sans-serif; background: #121212; color: #e0e0e0; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .card { background: #1e1e1e; padding: 20px; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.5); width: 100%; max-width: 400px; margin-bottom: 20px; }
    h2 { margin-top: 0; color: #00e676; text-align: center; }
    label { display: block; margin: 12px 0 4px; font-weight: bold; font-size: 14px; }
    input, select, button { width: 100%; padding: 10px; border-radius: 6px; border: 1px solid #333; background: #252525; color: #fff; box-sizing: border-box; }
    button { background: #00e676; color: #121212; font-weight: bold; cursor: pointer; margin-top: 15px; border: none; }
    button:hover { background: #00b55c; }
    #canvas { width: 100%; height: 150px; background: #000; border-radius: 6px; display: block; }
    .ota-link { display: block; text-align: center; color: #a0a0a0; text-decoration: none; margin-top: 10px; font-size: 12px; }
    .ota-link:hover { color: #00e676; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Live Preview</h2>
    <canvas id="canvas" width="240" height="135"></canvas>
  </div>
  <div class="card">
    <h2>Control Console</h2>
    <form id="cfgForm">
      <label>UDP Target Port</label>
      <input type="number" id="port" name="udpPort" min="1" max="65535">
      <label>Multicast Target Core IP</label>
      <input type="text" id="ip" name="multicastIP">
      <label>Spectrum Render Effect Style</label>
      <select id="effect" name="visualizerMode">
        <option value="0">Classical GEQ</option>
        <option value="1">Center-Out Mirror</option>
        <option value="2">Rainbow Flow</option>
      </select>
      <button type="button" onclick="submitConfig()">Save Options</button>
    </form>
    <a href="/update" class="ota-link" target="_blank">Firmware Update Dashboard (OTA)</a>
  </div>
  <script>
    var ws = new WebSocket('ws://' + window.location.hostname + ':81/');
    var ctx = document.getElementById('canvas').getContext('2d');
    
    fetch('/get-config').then(r => r.json()).then(data => {
      document.getElementById('port').value = data.udpPort;
      document.getElementById('ip').value = data.multicastIP;
      document.getElementById('effect').value = data.visualizerMode;
    });

    ws.onmessage = function(evt) {
      var data = JSON.parse(evt.data);
      ctx.clearRect(0, 0, 240, 135);
      var w = (240 / 16) - 2;
      for(var i=0; i<16; i++) {
        var h = (data.bins[i] / 255) * 135;
        ctx.fillStyle = (data.mode == 2) ? 'hsl(' + (i * 22) + ', 100%, 50%)' : '#00e676';
        ctx.fillRect(i * (w + 2), 135 - h, w, h);
      }
    };

    function submitConfig() {
      var payload = {
        udpPort: parseInt(document.getElementById('port').value),
        multicastIP: document.getElementById('ip').value,
        visualizerMode: parseInt(document.getElementById('effect').value)
      };
      fetch('/save-config', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleGetConfig() {
  StaticJsonDocument<200> doc;
  doc["udpPort"] = config.udpPort;
  doc["multicastIP"] = config.multicastIP;
  doc["visualizerMode"] = config.visualizerMode;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleSaveConfig() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg("plain"));
    
    portENTER_CRITICAL(&dataMutex);
    config.udpPort = doc["udpPort"];
    strncpy(config.multicastIP, doc["multicastIP"], 16);
    config.visualizerMode = doc["visualizerMode"];
    portEXIT_CRITICAL(&dataMutex);
    
    udp.stop();
    udp.beginMulticast(IPAddress(239, 0, 0, 1), config.udpPort);
    server.send(200, "text/plain", "OK");
  }
}

// Thread Task 0 Runtime handling all web data pipes safely
void core0WebTask(void * pvParameters) {
  server.on("/", handleRoot);
  server.on("/get-config", handleGetConfig);
  server.on("/save-config", handleSaveConfig);
  
  // Link and configure ElegantOTA interface hooks on route port handles
  ElegantOTA.begin(&server);
  
  server.begin();
  webSocket.begin();

  while(true) {
    server.handleClient();
    webSocket.loop();
    ElegantOTA.loop();

    if (newPacketAvailable) {
      StaticJsonDocument<256> doc;
      JsonArray bins = doc.createNestedArray("bins");
      
      portENTER_CRITICAL(&dataMutex);
      for(int i=0; i<16; i++) bins.add(sharedPacket.binData[i]);
      doc["mode"] = config.visualizerMode;
      newPacketAvailable = false;
      portEXIT_CRITICAL(&dataMutex);

      String jsonString;
      serializeJson(doc, jsonString);
      webSocket.broadcastTXT(jsonString);
    }
    vTaskDelay(pdMS_TO_TICKS(12)); 
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  gfx->begin();
  gfx->setRotation(1);
  gfx->fillScreen(RGB565_BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(10, 40);
  gfx->println("Booting Client Node...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(400); }

  // ----------------------------------------------------
  // MANDATORY 2-SECOND DELAY SPLASH SCREEN FOR BOOT METRICS
  // ----------------------------------------------------
  gfx->fillScreen(RGB565_BLACK);
  gfx->setCursor(10, 25);
  gfx->setTextColor(RGB565_GREEN);
  gfx->println("SYSTEM STATUS: ONLINE");
  
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(10, 55);
  gfx->print("IP: "); 
  gfx->println(WiFi.localIP());
  
  gfx->setCursor(10, 85);
  gfx->print("VER: ");
  gfx->setTextColor(RGB565_YELLOW);
  gfx->println(FIRMWARE_VERSION);
  
  // Lock display execution pattern exactly for 2 seconds to make information readable
  delay(2000); 
  
  gfx->fillScreen(RGB565_BLACK); // Clear layout for visualizer execution canvas

  udp.beginMulticast(IPAddress(239,0,0,1), config.udpPort);
  xTaskCreatePinnedToCore(core0WebTask, "WebTask", 8192, NULL, 1, NULL, 0);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize >= sizeof(WledAudioPacket)) {
    WledAudioPacket rawPacket;
    udp.read((char*)&rawPacket, sizeof(WledAudioPacket));

    if (memcmp(rawPacket.header, "wsa2", 4) == 0) {
      portENTER_CRITICAL(&dataMutex);
      memcpy(&sharedPacket, &rawPacket, sizeof(WledAudioPacket));
      newPacketAvailable = true;
      uint8_t targetMode = config.visualizerMode;
      portEXIT_CRITICAL(&dataMutex);

      int sW = gfx->width();
      int sH = gfx->height();
      int numBars = 16;
      int gap = 2;
      int barW = (sW - (gap * (numBars + 1))) / numBars;

      gfx->startWrite();
      for (int i = 0; i < numBars; i++) {
        int rawVal = sharedPacket.binData[i];
        int barH = map(rawVal, 0, 255, 0, sH - 12);
        int xPos = gap + (i * (barW + gap));
        
        uint16_t color = RGB565_GREEN;
        if (targetMode == 2) {
          uint8_t r = (i * 16); uint8_t g = 255 - r; uint8_t b = 128;
          color = gfx->color565(r, g, b);
        }

        if (targetMode == 1) { 
          int midY = sH / 2;
          int halfH = barH / 2;
          gfx->writeFillRect(xPos, midY - halfH, barW, halfH * 2, color);
          gfx->writeFillRect(xPos, 0, barW, midY - halfH, RGB565_BLACK);
          gfx->writeFillRect(xPos, midY + halfH, barW, sH - (midY + halfH), RGB565_BLACK);
        } else { 
          int yPos = sH - barH;
          gfx->writeFillRect(xPos, yPos, barW, barH, color);
          gfx->writeFillRect(xPos, 0, barW, yPos, RGB565_BLACK);

          if (barH >= peakHolds[i]) {
            peakHolds[i] = barH;
          } else {
            if (peakHolds[i] > 0) peakHolds[i]--;
          }
          if(peakHolds[i] > 0) {
            gfx->writeFillRect(xPos, sH - peakHolds[i], barW, 2, RGB565_RED);
          }
        }
      }
      gfx->endWrite();
    }
  }
}

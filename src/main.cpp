#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include "config.h"

// Forward declaration function prototypes
void saveSettingsToFlash();
void loadSettingsFromFlash();

TaskHandle_t networkTaskHandle = NULL;

#include "web_handlers.h"

#ifdef BLACK
#undef BLACK
#endif
#ifdef GREEN
#undef GREEN
#endif

const char* ssid     = "toi";
const char* password = "dcba@4321";
DevConfig config;

WiFiUDP udp;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Preferences preferences;
portMUX_TYPE dataMutex = portMUX_INITIALIZER_UNLOCKED;

uint8_t sharedVisualizerBins[16] = {0};
uint8_t previousBins[16] = {0};
unsigned long lastPacketTime = 0; 
unsigned long lastRenderedPacketCount = 0;

uint8_t globalNetworkPacketBuffer[512] = {0};

bool newPacketAvailable = false;
bool isTestModeActive = false;
bool isUdpConnected = false;
String testColorHexWeb = "#000000";

volatile bool triggerHardwareReboot = false;
volatile bool triggerUdpReinit = false;

unsigned long lastDebugPrint = 0;
unsigned long validPacketCount = 0;
unsigned long droppedPacketCount = 0;

Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;
uint16_t peakHolds[16] = {0}; 

void loadSettingsFromFlash() {
  preferences.begin("geq_cfg", true);
  config.udpPort = preferences.getUShort("port", 11980);
  String ip = preferences.getString("ip", "239.0.0.1");
  if (ip.length() < 7 || ip == "" || ip.startsWith("0.") || ip.endsWith(".0")) { 
    ip = "239.0.0.1"; 
  }
  
  memset(config.multicastIP, 0, 16);
  strncpy(config.multicastIP, ip.c_str(), 15);
  
  config.visualizerMode = preferences.getUChar("mode", 1);
  config.displayRotation = preferences.getUChar("rot", 3);
  config.audioFloor = preferences.getUChar("floor", 25); 
  config.audioGain = preferences.getUChar("gain", 15);
  preferences.end();
}

void saveSettingsToFlash() {
  preferences.begin("geq_cfg", false);
  preferences.putUShort("port", config.udpPort);
  preferences.putString("ip", String(config.multicastIP));
  preferences.putUChar("mode", config.visualizerMode);
  preferences.putUChar("rot", config.displayRotation);
  preferences.putUChar("floor", config.audioFloor);
  preferences.putUChar("gain", config.audioGain);
  preferences.end();
}

void beginUdpMulticast() {
  isUdpConnected = false;
  udp.stop();
  vTaskDelay(pdMS_TO_TICKS(150)); 
  const char* targetIPStr = (strlen(config.multicastIP) > 6) ? config.multicastIP : "239.0.0.1";
  uint16_t targetPort = (config.udpPort > 0) ? config.udpPort : 11980;
  
  IPAddress groupIP;
  if (groupIP.fromString(targetIPStr)) { 
    if (udp.beginMulticast(groupIP, targetPort)) {
      Serial.printf("[NET] Listening on Multicast: %s:%d\n", targetIPStr, targetPort);
      isUdpConnected = true;
      lastPacketTime = millis();
    }
  }
}

void core1NetworkIngestTask(void * pvParameters) {
  while(true) {
    if (isUdpConnected) {
      int packetSize = udp.parsePacket();
      if (packetSize >= 16 && packetSize < 512) {
        int readLen = udp.read((char*)globalNetworkPacketBuffer, packetSize);
        if (readLen >= 16) {
          portENTER_CRITICAL(&dataMutex);
          validPacketCount++; 
          lastPacketTime = millis(); 
          
          int dataOffset = readLen - 16; 
          uint8_t floorGate = config.audioFloor;
          float gainMultiplier = (float)config.audioGain / 10.0f;
          
          for (int b = 0; b < 16; b++) {
            uint8_t rawVal = globalNetworkPacketBuffer[dataOffset + b];
            if (rawVal <= floorGate) {
              sharedVisualizerBins[b] = 0;
            } else {
              int processed = (int)((rawVal - floorGate) * gainMultiplier);
              sharedVisualizerBins[b] = (processed > 255) ? 255 : (uint8_t)processed;
            }
          }
          newPacketAvailable = true;
          portEXIT_CRITICAL(&dataMutex);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2)); 
  }
}

void setup() {
  Serial.begin(115200); delay(500); loadSettingsFromFlash();
  pinMode(4, OUTPUT); digitalWrite(4, HIGH);
  bus = new Arduino_ESP32SPI(16, 5, 18, 19, -1, VSPI_HOST);
  gfx = new Arduino_ST7789(bus, 23, config.displayRotation, true, 135, 240, 52, 40, 53, 40);
  gfx->begin(80000000L); gfx->fillScreen(RGB565_BLACK);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(200);

  beginUdpMulticast();
  xTaskCreatePinnedToCore(core0WebTask, "WebTask", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(core1NetworkIngestTask, "NetIngest", 4096, NULL, 2, &networkTaskHandle, 1);
}

void loop() {
  if (triggerHardwareReboot) {
    if(networkTaskHandle != NULL) vTaskDelete(networkTaskHandle);
    udp.stop(); webSocket.close(); server.stop(); WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(250)); ESP.restart();
  }
  if (triggerUdpReinit) { triggerUdpReinit = false; beginUdpMulticast(); }

  if (isTestModeActive) {
    uint16_t testColors[] = {RGB565_RED, RGB565_GREEN, RGB565_BLUE, RGB565_WHITE};
    const char* webHex[]  = {"#ff0000", "#00ff00", "#0000ff", "#ffffff"};
    uint8_t currentRotation = config.displayRotation;
    
    gfx->setRotation(currentRotation);
    for(int c = 0; c < 4; c++) {
      testColorHexWeb = webHex[c]; gfx->fillScreen(testColors[c]);
      gfx->setTextSize(2); gfx->setTextColor(RGB565_BLACK); gfx->setCursor(15, 40);
      gfx->printf("ROTATION ID: %d", currentRotation);
      gfx->setCursor(15, 75);
      if (currentRotation == 1 || currentRotation == 3) { gfx->print("LANDSCAPE MODE"); } else { gfx->print("PORTRAIT MODE"); }
      vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
    isTestModeActive = false; gfx->fillScreen(RGB565_BLACK); lastPacketTime = millis(); return;
  }

  // FIXED: Removed redundant udp.parsePacket() call. Core 1 now checks the asynchronous timeout status smoothly
  if (isUdpConnected) {
    if (!newPacketAvailable && (millis() - lastPacketTime > 150)) {
      portENTER_CRITICAL(&dataMutex);
      bool hasDataToDecay = false;
      for (int b = 0; b < 16; b++) {
        if (sharedVisualizerBins[b] > 0) {
          if (sharedVisualizerBins[b] > 35) sharedVisualizerBins[b] -= 35; else sharedVisualizerBins[b] = 0;
          hasDataToDecay = true;
        }
      }
      if (!hasDataToDecay) {
        for (int i = 0; i < 16; i++) { peakHolds[i] = 0; previousBins[i] = 0; }
        gfx->fillScreen(RGB565_BLACK);
        lastRenderedPacketCount = 0; 
      } else { 
        newPacketAvailable = true; 
      }
      lastPacketTime = millis(); 
      portEXIT_CRITICAL(&dataMutex);
    }
  }

  if (newPacketAvailable && !isTestModeActive) {
    uint8_t localBins[16]; uint8_t targetMode; uint8_t activeRotation;
    unsigned long currentValidCount;
    
    portENTER_CRITICAL(&dataMutex);
    memcpy(localBins, sharedVisualizerBins, 16);
    targetMode = config.visualizerMode; activeRotation = config.displayRotation;
    currentValidCount = validPacketCount;
    newPacketAvailable = false; 
    portEXIT_CRITICAL(&dataMutex);

    int baseW = 240; int baseH = 135;
    if (activeRotation == 0 || activeRotation == 2) { baseW = 135; baseH = 240; }
    gfx->setRotation(activeRotation);
    int numBars = 16; int gap = 2;
    int barW = (baseW - (gap * (numBars + 1))) / numBars;

    if (currentValidCount != lastRenderedPacketCount) {
      lastRenderedPacketCount = currentValidCount;
      char countStr[32];
      snprintf(countStr, sizeof(countStr), "PKT:%lu", currentValidCount);
      gfx->setTextSize(1); gfx->setTextColor(RGB565_YELLOW, RGB565_BLACK); 
      gfx->setCursor(baseW - 55, 2); gfx->print(countStr);
    }

    gfx->startWrite();
    for (int i = 0; i < numBars; i++) {
      int rawVal = localBins[i]; 
      if (rawVal == 0 && peakHolds[i] > 0) { if (peakHolds[i] > 4) peakHolds[i] -= 5; else peakHolds[i] = 0; }
      int barH = map(rawVal, 0, 255, 0, baseH - 12);
      int xPos = gap + (i * (barW + gap));

      if (barH >= peakHolds[i]) peakHolds[i] = barH; 
      else if (peakHolds[i] > 0) { if (peakHolds[i] > 2) peakHolds[i] -= 3; else peakHolds[i] = 0; }

      if (rawVal == previousBins[i] && barH == 0 && peakHolds[i] == 0) continue; 
      previousBins[i] = rawVal;

      uint16_t color = RGB565_GREEN;
      if (targetMode == 1) color = RGB565_CYAN;
      else if (targetMode == 2) color = gfx->color565((i * 16), 255 - (i * 16), 128); 
      else if (targetMode == 3) color = gfx->color565(255, 255 - rawVal, 0);         
      else if (targetMode == 4) color = gfx->color565(rawVal, 0, 255);                 

      int x, y, w, h;
      if (targetMode == 1) { 
        int midY = baseH / 2; int halfH = barH / 2;
        x = xPos; y = midY - halfH; w = barW; h = halfH * 2;
        gfx->writeFillRect(x, y, w, h, color); gfx->writeFillRect(x, 0, w, y, RGB565_BLACK);
        gfx->writeFillRect(x, midY + halfH, w, baseH - (midY + halfH), RGB565_BLACK);
      } else if (targetMode == 3) { 
        x = xPos; y = 0; w = barW; h = barH;
        gfx->writeFillRect(x, y, w, h, color); gfx->writeFillRect(x, h, w, baseH - h, RGB565_BLACK);
      } else { 
        x = xPos; y = baseH - barH; w = barW; h = barH;
        gfx->writeFillRect(x, y, w, h, color); gfx->writeFillRect(x, 0, w, y, RGB565_BLACK);
        if (peakHolds[i] > 0) gfx->writeFillRect(x, baseH - peakHolds[i], w, 2, RGB565_RED); 
      }
    }
    gfx->endWrite();
  }
  vTaskDelay(pdMS_TO_TICKS(1)); 
}

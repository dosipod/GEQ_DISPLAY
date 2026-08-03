#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Preferences preferences;
WiFiUDP udp;
portMUX_TYPE dataMutex = portMUX_INITIALIZER_UNLOCKED;

#include "config.h"

DevConfig config;
uint8_t sharedVisualizerBins[16] = {0};
uint8_t previousBins[16] = {0};
uint16_t peakHolds[16] = {0}; 
uint8_t globalNetworkPacketBuffer[512] = {0};

unsigned long lastPacketTime = 0; 
unsigned long lastRenderedPacketCount = 0;

bool newPacketAvailable = false;
bool isTestModeActive = false;
bool isUdpConnected = false;
String testColorHexWeb = "#000000";

volatile bool triggerHardwareReboot = false;
volatile bool triggerUdpReinit = false;
bool internalDisplaySleepStateTracker = false;

unsigned long validPacketCount = 0;
unsigned long droppedPacketCount = 0;

TaskHandle_t networkTaskHandle = NULL;
Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;

#include "network.h"     
#include "web_handlers.h"
#include "effects.h"     

const char* ssid     = "toi";
const char* password = "dcba@4321";

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
  
  internalDisplaySleepStateTracker = !config.isDisplayOn;
  if(internalDisplaySleepStateTracker) { gfx->displayOff(); digitalWrite(4, LOW); }
}

void loop() {
  if (triggerHardwareReboot) {
    if(networkTaskHandle != NULL) vTaskDelete(networkTaskHandle);
    udp.stop(); webSocket.close(); server.stop(); WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(250)); ESP.restart();
  }
  if (triggerUdpReinit) { triggerUdpReinit = false; beginUdpMulticast(); }

  bool currentPowerSetting;
  portENTER_CRITICAL(&dataMutex); currentPowerSetting = config.isDisplayOn; portEXIT_CRITICAL(&dataMutex);

  if (!currentPowerSetting) {
    if (!internalDisplaySleepStateTracker) {
      internalDisplaySleepStateTracker = true; gfx->fillScreen(RGB565_BLACK);
      gfx->displayOff(); digitalWrite(4, LOW); 
    }
    vTaskDelay(pdMS_TO_TICKS(50)); return; 
  } else {
    if (internalDisplaySleepStateTracker) {
      internalDisplaySleepStateTracker = false; digitalWrite(4, HIGH);
      gfx->displayOn(); gfx->fillScreen(RGB565_BLACK);
    }
  }

  if (isTestModeActive) {
    uint16_t testColors[] = {RGB565_RED, RGB565_GREEN, RGB565_BLUE, RGB565_WHITE};
    const char* webHex[]  = {"#ff0000", "#00ff00", "#0000ff", "#ffffff"};
    uint8_t currentRotation = config.displayRotation; gfx->setRotation(currentRotation);
    for(int c = 0; c < 4; c++) {
      testColorHexWeb = webHex[c]; gfx->fillScreen(testColors[c]);
      gfx->setTextSize(2); gfx->setTextColor(RGB565_BLACK); gfx->setCursor(15, 40);
      gfx->printf("ROTATION ID: %d", currentRotation); gfx->setCursor(15, 75);
      if (currentRotation == 1 || currentRotation == 3) { gfx->print("LANDSCAPE MODE"); } else { gfx->print("PORTRAIT MODE"); }
      vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
    isTestModeActive = false; gfx->fillScreen(RGB565_BLACK); lastPacketTime = millis(); return;
  }

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
        gfx->fillScreen(RGB565_BLACK); lastRenderedPacketCount = 0; 
      } else { newPacketAvailable = true; }
      lastPacketTime = millis(); portEXIT_CRITICAL(&dataMutex);
    }
  }

  if (newPacketAvailable && !isTestModeActive) {
    uint8_t localBins[16]; uint8_t targetMode; uint8_t activeRotation; uint8_t fallSpeed; unsigned long currentValidCount;
    portENTER_CRITICAL(&dataMutex);
    memcpy(localBins, sharedVisualizerBins, 16);
    targetMode = config.visualizerMode; activeRotation = config.displayRotation;
    fallSpeed = config.peakGravity; currentValidCount = validPacketCount;
    newPacketAvailable = false; portEXIT_CRITICAL(&dataMutex);

    int baseW = 240; int baseH = 135;
    if (activeRotation == 0 || activeRotation == 2) { baseW = 135; baseH = 240; }
    gfx->setRotation(activeRotation);
    int numBars = 16; int gap = 2; int barW = (baseW - (gap * (numBars + 1))) / numBars;

    if (currentValidCount != lastRenderedPacketCount) {
      lastRenderedPacketCount = currentValidCount; char countStr[16];
      snprintf(countStr, sizeof(countStr), "PKT:%lu", currentValidCount);
      gfx->setTextSize(1); gfx->setTextColor(RGB565_YELLOW, RGB565_BLACK); 
      gfx->setCursor(baseW - 55, 2); gfx->print(countStr);
    }

    uint32_t totalVolumeSum = 0;
    for (int i = 0; i < 16; i++) totalVolumeSum += localBins[i];
    uint8_t globalAverageVolume = totalVolumeSum / 16;

    gfx->startWrite();
    if (targetMode == 9) {
      int maxRadiusW = baseW / 2; int maxRadiusH = baseH / 2;
      int pulseW = map(globalAverageVolume, 0, 255, 4, maxRadiusW); int pulseH = map(globalAverageVolume, 0, 255, 4, maxRadiusH);
      int centerX = baseW / 2; int centerY = baseH / 2;
      uint16_t outerBoxColor = gfx->color565(0, globalAverageVolume, 255); uint16_t innerBoxColor = gfx->color565(255, 0, globalAverageVolume);
      
      gfx->writeFillRect(centerX - pulseW, centerY - pulseH, pulseW * 2, 3, outerBoxColor);
      gfx->writeFillRect(centerX - pulseW, centerY + pulseH - 3, pulseW * 2, 3, outerBoxColor);
      gfx->writeFillRect(centerX - pulseW, centerY - pulseH, 3, pulseH * 2, outerBoxColor);
      gfx->writeFillRect(centerX + pulseW - 3, centerY - pulseH, 3, pulseH * 2, outerBoxColor);
      
      int innerW = pulseW / 2; int innerH = pulseH / 2;
      gfx->writeFillRect(centerX - innerW, centerY - innerH, innerW * 2, 2, innerBoxColor);
      gfx->writeFillRect(centerX - innerW, centerY + innerH - 2, innerW * 2, 2, innerBoxColor);
      gfx->writeFillRect(centerX - innerW, centerY - innerH, 2, innerH * 2, innerBoxColor);
      gfx->writeFillRect(centerX + innerW - 2, centerY - innerH, 2, innerH * 2, innerBoxColor);
      
      gfx->writeFillRect(0, 0, centerX - pulseW, baseH, RGB565_BLACK); gfx->writeFillRect(centerX + pulseW, 0, baseW - (centerX + pulseW), baseH, RGB565_BLACK);
      gfx->writeFillRect(centerX - pulseW, 0, pulseW * 2, centerY - pulseH, RGB565_BLACK); gfx->writeFillRect(centerX - pulseW, centerY + pulseH, pulseW * 2, baseH - (centerY + pulseH), RGB565_BLACK);
      gfx->writeFillRect(centerX - innerW + 2, centerY - innerH + 2, (innerW * 2) - 4, (innerH * 2) - 4, RGB565_BLACK);
    } 
    else {
      for (int i = 0; i < numBars; i++) {
        int rawVal = localBins[i]; 
        if (rawVal == 0 && peakHolds[i] > 0) { if (peakHolds[i] > (fallSpeed + 1)) peakHolds[i] -= (fallSpeed + 1); else peakHolds[i] = 0; }
        int barH = map(rawVal, 0, 255, 0, baseH - 12); int xPos = gap + (i * (barW + gap));

        if (barH >= peakHolds[i]) peakHolds[i] = barH; 
        else if (peakHolds[i] > 0) { if (peakHolds[i] > fallSpeed) peakHolds[i] -= fallSpeed; else peakHolds[i] = 0; }

        if (rawVal == previousBins[i] && barH == 0 && peakHolds[i] == 0) continue; 
        previousBins[i] = rawVal;

        renderSpectrumEffects(targetMode, i, xPos, barW, baseW, baseH, barH, rawVal, fallSpeed, globalAverageVolume, localBins, previousBins);
      }
    }
    gfx->endWrite();
  }
  vTaskDelay(pdMS_TO_TICKS(1)); 
}
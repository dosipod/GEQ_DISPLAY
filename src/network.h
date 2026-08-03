#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include "config.h"

// Reference global storage objects instantiated inside main.cpp safely
extern WebServer server;
extern WebSocketsServer webSocket;
extern DevConfig config;
extern WiFiUDP udp;
extern Preferences preferences;
extern portMUX_TYPE dataMutex;

// FIXED: Standardized true 16-element explicit array boundaries for compilation safety
extern uint8_t sharedVisualizerBins[16];
extern uint8_t previousBins[16];
extern uint16_t peakHolds[16];
extern uint8_t globalNetworkPacketBuffer[512];

extern bool newPacketAvailable;
extern bool isUdpConnected;
extern unsigned long lastPacketTime;
extern unsigned long validPacketCount;

inline void loadSettingsFromFlash() {
  preferences.begin("geq_cfg", true);
  config.udpPort = preferences.getUShort("port", 11980);
  String ip = preferences.getString("ip", "239.0.0.1");
  if (ip.length() < 7 || ip == "" || ip.startsWith("0.") || ip.endsWith(".0")) { ip = "239.0.0.1"; }
  memset(config.multicastIP, 0, 16);
  strncpy(config.multicastIP, ip.c_str(), 15);
  config.visualizerMode = preferences.getUChar("mode", 1);
  config.displayRotation = preferences.getUChar("rot", 3);
  config.audioFloor = preferences.getUChar("floor", 25); 
  config.audioGain = preferences.getUChar("gain", 15);
  config.peakGravity = preferences.getUChar("grav", 3); 
  config.isDisplayOn = preferences.getBool("pwr", true); 
  preferences.end();
}

inline void saveSettingsToFlash() {
  preferences.begin("geq_cfg", false);
  preferences.putUShort("port", config.udpPort);
  preferences.putString("ip", String(config.multicastIP));
  preferences.putUChar("mode", config.visualizerMode);
  preferences.putUChar("rot", config.displayRotation);
  preferences.putUChar("floor", config.audioFloor);
  preferences.putUChar("gain", config.audioGain);
  preferences.putUChar("grav", config.peakGravity); 
  preferences.putBool("pwr", config.isDisplayOn); 
  preferences.end();
}

inline void beginUdpMulticast() {
  isUdpConnected = false;
  udp.stop();
  vTaskDelay(pdMS_TO_TICKS(150)); 
  const char* targetIPStr = (strlen(config.multicastIP) > 6) ? config.multicastIP : "239.0.0.1";
  uint16_t targetPort = (config.udpPort > 0) ? config.udpPort : 11980;
  IPAddress groupIP;
  if (groupIP.fromString(targetIPStr)) { 
    if (udp.beginMulticast(groupIP, targetPort)) {
      isUdpConnected = true;
      lastPacketTime = millis();
    }
  }
}

inline void core1NetworkIngestTask(void * pvParameters) {
  while(true) {
    if (isUdpConnected) {
      int packetSize = udp.parsePacket();
      if (packetSize >= 16 && packetSize < 512) {
        int readLen = udp.read((char*)globalNetworkPacketBuffer, packetSize);
        if (readLen >= 16) {
          portENTER_CRITICAL(&dataMutex);
          validPacketCount++; lastPacketTime = millis(); 
          int dataOffset = readLen - 16; 
          uint8_t floorGate = config.audioFloor;
          float gainMultiplier = (float)config.audioGain / 10.0f;
          
          for (int b = 0; b < 16; b++) {
            uint8_t rawVal = globalNetworkPacketBuffer[dataOffset + b];
            if (rawVal <= floorGate) sharedVisualizerBins[b] = 0;
            else {
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

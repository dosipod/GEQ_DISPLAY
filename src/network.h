#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "config.h"

extern WiFiUDP udp;
extern portMUX_TYPE dataMutex;
extern uint8_t sharedVisualizerBins[16];
extern uint8_t globalNetworkPacketBuffer[512];
extern bool newPacketAvailable;
extern bool isUdpConnected;
extern unsigned long validPacketCount;
extern unsigned long droppedPacketCount;
extern unsigned long lastPacketTime;

static float smoothBins[16] = {0.0f};

inline void beginUdpMulticast() {
  udp.stop();
  IPAddress multiIP;
  if (multiIP.fromString(config.multicastIP)) {
    if (udp.beginMulticast(multiIP, config.udpPort)) {
      isUdpConnected = true;
      Serial.printf("[UDP] Listening on %s:%d\n", config.multicastIP, config.udpPort);
    } else {
      isUdpConnected = false;
      Serial.println("[UDP] Multicast join failed!");
    }
  }
}

inline void core1NetworkIngestTask(void * pvParameters) {
  while (true) {
    if (isUdpConnected) {
      int packetSize = udp.parsePacket();
      if (packetSize > 0) {
        int len = udp.read(globalNetworkPacketBuffer, sizeof(globalNetworkPacketBuffer));
        
        if (len >= 16) {
          portENTER_CRITICAL(&dataMutex);
          
          // Determine byte offset for packet headers
          int offset = 0;
          if (len > 16 && (len % 16 != 0)) {
            offset = len - 16;
          }

          // Step 1: Find peak in current frame for adaptive scaling
          uint8_t frameMax = 1;
          for (int i = 0; i < 16; i++) {
            uint8_t v = globalNetworkPacketBuffer[offset + i];
            if (v > frameMax) frameMax = v;
          }

          // Step 2: Process bins with responsive headroom
          for (int i = 0; i < 16; i++) {
            uint8_t rawVal = globalNetworkPacketBuffer[offset + i];
            
            // Scaled floor threshold relative to signal
            float effectiveFloor = (float)config.audioFloor;
            
            float processedVal = 0.0f;
            if (rawVal > effectiveFloor) {
              processedVal = (float)(rawVal - effectiveFloor);
            }
            
            // Apply Audio Gain
            processedVal *= config.audioGain;
            
            // Hard cap at 255
            if (processedVal > 255.0f) processedVal = 255.0f;

            // Attack & Release Smoothing
            if (processedVal >= smoothBins[i]) {
              // Immediate peak response
              smoothBins[i] = processedVal;
            } else {
              // Smooth decay based on Peak Gravity setting
              float decayFactor = 0.60f + (config.peakGravity * 0.03f); // 0.63 - 0.90
              if (decayFactor > 0.95f) decayFactor = 0.95f;
              
              smoothBins[i] = (smoothBins[i] * decayFactor) + (processedVal * (1.0f - decayFactor));
              if (smoothBins[i] < 1.0f) smoothBins[i] = 0.0f;
            }

            sharedVisualizerBins[i] = (uint8_t)smoothBins[i];
          }
          
          validPacketCount++;
          newPacketAvailable = true;
          lastPacketTime = millis();
          portEXIT_CRITICAL(&dataMutex);
        } else {
          droppedPacketCount++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
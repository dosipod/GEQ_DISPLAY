#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include "config.h"
#include "html.h"

extern WebServer server;
extern WebSocketsServer webSocket;
extern uint8_t sharedVisualizerBins[16];
extern portMUX_TYPE dataMutex;
extern bool newPacketAvailable;
extern bool isTestModeActive;
extern String testColorHexWeb;

void drawRotationTestText(uint8_t rot);

inline void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

inline void handleGetConfig() {
  StaticJsonDocument<512> doc;
  doc["udpPort"] = config.udpPort;
  doc["multicastIP"] = String(config.multicastIP); 
  doc["visualizerMode"] = config.visualizerMode;
  doc["displayRotation"] = config.displayRotation;
  doc["version"] = FIRMWARE_VERSION;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

inline void handleSaveConfig() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<512> doc;
    if (!deserializeJson(doc, server.arg("plain"))) {
      portENTER_CRITICAL(&dataMutex);
      config.udpPort = doc["udpPort"] | 11980;
      const char* incomingIP = doc["multicastIP"];
      if (incomingIP && strlen(incomingIP) > 5) {
        strncpy(config.multicastIP, incomingIP, 15);
        config.multicastIP[15] = '\0';
      } else {
        strcpy(config.multicastIP, "239.0.0.1");
      }
      config.visualizerMode = doc["visualizerMode"] | 1;
      config.displayRotation = doc["displayRotation"] | 1;
      portEXIT_CRITICAL(&dataMutex);
      
      extern void saveSettingsToFlash();
      saveSettingsToFlash();
      
      server.send(200, "text/plain", "OK");
      server.client().flush();
      vTaskDelay(pdMS_TO_TICKS(50));
      
      drawRotationTestText(config.displayRotation);
      triggerUdpReinit = true; 
    } else {
      server.send(400, "text/plain", "JSON Error");
    }
  }
}

inline void handleTestDisplay() { 
  server.send(200, "text/plain", "TEST_START"); 
  server.client().flush();
  vTaskDelay(pdMS_TO_TICKS(20));
  isTestModeActive = true; 
}

inline void handleReboot() { 
  Serial.println("[WEB] Reboot button clicked! Scheduling immediate reset...");
  server.send(200, "text/plain", "REBOOTING"); 
  server.client().flush();
  vTaskDelay(pdMS_TO_TICKS(100)); // Allow packet transmission to finalize completely
  
  // Set the shared volatile flag
  triggerRebootFlag = true; 
}

inline void handleNotFound() {
  String path = server.uri();
  if (path == "/" || path == "") { handleRoot(); }
  else if (path == "/get-config") { handleGetConfig(); }
  else if (path == "/save-config") { handleSaveConfig(); }
  else { server.send(404, "text/plain", "Not Found"); }
}

inline void core0WebTask(void * pvParameters) {
  server.on("/", handleRoot);
  server.on("/get-config", handleGetConfig);
  server.on("/save-config", handleSaveConfig);
  server.on("/test-display", handleTestDisplay);
  server.on("/reboot", handleReboot);
  server.onNotFound(handleNotFound);
  
  ElegantOTA.begin(&server);
  server.begin();
  webSocket.begin();

  while(true) {
    server.handleClient();
    webSocket.loop();
    ElegantOTA.loop();
    
    if (isTestModeActive) {
      StaticJsonDocument<128> doc;
      doc["isTest"] = true;
      doc["testColor"] = testColorHexWeb;
      String jsonString;
      serializeJson(doc, jsonString);
      webSocket.broadcastTXT(jsonString);
      vTaskDelay(pdMS_TO_TICKS(100));
    } else if (newPacketAvailable) {
      StaticJsonDocument<512> doc;
      JsonArray bins = doc.createNestedArray("bins");
      portENTER_CRITICAL(&dataMutex);
      for(int i=0; i<16; i++) bins.add(sharedVisualizerBins[i]);
      doc["mode"] = config.visualizerMode;
      doc["isTest"] = false;
      newPacketAvailable = false;
      portEXIT_CRITICAL(&dataMutex);
      String jsonString;
      serializeJson(doc, jsonString);
      webSocket.broadcastTXT(jsonString);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}


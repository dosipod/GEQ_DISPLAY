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
extern uint8_t sharedVisualizerBins[];
extern portMUX_TYPE dataMutex;
extern bool newPacketAvailable;
extern bool isTestModeActive;
extern String testColorHexWeb;

extern volatile bool triggerHardwareReboot;
extern volatile bool triggerUdpReinit;
extern unsigned long validPacketCount; 

inline void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

inline void handleGetConfig() {
  StaticJsonDocument<512> doc;
  doc["udpPort"] = config.udpPort;
  doc["multicastIP"] = String(config.multicastIP);
  doc["audioFloor"] = (int)config.audioFloor;
  doc["audioGain"] = config.audioGain; 
  doc["peakGravity"] = (int)config.peakGravity; 
  doc["isDisplayOn"] = config.isDisplayOn; 
  doc["visualizerMode"] = config.visualizerMode;
  doc["displayRotation"] = config.displayRotation;
  doc["version"] = FIRMWARE_VERSION;
  String out;
  serializeJson(doc, out); 
  server.send(200, "application/json", out);
}

inline void handleSetMode() {
  if (server.hasArg("value")) {
    uint8_t newMode = server.arg("value").toInt();
    portENTER_CRITICAL(&dataMutex);
    config.visualizerMode = newMode;
    portEXIT_CRITICAL(&dataMutex);
    saveSettingsToFlash();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameter");
  }
}

inline void handleSetPower() {
  if (server.hasArg("state")) {
    bool newState = (server.arg("state").toInt() == 1);
    portENTER_CRITICAL(&dataMutex);
    config.isDisplayOn = newState;
    portEXIT_CRITICAL(&dataMutex);
    saveSettingsToFlash();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameter");
  }
}

inline void handleSetSlider() {
  if (server.hasArg("floor")) {
    portENTER_CRITICAL(&dataMutex);
    config.audioFloor = server.arg("floor").toInt();
    portEXIT_CRITICAL(&dataMutex);
  }
  if (server.hasArg("gain")) {
    portENTER_CRITICAL(&dataMutex);
    config.audioGain = server.arg("gain").toFloat();
    portEXIT_CRITICAL(&dataMutex);
  }
  if (server.hasArg("gravity")) {
    portENTER_CRITICAL(&dataMutex);
    config.peakGravity = server.arg("gravity").toInt();
    portEXIT_CRITICAL(&dataMutex);
  }
  saveSettingsToFlash();
  server.send(200, "text/plain", "OK");
}

inline void handleSaveConfig() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (!err) {
      portENTER_CRITICAL(&dataMutex);
      config.udpPort = doc["udpPort"] | 11980;
      String incomingIP = doc["multicastIP"] | "239.0.0.1";
      strncpy(config.multicastIP, incomingIP.c_str(), 15);
      config.multicastIP[15] = '\0'; 
      config.audioFloor = doc["audioFloor"].as<uint8_t>();
      config.audioGain = doc["audioGain"].as<float>();
      config.peakGravity = doc["peakGravity"] | 3; 
      if (doc.containsKey("isDisplayOn")) {
        config.isDisplayOn = doc["isDisplayOn"].as<bool>();
      }
      config.visualizerMode = doc["visualizerMode"] | 0;
      config.displayRotation = doc["displayRotation"] | 3;
      portEXIT_CRITICAL(&dataMutex);
      
      saveSettingsToFlash();
      
      server.send(200, "text/plain", "OK");
      server.client().stop(); 
      triggerUdpReinit = true;
    }
  }
}

inline void handleTestDisplay() { server.send(200, "text/plain", "TEST_START"); server.client().stop(); isTestModeActive = true; }
inline void handleReboot() { server.send(200, "text/plain", "REBOOTING"); server.client().stop(); triggerHardwareReboot = true; }

inline void handleNotFound() {
  String path = server.uri();
  if (path == "/" || path == "") handleRoot();
  else if (path == "/get-config") handleGetConfig();
  else if (path == "/set-mode") handleSetMode();
  else if (path == "/set-power") handleSetPower();
  else if (path == "/set-slider") handleSetSlider();
  else if (path == "/save-config") handleSaveConfig();
  else if (path == "/reboot") handleReboot();
  else if (path == "/test-display") handleTestDisplay();
  else server.send(404, "text/plain", "Not Found");
}

inline void core0WebTask(void * pvParameters) {
  server.on("/", handleRoot);
  server.on("/get-config", handleGetConfig);
  server.on("/set-mode", handleSetMode);
  server.on("/set-power", handleSetPower);
  server.on("/set-slider", handleSetSlider);
  server.on("/save-config", handleSaveConfig);
  server.on("/test-display", handleTestDisplay);
  server.on("/reboot", handleReboot);
  server.onNotFound(handleNotFound);
  ElegantOTA.begin(&server);
  server.begin();
  webSocket.begin();

  unsigned long lastBrowserUpdateTimestamp = 0;

  while(true) {
    server.handleClient(); 
    webSocket.loop(); 
    ElegantOTA.loop();
    
    if (isTestModeActive) {
      StaticJsonDocument<128> doc; doc["isTest"] = true; doc["testColor"] = testColorHexWeb;
      String jsonString; serializeJson(doc, jsonString); webSocket.broadcastTXT(jsonString);
      vTaskDelay(pdMS_TO_TICKS(100));
    } 
    else if (millis() - lastBrowserUpdateTimestamp > 33) {
      lastBrowserUpdateTimestamp = millis();
      StaticJsonDocument<512> doc; 
      JsonArray bins = doc.createNestedArray("bins");
      
      portENTER_CRITICAL(&dataMutex);
      for(int i=0; i<16; i++) bins.add(sharedVisualizerBins[i]);
      doc["mode"] = config.visualizerMode; 
      doc["isTest"] = false;
      doc["pktCount"] = validPacketCount; 
      portEXIT_CRITICAL(&dataMutex);
      
      String jsonString; 
      serializeJson(doc, jsonString); 
      webSocket.broadcastTXT(jsonString);
    }
    vTaskDelay(pdMS_TO_TICKS(5)); 
  }
}
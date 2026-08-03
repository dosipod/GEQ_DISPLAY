#pragma once
#include <Arduino.h>

#define FIRMWARE_VERSION "v7.5.0-STABLE-BASE"

extern const char* ssid;
extern const char* password;

struct DevConfig {
  uint16_t udpPort = 11980;
  char multicastIP[16] = "239.0.0.1"; // FIXED: Restored strict 16-element character string buffer bounds
  uint8_t visualizerMode = 1; 
  uint8_t displayRotation = 3; 
  uint8_t audioFloor = 25; 
  uint8_t audioGain = 15; 
  uint8_t peakGravity = 3; 
  bool isDisplayOn = true; 
};
extern DevConfig config;

extern const char INDEX_HTML[] PROGMEM;



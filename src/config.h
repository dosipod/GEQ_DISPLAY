#pragma once
#include <Arduino.h>

#define FIRMWARE_VERSION "v5.5.0-MULTI-EFFECTS"

extern const char* ssid;
extern const char* password;

struct DevConfig {
  uint16_t udpPort = 11980;
  char multicastIP[16] = "239.0.0.1"; // 16-byte fixed text array container
  uint8_t visualizerMode = 1; 
  uint8_t displayRotation = 3;         // Landscape Left standard baseline
  uint8_t audioFloor = 25; 
  uint8_t audioGain = 15; 
};
extern DevConfig config;

extern const char INDEX_HTML[] PROGMEM;

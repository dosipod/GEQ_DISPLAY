#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define FIRMWARE_VERSION "2.4.0"

extern Preferences preferences;

struct DevConfig {
  uint16_t udpPort = 11980;
  char multicastIP[16] = "239.0.0.1";
  uint8_t audioFloor = 5;
  float audioGain = 1.0f;
  uint8_t peakGravity = 3;
  bool isDisplayOn = true;
  uint8_t visualizerMode = 0;
  uint8_t displayRotation = 3;
};

extern DevConfig config;

inline void loadSettingsFromFlash() {
  preferences.begin("geq_config", true);
  config.udpPort = preferences.getUInt("udpPort", 11980);
  String savedIP = preferences.getString("multicastIP", "239.0.0.1");
  strncpy(config.multicastIP, savedIP.c_str(), 15);
  config.multicastIP[15] = '\0';
  config.audioFloor = preferences.getUChar("audioFloor", 5);
  config.audioGain = preferences.getFloat("audioGain", 1.0f);
  config.peakGravity = preferences.getUChar("peakGravity", 3);
  config.isDisplayOn = preferences.getBool("isDisplayOn", true);
  config.visualizerMode = preferences.getUChar("visualizerMode", 0);
  config.displayRotation = preferences.getUChar("displayRotation", 3);
  preferences.end();
}

inline void saveSettingsToFlash() {
  preferences.begin("geq_config", false);
  preferences.putUInt("udpPort", config.udpPort);
  preferences.putString("multicastIP", String(config.multicastIP));
  preferences.putUChar("audioFloor", config.audioFloor);
  preferences.putFloat("audioGain", config.audioGain);
  preferences.putUChar("peakGravity", config.peakGravity);
  preferences.putBool("isDisplayOn", config.isDisplayOn);
  preferences.putUChar("visualizerMode", config.visualizerMode);
  preferences.putUChar("displayRotation", config.displayRotation);
  preferences.end();
}
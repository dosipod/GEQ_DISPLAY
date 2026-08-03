#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;
extern uint16_t peakHolds[];

// FIXED: Clean, encapsulated helper function handles all 16 rendering geometries and color shifts
inline void renderSpectrumEffects(uint8_t targetMode, int i, int xPos, int barW, int baseW, int baseH, int barH, int rawVal, uint8_t fallSpeed, uint8_t globalAverageVolume, uint8_t localBins[], uint8_t previousBins[]) {
  uint16_t color = RGB565_GREEN;
  
  if (targetMode == 1) color = RGB565_CYAN;
  else if (targetMode == 2) color = gfx->color565((i * 16), 255 - (i * 16), 128); 
  else if (targetMode == 3) color = gfx->color565(255, 255 - rawVal, 0);         
  else if (targetMode == 4) color = gfx->color565(rawVal, 0, 255);                 
  else if (targetMode == 5) color = (i < 8) ? RGB565_MAGENTA : RGB565_CYAN;
  else if (targetMode == 6) {
    if (rawVal < 100) color = RGB565_GREEN;
    else if (rawVal < 200) color = RGB565_YELLOW;
    else color = RGB565_RED;
  }
  else if (targetMode == 7) color = gfx->color565(0, rawVal, 255);
  else if (targetMode == 8) {
    float waveOffset = sin((i * 0.4f) + (millis() * 0.005f)) * 10.0f;
    barH = constrain(barH + (int)waveOffset, 0, baseH - 12);
    color = gfx->color565(0, 100 + (rawVal * 0.5), 255);
  }
  else if (targetMode == 10) {
    color = (i < 8) ? gfx->color565(255, 255 - rawVal, 0) : gfx->color565(0, rawVal, 255);
  }
  else if (targetMode == 11) {
    color = gfx->color565(200, 50, i * 16);
  }
  else if (targetMode == 13) {
    color = (rawVal > 150) ? RGB565_MAGENTA : gfx->color565(0, rawVal, i * 8);
  }
  else if (targetMode == 14) {
    color = (globalAverageVolume > 140) ? RGB565_BLUE : RGB565_GREEN;
  }
  else if (targetMode == 15) {
    color = gfx->color565(rawVal, 255 - rawVal, 255);
  }

  int x, y, w, h;
  if (targetMode == 1) { 
    int midY = baseH / 2; int halfH = barH / 2;
    x = xPos; y = midY - halfH; w = barW; h = halfH * 2;
    gfx->writeFillRect(x, y, w, h, color); gfx->writeFillRect(x, 0, w, y, RGB565_BLACK);
    gfx->writeFillRect(x, midY + halfH, w, baseH - (midY + halfH), RGB565_BLACK);
  } else if (targetMode == 3) { 
    x = xPos; y = 0; w = barW; h = barH;
    gfx->writeFillRect(x, y, w, h, color); gfx->writeFillRect(x, h, w, baseH - h, RGB565_BLACK);
  } else if (targetMode == 5) {
    int quarterH = barH / 2;
    gfx->writeFillRect(xPos, 0, barW, quarterH, color); 
    gfx->writeFillRect(xPos, baseH - quarterH, barW, quarterH, color); 
    gfx->writeFillRect(xPos, quarterH, barW, baseH - (quarterH * 2), RGB565_BLACK); 
  } else if (targetMode == 7) {
    x = xPos; y = baseH - barH; w = barW;
    if (barH > 3) {
      gfx->writeFillRect(x, y, w, 3, RGB565_WHITE); 
      gfx->writeFillRect(x, y + 3, w, barH - 3, color); 
      gfx->writeFillRect(x, 0, w, y, RGB565_BLACK);
    } else {
      gfx->writeFillRect(x, 0, w, baseH, RGB565_BLACK);
    }
  } 
  else if (targetMode == 11) {
    int maxBarW = baseW / 2;
    int dynamicW = map(rawVal, 0, 255, 0, maxBarW);
    int barY = gap + (i * ((baseH - (gap * (16 + 1))) / 16 + gap));
    int barHeightY = (baseH - (gap * (16 + 1))) / 16;
    
    if (i < 8) {
      gfx->writeFillRect(0, barY, dynamicW, barHeightY, color);
      gfx->writeFillRect(dynamicW, barY, maxBarW - dynamicW, barHeightY, RGB565_BLACK);
    } else {
      gfx->writeFillRect(baseW - dynamicW, barY, dynamicW, barHeightY, color);
      gfx->writeFillRect(maxBarW, barY, maxBarW - dynamicW, barHeightY, RGB565_BLACK);
    }
  }
  else if (targetMode == 12) {
    gfx->writeFillRect(xPos, 0, barW, baseH - peakHolds[i], RGB565_BLACK);
    if (peakHolds[i] > 0) {
      uint16_t strobeColor = gfx->color565(rawVal, 255, 255);
      gfx->writeFillRect(xPos, baseH - peakHolds[i], barW, 3, strobeColor);
      gfx->writeFillRect(xPos, baseH - peakHolds[i] + 3, barW, peakHolds[i] - 3, RGB565_BLACK);
    }
  }
  else if (targetMode == 14) {
    if (globalAverageVolume > 140) {
      gfx->writeFillRect(xPos, 0, barW, baseH - barH, RGB565_WHITE); 
      gfx->writeFillRect(xPos, baseH - barH, barW, barH, color);
    } else {
      gfx->writeFillRect(xPos, baseH - barH, barW, barH, color);
      gfx->writeFillRect(xPos, 0, barW, baseH - barH, RGB565_BLACK);
    }
  }
  else if (targetMode == 15) {
    gfx->writeFillRect(xPos, 0, barW, baseH - peakHolds[i], RGB565_BLACK);
    if (peakHolds[i] > 0) {
      int snakeLen = (peakHolds[i] > 12) ? 12 : peakHolds[i];
      gfx->writeFillRect(xPos, baseH - peakHolds[i], barW, snakeLen, color); 
      gfx->writeFillRect(xPos, baseH - peakHolds[i] + snakeLen, barW, peakHolds[i] - snakeLen, RGB565_BLACK);
    }
  }
  else { 
    x = xPos; y = baseH - barH; w = barW; h = barH;
    gfx->writeFillRect(x, y, w, h, color); gfx->writeFillRect(x, 0, w, y, RGB565_BLACK);
    if (peakHolds[i] > 0) gfx->writeFillRect(x, baseH - peakHolds[i], w, 2, RGB565_RED); 
  }
}

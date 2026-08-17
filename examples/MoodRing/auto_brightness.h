// auto_brightness.h - Content-aware brightness compression for LED strips
#pragma once

#include "FastLED.h"

// Average brightness of an LED array as a percentage (0-100).
float getAverageBrightness(CRGB *leds, int numLeds);

// Map content brightness to an output brightness byte using a three-segment
// compression curve:
//   below lowThreshold  -> full brightness (255)
//   lowThreshold..high  -> linear ramp down to maxBrightness
//   above highThreshold -> maxBrightness
uint8_t applyBrightnessCompression(float inputBrightnessPercent,
                                   uint8_t maxBrightness, float lowThreshold,
                                   float highThreshold);

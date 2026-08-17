// auto_brightness.h - Content-aware brightness compression for LED strips
//
// NOTE: a twin of this file lives in examples/AnimartrixRing/auto_brightness.h.
// Examples cannot include across directories, so the file is duplicated.
// Fix both, or neither.
#pragma once

#include "FastLED.h"
#include "fl/stl/span.h"

// Average brightness of an LED strip as a percentage (0-100).
float getAverageBrightness(fl::span<const CRGB> leds);

// Map content brightness to an output brightness byte using a three-segment
// compression curve:
//   below lowThreshold  -> full brightness (255)
//   lowThreshold..high  -> linear ramp down to maxBrightness
//   above highThreshold -> maxBrightness
uint8_t applyBrightnessCompression(float inputBrightnessPercent,
                                   uint8_t maxBrightness, float lowThreshold,
                                   float highThreshold);

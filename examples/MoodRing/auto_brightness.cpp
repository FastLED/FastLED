// auto_brightness.cpp
//
// NOTE: a twin of this file lives in examples/AnimartrixRing/auto_brightness.cpp.
// Examples cannot include across directories, so the file is duplicated.
// Fix both, or neither.
#include "auto_brightness.h"

float getAverageBrightness(fl::span<const CRGB> leds) {
    if (leds.empty()) {
        return 0.0f;
    }
    uint32_t total = 0;
    for (const CRGB &led : leds) {
        total += led.r + led.g + led.b;
    }
    float avgValue = float(total) / float(leds.size() * 3);
    return (avgValue / 255.0f) * 100.0f;
}

uint8_t applyBrightnessCompression(float inputBrightnessPercent,
                                   uint8_t maxBrightness, float lowThreshold,
                                   float highThreshold) {
    float maxBrightnessPercent = (maxBrightness / 255.0f) * 100.0f;
    if (inputBrightnessPercent < lowThreshold) {
        return 255;
    } else if (inputBrightnessPercent < highThreshold) {
        float range = highThreshold - lowThreshold;
        float progress = (inputBrightnessPercent - lowThreshold) / range;
        float targetPercent =
            100.0f - (progress * (100.0f - maxBrightnessPercent));
        return static_cast<uint8_t>((targetPercent / 100.0f) * 255.0f);
    } else {
        return maxBrightness;
    }
}

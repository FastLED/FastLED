// auto_brightness.cpp
#include "auto_brightness.h"

float getAverageBrightness(CRGB *leds, int numLeds) {
    uint32_t total = 0;
    for (int i = 0; i < numLeds; i++) {
        total += leds[i].r + leds[i].g + leds[i].b;
    }
    float avgValue = float(total) / float(numLeds * 3);
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

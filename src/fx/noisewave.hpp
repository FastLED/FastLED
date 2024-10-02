#pragma once

#include "FastLED.h"

struct NoiseGenerator {
    int32_t iteration_scale;
    unsigned long time_multiplier;

    NoiseGenerator() : iteration_scale(10), time_multiplier(10) {}
    NoiseGenerator(int32_t itScale, int32_t timeMul) : iteration_scale(itScale), time_multiplier(timeMul) {}

    uint8_t Value(int32_t i, unsigned long time_ms) const {
        uint32_t input = iteration_scale * i + time_ms * time_multiplier;
        uint16_t v1 = inoise16(input);
        return uint8_t(v1 >> 8);
    }

    int LedValue(int32_t i, unsigned long time_ms) const {
        int val = Value(i, time_ms);
        return max(0, val - 128) * 2;
    }
};

struct NoiseWaveData {
    CRGB* leds = nullptr;
    uint16_t num_leds = 0;
    NoiseGenerator noiseGeneratorRed;
    NoiseGenerator noiseGeneratorBlue;
    unsigned long start_time = 0;

    NoiseWaveData(CRGB* leds, uint16_t num_leds)
        : leds(leds), num_leds(num_leds), 
          noiseGeneratorRed(500, 14), noiseGeneratorBlue(500, 10) {
        start_time = millis();
    }
};

void NoiseWaveLoop(NoiseWaveData& self) {
    if (self.leds == nullptr || self.num_leds == 0) {
        return;
    }

    unsigned long time_now = millis() - self.start_time;

    for (int32_t i = 0; i < self.num_leds; ++i) {
        int r = self.noiseGeneratorRed.LedValue(i, time_now);
        int b = self.noiseGeneratorBlue.LedValue(i, time_now + 100000) >> 1;
        int g = 0;
        self.leds[i] = CRGB(r, g, b);
    }
}


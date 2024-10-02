#pragma once

#include "FastLED.h"
#include "noisegen.h"


#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

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

FASTLED_NAMESPACE_END

#pragma once

#include "FastLED.h"
#include "noisegen.h"
#include "fx/fx1d.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class NoiseWave : public FxStrip {
public:
    NoiseWave(uint16_t num_leds) 
        : FxStrip(num_leds), leds(leds),
          noiseGeneratorRed(500, 14), noiseGeneratorBlue(500, 10) {}

    void lazyInit() override {
        start_time = millis();
    }

    void draw(uint32_t now, CRGB* leds) override {
        if (leds == nullptr || mNumLeds == 0) {
            return;
        }

        unsigned long time_now = millis() - start_time;

        for (int32_t i = 0; i < mNumLeds; ++i) {
            int r = noiseGeneratorRed.LedValue(i, time_now);
            int b = noiseGeneratorBlue.LedValue(i, time_now + 100000) >> 1;
            int g = 0;
            leds[i] = CRGB(r, g, b);
        }
    }

    const char* fxName() const override { return "NoiseWave"; }

private:
    CRGB* leds;
    NoiseGenerator noiseGeneratorRed;
    NoiseGenerator noiseGeneratorBlue;
    unsigned long start_time = 0;
};

FASTLED_NAMESPACE_END

#pragma once

#include "FastLED.h"
#include "fx/fx1d.h"
#include "fl/namespace.h"
#include "noisegen.h"

namespace fl {

FASTLED_SMART_PTR(NoiseWave);

class NoiseWave : public Fx1d {
  public:
    NoiseWave(uint16_t num_leds)
        : Fx1d(num_leds), noiseGeneratorRed(500, 14),
          noiseGeneratorBlue(500, 10) {}

    void draw(DrawContext context) override {
        if (context.leds == nullptr || mNumLeds == 0) {
            return;
        }
        if (start_time == 0) {
            start_time = context.now;
        }

        unsigned long time_now = millis() - start_time;

        for (int32_t i = 0; i < mNumLeds; ++i) {
            int r = noiseGeneratorRed.LedValue(i, time_now);
            int b = noiseGeneratorBlue.LedValue(i, time_now + 100000) >> 1;
            int g = 0;
            context.leds[i] = CRGB(r, g, b);
        }
    }

    fl::Str fxName() const override { return "NoiseWave"; }

  private:
    NoiseGenerator noiseGeneratorRed;
    NoiseGenerator noiseGeneratorBlue;
    uint32_t start_time = 0;
};

}  // namespace fl

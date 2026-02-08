#pragma once

#include "fl/fastled.h"
#include "fl/fx/fx1d.h"
#include "noisegen.h"

namespace fl {

FASTLED_SHARED_PTR(NoiseWave);

class NoiseWave : public Fx1d {
  public:
    NoiseWave(u16 num_leds)
        : Fx1d(num_leds), noiseGeneratorRed(500, 14),
          noiseGeneratorBlue(500, 10) {}

    void draw(DrawContext context) override {
        if (context.leds == nullptr || mNumLeds == 0) {
            return;
        }
        if (start_time == 0) {
            start_time = context.now;
        }

        unsigned long time_now = fl::millis() - start_time;

        for (i32 i = 0; i < mNumLeds; ++i) {
            int r = noiseGeneratorRed.LedValue(i, time_now);
            int b = noiseGeneratorBlue.LedValue(i, time_now + 100000) >> 1;
            int g = 0;
            context.leds[i] = CRGB(r, g, b);
        }
    }

    fl::string fxName() const override { return "NoiseWave"; }

  private:
    NoiseGenerator noiseGeneratorRed;
    NoiseGenerator noiseGeneratorBlue;
    fl::u32 start_time = 0;
};

} // namespace fl

#pragma once

#include "FastLED.h"
#include "fx/fx1d.h"
#include "fl/namespace.h"

namespace fl {

FASTLED_SMART_PTR(Cylon);

/// @brief   An animation that moves a single LED back and forth (Larson Scanner
/// effect)
class Cylon : public Fx1d {
  public:
    uint8_t delay_ms;
    Cylon(uint16_t num_leds, uint8_t fade_amount = 250, uint8_t delay_ms = 10)
        : Fx1d(num_leds), delay_ms(delay_ms), fade_amount(fade_amount) {}


    void draw(DrawContext context) override {
        if (context.leds == nullptr || mNumLeds == 0) {
            return;
        }

        CRGB *leds = context.leds;

        // Set the current LED to the current hue
        leds[position] = CHSV(hue++, 255, 255);

        // Fade all LEDs
        for (uint16_t i = 0; i < mNumLeds; i++) {
            leds[i].nscale8(fade_amount);
        }

        // Move the position
        if (reverse) {
            position--;
            if (position < 0) {
                position = 1;
                reverse = false;
            }
        } else {
            position++;
            if (position >= int16_t(mNumLeds)) {
                position = mNumLeds - 2;
                reverse = true;
            }
        }
    }

    fl::Str fxName() const override { return "Cylon"; }

  private:
    uint8_t hue = 0;

    uint8_t fade_amount;

    bool reverse = false;
    int16_t position = 0;
};

}  // namespace fl

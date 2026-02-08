#pragma once

#include "fl/fastled.h"
#include "fl/fx/fx1d.h"

namespace fl {

FASTLED_SHARED_PTR(Cylon);

/// @brief   An animation that moves a single LED back and forth (Larson Scanner
/// effect)
class Cylon : public Fx1d {
  public:
    u8 delay_ms;
    Cylon(u16 num_leds, u8 fade_amount = 250, u8 delay_ms = 10)
        : Fx1d(num_leds), delay_ms(delay_ms), fade_amount(fade_amount) {}

    void draw(DrawContext context) override {
        if (context.leds == nullptr || mNumLeds == 0) {
            return;
        }

        CRGB *leds = context.leds;

        // Set the current LED to the current hue
        leds[position] = CHSV(hue++, 255, 255);

        // Fade all LEDs
        for (u16 i = 0; i < mNumLeds; i++) {
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
            if (position >= i16(mNumLeds)) {
                position = mNumLeds - 2;
                reverse = true;
            }
        }
    }

    fl::string fxName() const override { return "Cylon"; }

  private:
    u8 hue = 0;

    u8 fade_amount;

    bool reverse = false;
    i16 position = 0;
};

} // namespace fl

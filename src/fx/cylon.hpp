#pragma once

#include "namespace.h"
#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

/// @brief   An animation that moves a single LED back and forth (Larson Scanner effect)

struct CylonData {
    CRGB* leds = nullptr;
    uint16_t num_leds = 0;
    uint8_t hue = 0;
    uint8_t fade_amount = 250;
    uint8_t delay_ms = 10;
    bool reverse = false;
    int16_t position = 0;

    // constructor
    CylonData(CRGB* leds, uint16_t num_leds, uint8_t fade_amount = 250, uint8_t delay_ms = 10)
        : leds(leds), num_leds(num_leds), fade_amount(fade_amount), delay_ms(delay_ms) {}
};


void CylonLoop(CylonData& self) {
    if (self.leds == nullptr || self.num_leds == 0) {
        return;
    }

    // Set the current LED to the current hue
    self.leds[self.position] = CHSV(self.hue++, 255, 255);



    // Fade all LEDs
    for(uint16_t i = 0; i < self.num_leds; i++) {
        self.leds[i].nscale8(self.fade_amount);
    }

    // Move the position
    if (self.reverse) {
        self.position--;
        if (self.position < 0) {
            self.position = 1;
            self.reverse = false;
        }
    } else {
        self.position++;
        if (self.position >= int16_t(self.num_leds)) {
            self.position = self.num_leds - 2;
            self.reverse = true;
        }
    }
}

FASTLED_NAMESPACE_END

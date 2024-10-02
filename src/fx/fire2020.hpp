/// @brief   Simple one-dimensional fire animation function

#ifndef FIRE2020_HPP
#define FIRE2020_HPP

#include "crgb.h"
#include "FastLED.h"

struct Fire2020Data {
    CRGB* leds = nullptr;
    uint16_t num_leds = 0;
    uint8_t* heat = nullptr; // Will be allocated if not provided.
    uint8_t cooling = 55;
    uint8_t sparking = 120;
    bool reverse_direction = false;
};

void Fire2020Loop(Fire2020Data& self)
{

    if (self.leds == nullptr) {
        return;
    }
    // We allow head to be auto created.
    if (self.heat == nullptr && self.num_leds > 0) {
        self.heat = new uint8_t[self.num_leds]();  // Initialize to zero
    }

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < self.num_leds; i++) {
        self.heat[i] = qsub8(self.heat[i], random8(0, ((self.cooling * 10) / self.num_leds) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = self.num_leds - 1; k >= 2; k--) {
        self.heat[k] = (self.heat[k - 1] + self.heat[k - 2] + self.heat[k - 2]) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < self.sparking) {
        int y = random8(7);
        self.heat[y] = qadd8(self.heat[y], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (int j = 0; j < self.num_leds; j++) {
        CRGB color = HeatColor(self.heat[j]);
        int pixelnumber;
        if (self.reverse_direction) {
            pixelnumber = (self.num_leds - 1) - j;
        } else {
            pixelnumber = j;
        }
        self.leds[pixelnumber] = color;
    }
}

#endif // FIRE2020_HPP


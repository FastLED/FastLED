/// @brief   Simple one-dimensional fire animation function

#ifndef FIRE2020_HPP
#define FIRE2020_HPP

#include "crgb.h"
#include "FastLED.h"

struct Fire2020Data {
    CRGB* leds;
    uint16_t num_leds;
    uint8_t cooling;
    uint8_t sparking;
    bool reverse_direction;
};

void Fire2020(Fire2020Data& config)
{
    // Array of temperature readings at each simulation cell
    static uint8_t* heat = nullptr;
    static uint16_t last_num_leds = 0;

    // Reallocate heat array if number of LEDs has changed
    if (config.num_leds != last_num_leds) {
        delete[] heat;
        heat = new uint8_t[config.num_leds];
        last_num_leds = config.num_leds;
    }

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < config.num_leds; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((config.cooling * 10) / config.num_leds) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = config.num_leds - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < config.sparking) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (int j = 0; j < config.num_leds; j++) {
        CRGB color = HeatColor(heat[j]);
        int pixelnumber;
        if (config.reverse_direction) {
            pixelnumber = (config.num_leds - 1) - j;
        } else {
            pixelnumber = j;
        }
        config.leds[pixelnumber] = color;
    }
}

#endif // FIRE2020_HPP


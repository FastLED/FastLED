#pragma once

/// @brief   Simple one-dimensional fire animation function
// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//// 
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation, 
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking. 
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100 

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.




#include "crgb.h"
#include "FastLED.h"


struct Fire2012Data {
    CRGB* leds = nullptr;
    uint16_t num_leds = 0;
    uint8_t* heat = nullptr; // Will be allocated if not provided.
    uint8_t cooling = 55;
    uint8_t sparking = 120;
    bool reverse_direction = false;
    CRGBPalette16 palette = HeatColors_p; // Default palette
    // constructor
    Fire2012Data(CRGB* leds, uint16_t num_leds, uint8_t* heat, uint8_t cooling = 55, uint8_t sparking = 120, bool reverse_direction = false, const CRGBPalette16& palette = HeatColors_p)
        : leds(leds), num_leds(num_leds), heat(heat), cooling(cooling), sparking(sparking), reverse_direction(reverse_direction), palette(palette) {}
};

void Fire2012Loop(Fire2012Data& self)
{

    if (self.leds == nullptr) {
        return;
    }
    // We allow head to be auto created.
    if (self.heat == nullptr && self.num_leds > 0) {
        self.heat = new uint8_t[self.num_leds]();  // Initialize to zero
    }

    // Step 1.  Cool down every cell a little
    for (uint16_t i = 0; i < self.num_leds; i++) {
        self.heat[i] = qsub8(self.heat[i], random8(0, ((self.cooling * 10) / self.num_leds) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (uint16_t k = self.num_leds - 1; k >= 2; k--) {
        self.heat[k] = (self.heat[k - 1] + self.heat[k - 2] + self.heat[k - 2]) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < self.sparking) {
        int y = random8(7);
        self.heat[y] = qadd8(self.heat[y], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (uint16_t j = 0; j < self.num_leds; j++) {
        // Scale the heat value from 0-255 down to 0-240
        // for best results with color palettes.
        uint8_t colorindex = scale8(self.heat[j], 240);
        CRGB color = ColorFromPalette(self.palette, colorindex);
        int pixelnumber;
        if (self.reverse_direction) {
            pixelnumber = (self.num_leds - 1) - j;
        } else {
            pixelnumber = j;
        }
        self.leds[pixelnumber] = color;
    }
}

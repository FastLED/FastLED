#pragma once

#include "FastLED.h"
#include "fx/fx1d.h"
#include "fl/namespace.h"

namespace fl {


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

FASTLED_SMART_PTR(Fire2012);

class Fire2012 : public Fx1d {
  public:
    Fire2012(uint16_t num_leds, uint8_t cooling = 55, uint8_t sparking = 120,
             bool reverse_direction = false,
             const CRGBPalette16 &palette = (const CRGBPalette16&)HeatColors_p)
        : Fx1d(num_leds), cooling(cooling), sparking(sparking),
          reverse_direction(reverse_direction), palette(palette) {
        heat.reset(new uint8_t[num_leds]()); // Initialize to zero
    }

    ~Fire2012() {}

    void draw(DrawContext context) override {
        CRGB *leds = context.leds;
        if (leds == nullptr) {
            return;
        }

        // Step 1.  Cool down every cell a little
        for (uint16_t i = 0; i < mNumLeds; i++) {
            heat[i] =
                qsub8(heat[i], random8(0, ((cooling * 10) / mNumLeds) + 2));
        }

        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (uint16_t k = mNumLeds - 1; k >= 2; k--) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }

        // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
        if (random8() < sparking) {
            int y = random8(7);
            heat[y] = qadd8(heat[y], random8(160, 255));
        }

        // Step 4.  Map from heat cells to LED colors
        for (uint16_t j = 0; j < mNumLeds; j++) {
            // Scale the heat value from 0-255 down to 0-240
            // for best results with color palettes.
            uint8_t colorindex = scale8(heat[j], 240);
            CRGB color = ColorFromPalette(palette, colorindex);
            int pixelnumber;
            if (reverse_direction) {
                pixelnumber = (mNumLeds - 1) - j;
            } else {
                pixelnumber = j;
            }
            leds[pixelnumber] = color;
        }
    }

    fl::Str fxName() const override { return "Fire2012"; }

  private:
    fl::scoped_array<uint8_t> heat;
    uint8_t cooling;
    uint8_t sparking;
    bool reverse_direction;
    CRGBPalette16 palette;
};

}  // namespace fl

#ifndef __INC_COLORUTILS_H
#define __INC_COLORUTILS_H

#include "pixeltypes.h"


// fill_solid -   fill a range of LEDs with a solid color
void fill_solid( struct CRGB * pFirstLED, int numToFill,
                 const struct CRGB& color);

// fill_rainbow - fill a range of LEDs with a rainbow of colors, at
//                full saturation and full value (brightness)
void fill_rainbow( struct CRGB * pFirstLED, int numToFill,
                   uint8_t initialhue,
                   uint8_t deltahue = 5);


// CRGB HeatColor( uint8_t temperature)
//
// Approximates a 'black body radiation' spectrum for
// a given 'heat' level.  This is useful for animations of 'fire'.
// Heat is specified as an arbitrary scale from 0 (cool) to 255 (hot).
// This is NOT a chromatically correct 'black body radiation'
// spectrum, but it's surprisingly close, and it's fast and small.
CRGB HeatColor( uint8_t temperature);

#endif

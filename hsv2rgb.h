#ifndef __INC_HSV2RGB_H
#define __INC_HSV2RGB_H

#include "pixeltypes.h"


// hsv2rgb - convert hue, saturation, and value to RGB.
//
//           NOTE: Hue is 0-191 only!
//                 Saturation & value are 0-255 each.
//

#if defined(__AVR__)
#define hsv2rgb hsv2rgb_avr
#else
#define hsvrgb  hsv2rgb_C
#endif

void hsv2rgb_C (struct CHSV & hsv, struct CRGB & rgb);
void hsv2rgb_avr(struct CHSV & hsv, struct CRGB & rgb);


// rainbow2rgb - convert a hue, saturation, and value to RGB
//               but use a visually balanced rainbow instead of
//               a mathematically balanced spectrum.
//
//               NOTE: here hue is 0-255, not just 0-191!
//

void rainbow2rgb( CHSV& hsv, CRGB& rgb);


#endif

#ifndef __INC_HSV2RGB_H
#define __INC_HSV2RGB_H

#include "pixeltypes.h"


void hsv2rgb_C (struct CHSV & hsv, struct CRGB & rgb);
void hsv2rgb_avr(struct CHSV & hsv, struct CRGB & rgb);

#if defined(__AVR__)
#define hsv2rgb hsv2rgb_avr
#else
#define hsvrgb  hsv2rgb_C
#endif


#endif

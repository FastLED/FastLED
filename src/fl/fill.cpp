

#include <stdint.h>

#include "fill.h"

namespace fl {

void fill_solid(struct CRGB *targetArray, int numToFill,
                const struct CRGB &color) {
    for (int i = 0; i < numToFill; ++i) {
        targetArray[i] = color;
    }
}

void fill_solid(struct CHSV *targetArray, int numToFill,
                const struct CHSV &color) {
    for (int i = 0; i < numToFill; ++i) {
        targetArray[i] = color;
    }
}

// void fill_solid( struct CRGB* targetArray, int numToFill,
// 				 const struct CHSV& hsvColor)
// {
// 	fill_solid<CRGB>( targetArray, numToFill, (CRGB) hsvColor);
// }

void fill_rainbow(struct CRGB *targetArray, int numToFill, uint8_t initialhue,
                  uint8_t deltahue) {
    CHSV hsv;
    hsv.hue = initialhue;
    hsv.val = 255;
    hsv.sat = 240;
    for (int i = 0; i < numToFill; ++i) {
        targetArray[i] = hsv;
        hsv.hue += deltahue;
    }
}

void fill_rainbow(struct CHSV *targetArray, int numToFill, uint8_t initialhue,
                  uint8_t deltahue) {
    CHSV hsv;
    hsv.hue = initialhue;
    hsv.val = 255;
    hsv.sat = 240;
    for (int i = 0; i < numToFill; ++i) {
        targetArray[i] = hsv;
        hsv.hue += deltahue;
    }
}

void fill_rainbow_circular(struct CRGB *targetArray, int numToFill,
                           uint8_t initialhue, bool reversed) {
    if (numToFill == 0)
        return; // avoiding div/0

    CHSV hsv;
    hsv.hue = initialhue;
    hsv.val = 255;
    hsv.sat = 240;

    const uint16_t hueChange =
        65535 / (uint16_t)numToFill; // hue change for each LED, * 256 for
                                     // precision (256 * 256 - 1)
    uint16_t hueOffset = 0; // offset for hue value, with precision (*256)

    for (int i = 0; i < numToFill; ++i) {
        targetArray[i] = hsv;
        if (reversed)
            hueOffset -= hueChange;
        else
            hueOffset += hueChange;
        hsv.hue = initialhue +
                  (uint8_t)(hueOffset >>
                            8); // assign new hue with precise offset (as 8-bit)
    }
}

void fill_rainbow_circular(struct CHSV *targetArray, int numToFill,
                           uint8_t initialhue, bool reversed) {
    if (numToFill == 0)
        return; // avoiding div/0

    CHSV hsv;
    hsv.hue = initialhue;
    hsv.val = 255;
    hsv.sat = 240;

    const uint16_t hueChange =
        65535 / (uint16_t)numToFill; // hue change for each LED, * 256 for
                                     // precision (256 * 256 - 1)
    uint16_t hueOffset = 0; // offset for hue value, with precision (*256)

    for (int i = 0; i < numToFill; ++i) {
        targetArray[i] = hsv;
        if (reversed)
            hueOffset -= hueChange;
        else
            hueOffset += hueChange;
        hsv.hue = initialhue +
                  (uint8_t)(hueOffset >>
                            8); // assign new hue with precise offset (as 8-bit)
    }
}

void fill_gradient_RGB(CRGB *leds, uint16_t startpos, CRGB startcolor,
                       uint16_t endpos, CRGB endcolor) {
    // if the points are in the wrong order, straighten them
    if (endpos < startpos) {
        uint16_t t = endpos;
        CRGB tc = endcolor;
        endcolor = startcolor;
        endpos = startpos;
        startpos = t;
        startcolor = tc;
    }

    saccum87 rdistance87;
    saccum87 gdistance87;
    saccum87 bdistance87;

    rdistance87 = (endcolor.r - startcolor.r) << 7;
    gdistance87 = (endcolor.g - startcolor.g) << 7;
    bdistance87 = (endcolor.b - startcolor.b) << 7;

    uint16_t pixeldistance = endpos - startpos;
    int16_t divisor = pixeldistance ? pixeldistance : 1;

    saccum87 rdelta87 = rdistance87 / divisor;
    saccum87 gdelta87 = gdistance87 / divisor;
    saccum87 bdelta87 = bdistance87 / divisor;

    rdelta87 *= 2;
    gdelta87 *= 2;
    bdelta87 *= 2;

    accum88 r88 = startcolor.r << 8;
    accum88 g88 = startcolor.g << 8;
    accum88 b88 = startcolor.b << 8;
    for (uint16_t i = startpos; i <= endpos; ++i) {
        leds[i] = CRGB(r88 >> 8, g88 >> 8, b88 >> 8);
        r88 += rdelta87;
        g88 += gdelta87;
        b88 += bdelta87;
    }
}

void fill_gradient_RGB(CRGB *leds, uint16_t numLeds, const CRGB &c1,
                       const CRGB &c2) {
    uint16_t last = numLeds - 1;
    fill_gradient_RGB(leds, 0, c1, last, c2);
}

void fill_gradient_RGB(CRGB *leds, uint16_t numLeds, const CRGB &c1,
                       const CRGB &c2, const CRGB &c3) {
    uint16_t half = (numLeds / 2);
    uint16_t last = numLeds - 1;
    fill_gradient_RGB(leds, 0, c1, half, c2);
    fill_gradient_RGB(leds, half, c2, last, c3);
}

void fill_gradient_RGB(CRGB *leds, uint16_t numLeds, const CRGB &c1,
                       const CRGB &c2, const CRGB &c3, const CRGB &c4) {
    uint16_t onethird = (numLeds / 3);
    uint16_t twothirds = ((numLeds * 2) / 3);
    uint16_t last = numLeds - 1;
    fill_gradient_RGB(leds, 0, c1, onethird, c2);
    fill_gradient_RGB(leds, onethird, c2, twothirds, c3);
    fill_gradient_RGB(leds, twothirds, c3, last, c4);
}

} // namespace fl
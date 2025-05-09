

#include <stdint.h>

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "crgb.h"
#include "fl/blur.h"
#include "fl/colorutils_misc.h"
#include "fl/deprecated.h"
#include "fl/unused.h"
#include "fl/xymap.h"
#include "lib8tion/scale8.h"

namespace fl {

// Legacy XY function. This is a weak symbol that can be overridden by the user.
uint16_t XY(uint8_t x, uint8_t y) __attribute__((weak));

__attribute__((weak)) uint16_t XY(uint8_t x, uint8_t y) {
    FASTLED_UNUSED(x);
    FASTLED_UNUSED(y);
    FASTLED_ASSERT(false, "the user didn't provide an XY function");
    return 0;
}

// uint16_t XY(uint8_t x, uint8_t y) {
//   return 0;
// }
// make this a weak symbol
namespace {
uint16_t xy_legacy_wrapper(uint16_t x, uint16_t y, uint16_t width,
                           uint16_t height) {
    FASTLED_UNUSED(width);
    FASTLED_UNUSED(height);
    return XY(x, y);
}
} // namespace

// blur1d: one-dimensional blur filter. Spreads light to 2 line neighbors.
// blur2d: two-dimensional blur filter. Spreads light to 8 XY neighbors.
//
//           0 = no spread at all
//          64 = moderate spreading
//         172 = maximum smooth, even spreading
//
//         173..255 = wider spreading, but increasing flicker
//
//         Total light is NOT entirely conserved, so many repeated
//         calls to 'blur' will also result in the light fading,
//         eventually all the way to black; this is by design so that
//         it can be used to (slowly) clear the LEDs to black.
void blur1d(CRGB *leds, uint16_t numLeds, fract8 blur_amount) {
    uint8_t keep = 255 - blur_amount;
    uint8_t seep = blur_amount >> 1;
    CRGB carryover = CRGB::Black;
    for (uint16_t i = 0; i < numLeds; ++i) {
        CRGB cur = leds[i];
        CRGB part = cur;
        part.nscale8(seep);
        cur.nscale8(keep);
        cur += carryover;
        if (i)
            leds[i - 1] += part;
        leds[i] = cur;
        carryover = part;
    }
}

void blur2d(CRGB *leds, uint8_t width, uint8_t height, fract8 blur_amount,
            const XYMap &xymap) {
    blurRows(leds, width, height, blur_amount, xymap);
    blurColumns(leds, width, height, blur_amount, xymap);
}

void blur2d(CRGB *leds, uint8_t width, uint8_t height, fract8 blur_amount) {
    XYMap xy =
        XYMap::constructWithUserFunction(width, height, xy_legacy_wrapper);
    blur2d(leds, width, height, blur_amount, xy);
}

void blurRows(CRGB *leds, uint8_t width, uint8_t height, fract8 blur_amount,
              const XYMap &xyMap) {

    /*    for( uint8_t row = 0; row < height; row++) {
            CRGB* rowbase = leds + (row * width);
            blur1d( rowbase, width, blur_amount);
        }
    */
    // blur rows same as columns, for irregular matrix
    uint8_t keep = 255 - blur_amount;
    uint8_t seep = blur_amount >> 1;
    for (uint8_t row = 0; row < height; row++) {
        CRGB carryover = CRGB::Black;
        for (uint8_t i = 0; i < width; i++) {
            CRGB cur = leds[xyMap.mapToIndex(i, row)];
            CRGB part = cur;
            part.nscale8(seep);
            cur.nscale8(keep);
            cur += carryover;
            if (i)
                leds[xyMap.mapToIndex(i - 1, row)] += part;
            leds[xyMap.mapToIndex(i, row)] = cur;
            carryover = part;
        }
    }
}

// blurColumns: perform a blur1d on each column of a rectangular matrix
void blurColumns(CRGB *leds, uint8_t width, uint8_t height, fract8 blur_amount,
                 const XYMap &xyMap) {
    // blur columns
    uint8_t keep = 255 - blur_amount;
    uint8_t seep = blur_amount >> 1;
    for (uint8_t col = 0; col < width; ++col) {
        CRGB carryover = CRGB::Black;
        for (uint8_t i = 0; i < height; ++i) {
            CRGB cur = leds[xyMap.mapToIndex(col, i)];
            CRGB part = cur;
            part.nscale8(seep);
            cur.nscale8(keep);
            cur += carryover;
            if (i)
                leds[xyMap.mapToIndex(col, i - 1)] += part;
            leds[xyMap.mapToIndex(col, i)] = cur;
            carryover = part;
        }
    }
}

} // namespace fl
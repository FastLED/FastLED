

#include "fl/stdint.h"

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "crgb.h"
#include "fl/blur.h"
#include "fl/colorutils_misc.h"
#include "fl/compiler_control.h"
#include "fl/deprecated.h"
#include "fl/unused.h"
#include "fl/xymap.h"
#include "lib8tion/scale8.h"
#include "fl/int.h"

namespace fl {

// Legacy XY function. This is a weak symbol that can be overridden by the user.
fl::u16 XY(fl::u8 x, fl::u8 y) FL_LINK_WEAK;

FL_LINK_WEAK fl::u16 XY(fl::u8 x, fl::u8 y) {
    FASTLED_UNUSED(x);
    FASTLED_UNUSED(y);
    FASTLED_ASSERT(false, "the user didn't provide an XY function");
    return 0;
}

// fl::u16 XY(fl::u8 x, fl::u8 y) {
//   return 0;
// }
// make this a weak symbol
namespace {
fl::u16 xy_legacy_wrapper(fl::u16 x, fl::u16 y, fl::u16 width,
                           fl::u16 height) {
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
void blur1d(CRGB *leds, fl::u16 numLeds, fract8 blur_amount) {
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    CRGB carryover = CRGB::Black;
    for (fl::u16 i = 0; i < numLeds; ++i) {
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

void blur2d(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount,
            const XYMap &xymap) {
    blurRows(leds, width, height, blur_amount, xymap);
    blurColumns(leds, width, height, blur_amount, xymap);
}

void blur2d(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount) {
    XYMap xy =
        XYMap::constructWithUserFunction(width, height, xy_legacy_wrapper);
    blur2d(leds, width, height, blur_amount, xy);
}

void blurRows(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount,
              const XYMap &xyMap) {

    /*    for( fl::u8 row = 0; row < height; row++) {
            CRGB* rowbase = leds + (row * width);
            blur1d( rowbase, width, blur_amount);
        }
    */
    // blur rows same as columns, for irregular matrix
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    for (fl::u8 row = 0; row < height; row++) {
        CRGB carryover = CRGB::Black;
        for (fl::u8 i = 0; i < width; i++) {
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
void blurColumns(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount,
                 const XYMap &xyMap) {
    // blur columns
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    for (fl::u8 col = 0; col < width; ++col) {
        CRGB carryover = CRGB::Black;
        for (fl::u8 i = 0; i < height; ++i) {
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

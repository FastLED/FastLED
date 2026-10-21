

#include "fl/gfx/leds.h"
#include "crgb.h"
#include "fl/stl/assert.h"
#include "fl/math/xymap.h"
#include "fl/stl/noexcept.h"

namespace fl {

Leds::Leds(CRGB *leds, const XYMap &xymap) FL_NO_EXCEPT : mXyMap(xymap), mLeds(leds, xymap.getTotal()) {}

CRGB &Leds::operator()(int x, int y) FL_NO_EXCEPT {
    if (!mXyMap.has(x, y)) {
        return empty();
    }
    return mLeds[mXyMap(x, y)];
}

CRGB &Leds::empty() FL_NO_EXCEPT {
    static CRGB empty_led;
    return empty_led;
}

const CRGB &Leds::operator()(int x, int y) const FL_NO_EXCEPT {
    if (!mXyMap.has(x, y)) {
        return empty();
    }
    return mLeds[mXyMap(x, y)];
}

CRGB *Leds::operator[](int y) FL_NO_EXCEPT {
    FASTLED_ASSERT(mXyMap.isSerpentine() || mXyMap.isLineByLine(),
                   "XYMap is not serpentine or line by line");
    return &mLeds[mXyMap(0, y)];
}
const CRGB *Leds::operator[](int y) const FL_NO_EXCEPT {
    FASTLED_ASSERT(mXyMap.isSerpentine() || mXyMap.isLineByLine(),
                   "XYMap is not serpentine or line by line");
    return &mLeds[mXyMap(0, y)];
}

Leds::Leds(CRGB *leds, u16 width, u16 height)
    FL_NO_EXCEPT : Leds(leds, XYMap::constructRectangularGrid(width, height)) {}



} // namespace fl

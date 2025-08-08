

#include "fl/leds.h"
#include "crgb.h"
#include "fl/assert.h"
#include "fl/xymap.h"

namespace fl {

Leds::Leds(CRGB *leds, const XYMap &xymap) : mXyMap(xymap), mLeds(leds) {}

CRGB &Leds::operator()(int x, int y) {
    if (!mXyMap.has(x, y)) {
        return empty();
    }
    return mLeds[mXyMap(x, y)];
}

CRGB &Leds::empty() {
    static CRGB empty_led;
    return empty_led;
}

const CRGB &Leds::operator()(int x, int y) const {
    if (!mXyMap.has(x, y)) {
        return empty();
    }
    return mLeds[mXyMap(x, y)];
}

CRGB *Leds::operator[](int y) {
    FASTLED_ASSERT(mXyMap.isSerpentine() || mXyMap.isLineByLine(),
                   "XYMap is not serpentine or line by line");
    return &mLeds[mXyMap(0, y)];
}
const CRGB *Leds::operator[](int y) const {
    FASTLED_ASSERT(mXyMap.isSerpentine() || mXyMap.isLineByLine(),
                   "XYMap is not serpentine or line by line");
    return &mLeds[mXyMap(0, y)];
}

Leds::Leds(CRGB *leds, u16 width, u16 height)
    : Leds(leds, XYMap::constructRectangularGrid(width, height)) {}



} // namespace fl

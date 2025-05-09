
#include "fl/ledgrid.h"
#include "crgb.h"
#include "fl/assert.h"
#include "fl/xymap.h"

namespace fl {

LedGrid::LedGrid(CRGB *leds, const XYMap &xymap) : mXyMap(xymap), mLeds(leds) {}

CRGB &LedGrid::operator()(int x, int y) {
    if (!mXyMap.has(x, y)) {
        return empty();
    }
    return mLeds[mXyMap(x, y)];
}

CRGB &LedGrid::empty() {
    static CRGB empty_led;
    return empty_led;
}

const CRGB &LedGrid::operator()(int x, int y) const {
    if (!mXyMap.has(x, y)) {
        return empty();
    }
    return mLeds[mXyMap(x, y)];
}

CRGB *LedGrid::operator[](int y) {
    FASTLED_ASSERT(mXyMap.isSerpentine() || mXyMap.isLineByLine(),
                   "XYMap is not serpentine or line by line");
    return &mLeds[mXyMap(0, y)];
}
const CRGB *LedGrid::operator[](int y) const {
    FASTLED_ASSERT(mXyMap.isSerpentine() || mXyMap.isLineByLine(),
                   "XYMap is not serpentine or line by line");
    return &mLeds[mXyMap(0, y)];
}

LedGrid::LedGrid(CRGB *leds, uint16_t width, uint16_t height)
    : LedGrid(leds, XYMap::constructRectangularGrid(width, height)) {}

} // namespace fl

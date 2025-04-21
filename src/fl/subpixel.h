
#pragma once

#include <stdint.h>

#include "fl/point.h"

#include "fl/draw_mode.h"
#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;

class SubPixel2x2 {

  public:
    SubPixel2x2() = default;
    SubPixel2x2(const point_xy<int> &origin) : mOrigin(origin) {}
    SubPixel2x2(const SubPixel2x2 &) = default;
    SubPixel2x2 &operator=(const SubPixel2x2 &) = default;
    SubPixel2x2(SubPixel2x2 &&) = default;

    uint8_t &operator()(int x, int y) { return at(x, y); }
    uint8_t &at(int x, int y) { return mTile[y][x]; }
    const uint8_t& at(int x, int y) const { return mTile[y][x]; }

    uint8_t &lower_left() { return at(0, 0); }
    uint8_t &upper_left() { return at(0, 1); }
    uint8_t &lower_right() { return at(1, 0); }
    uint8_t &upper_right() { return at(1, 1); }

    point_xy<int> origin() const { return mOrigin; }

    // Draws the subpixel tile to the led array.
    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;

  private:
    uint8_t mTile[2][2] = {};
    point_xy<int> mOrigin;
};

} // namespace fl
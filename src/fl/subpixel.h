
#pragma once

#include <stdint.h>

#include "fl/namespace.h"
#include "fl/point.h"
#include "fl/slice.h"
#include "fl/point.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;
class Raster;
class XYDrawUint8Visitor;

class SubPixel2x2 {

  public:
    static void Rasterize(const Slice<const SubPixel2x2> &tiles,
                          Raster *output, rect_xy<int> *optional_bounds = nullptr);

    SubPixel2x2() = default;
    SubPixel2x2(const point_xy<int> &origin) : mOrigin(origin) {}
    SubPixel2x2(const SubPixel2x2 &) = default;
    SubPixel2x2 &operator=(const SubPixel2x2 &) = default;
    SubPixel2x2(SubPixel2x2 &&) = default;

    uint8_t &operator()(int x, int y) { return at(x, y); }
    uint8_t &at(int x, int y) { return mTile[y][x]; }
    const uint8_t &at(int x, int y) const { return mTile[y][x]; }

    uint8_t &lower_left() { return at(0, 0); }
    uint8_t &upper_left() { return at(0, 1); }
    uint8_t &lower_right() { return at(1, 0); }
    uint8_t &upper_right() { return at(1, 1); }

    point_xy<int> origin() const { return mOrigin; }

    rect_xy<int> bounds() const {
        point_xy<int> min = mOrigin;
        point_xy<int> max = mOrigin + point_xy<int>(1, 1);
        return rect_xy<int>(min, max);
    }

    // Draws the subpixel tile to the led array.
    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;
    void draw(const XYMap &xymap, XYDrawUint8Visitor *visitor) const ;

  private:
    uint8_t mTile[2][2] = {};
    // Subpixels can be rendered outside the viewport so this must be signed.
    point_xy<int> mOrigin;
};

} // namespace fl
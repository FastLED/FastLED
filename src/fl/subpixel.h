
#pragma once

#include <stdint.h>

#include "fl/namespace.h"
#include "fl/point.h"
#include "fl/slice.h"
#include "fl/xymap.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;
class XYRasterSparse;
class XYDrawUint8Visitor;

class SubPixel2x2 {

  public:
    static void Rasterize(const Slice<const SubPixel2x2> &tiles,
                          XYRasterSparse *output, rect_xy<int> *optional_bounds = nullptr) ;

    SubPixel2x2() = default;
    SubPixel2x2(const point_xy<int> &origin) : mOrigin(origin) {}
    SubPixel2x2(const SubPixel2x2 &) = default;
    SubPixel2x2 &operator=(const SubPixel2x2 &) = default;
    SubPixel2x2(SubPixel2x2 &&) = default;

    void scale(uint8_t scale);

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
        point_xy<int> max = mOrigin + point_xy<int>(2, 2);
        return rect_xy<int>(min, max);
    }

    // Draws the subpixel tile to the led array.
    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;

    // Inlined, yet customizable drawing access. This will only send you pixels
    // that are within the bounds of the XYMap.
    template <typename XYVisitor>
    void draw(const XYMap &xymap, XYVisitor& visitor) const {
        for (uint16_t x = 0; x < 2; ++x) {
            for (uint16_t y = 0; y < 2; ++y) {
                uint8_t value = at(x, y);
                if (value > 0) {
                    int xx = mOrigin.x + x;
                    int yy = mOrigin.y + y;
                    if (xymap.has(xx, yy)) {
                        int index = xymap(xx, yy);
                        visitor.draw(point_xy<int>(xx, yy), index, value);
                    }
                }
            }
        }
    }

  private:
    uint8_t mTile[2][2] = {};
    // Subpixels can be rendered outside the viewport so this must be signed.
    point_xy<int> mOrigin;
};

} // namespace fl
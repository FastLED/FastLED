
#pragma once

#include <stdint.h>

#include "fl/geometry.h"
#include "fl/namespace.h"
#include "fl/slice.h"
#include "fl/xymap.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;
class XYRasterU8Sparse;

class Tile2x2_u8 {

  public:
    static void Rasterize(const Slice<const Tile2x2_u8> &tiles,
                          XYRasterU8Sparse *output);

    Tile2x2_u8() = default;
    Tile2x2_u8(const vec2<int16_t> &origin) : mOrigin(origin) {}
    Tile2x2_u8(const Tile2x2_u8 &) = default;
    Tile2x2_u8 &operator=(const Tile2x2_u8 &) = default;
    Tile2x2_u8(Tile2x2_u8 &&) = default;

    void scale(uint8_t scale);

    uint8_t &operator()(int x, int y) { return at(x, y); }
    uint8_t &at(int x, int y) { return mTile[y][x]; }
    const uint8_t &at(int x, int y) const { return mTile[y][x]; }

    uint8_t &lower_left() { return at(0, 0); }
    uint8_t &upper_left() { return at(0, 1); }
    uint8_t &lower_right() { return at(1, 0); }
    uint8_t &upper_right() { return at(1, 1); }

    uint8_t maxValue() const {
        uint8_t max = 0;
        max = MAX(max, at(0, 0));
        max = MAX(max, at(0, 1));
        max = MAX(max, at(1, 0));
        max = MAX(max, at(1, 1));
        return max;
    }

    static Tile2x2_u8 Max(const Tile2x2_u8 &a, const Tile2x2_u8 &b) {
        Tile2x2_u8 result;
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                result.at(x, y) = MAX(a.at(x, y), b.at(x, y));
            }
        }
        return result;
    }

    vec2<int16_t> origin() const { return mOrigin; }

    /// bounds => [begin_x, end_x) (where end_x is exclusive)
    rect<int16_t> bounds() const {
        vec2<int16_t> min = mOrigin;
        vec2<int16_t> max = mOrigin + vec2<int16_t>(2, 2);
        return rect<int16_t>(min, max);
    }

    // Draws the subpixel tile to the led array.
    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;

    // Inlined, yet customizable drawing access. This will only send you pixels
    // that are within the bounds of the XYMap.
    template <typename XYVisitor>
    void draw(const XYMap &xymap, XYVisitor &visitor) const {
        for (uint16_t x = 0; x < 2; ++x) {
            for (uint16_t y = 0; y < 2; ++y) {
                uint8_t value = at(x, y);
                if (value > 0) {
                    int xx = mOrigin.x + x;
                    int yy = mOrigin.y + y;
                    if (xymap.has(xx, yy)) {
                        int index = xymap(xx, yy);
                        visitor.draw(vec2<int16_t>(xx, yy), index, value);
                    }
                }
            }
        }
    }

  private:
    uint8_t mTile[2][2] = {};
    // Subpixels can be rendered outside the viewport so this must be signed.
    vec2<int16_t> mOrigin;
};

} // namespace fl
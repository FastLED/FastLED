
#pragma once

#include <stdint.h>

#include "fl/geometry.h"
#include "fl/namespace.h"
#include "fl/pair.h"
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

    void setOrigin(int16_t x, int16_t y) { mOrigin = vec2<int16_t>(x, y); }

    uint8_t &operator()(int x, int y) { return at(x, y); }
    uint8_t &at(int x, int y) { return mTile[y][x]; }
    const uint8_t &at(int x, int y) const { return mTile[y][x]; }

    uint8_t &lower_left() { return at(0, 0); }
    uint8_t &upper_left() { return at(0, 1); }
    uint8_t &lower_right() { return at(1, 0); }
    uint8_t &upper_right() { return at(1, 1); }

    const uint8_t &lower_left() const { return at(0, 0); }
    const uint8_t &upper_left() const { return at(0, 1); }
    const uint8_t &lower_right() const { return at(1, 0); }
    const uint8_t &upper_right() const { return at(1, 1); }

    uint8_t maxValue() const;

    static Tile2x2_u8 MaxTile(const Tile2x2_u8 &a, const Tile2x2_u8 &b);

    vec2<int16_t> origin() const { return mOrigin; }

    /// bounds => [begin_x, end_x) (where end_x is exclusive)
    rect<int16_t> bounds() const;

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

class Tile2x2_u8_wrap {
    // This is a class that is like a Tile2x2_u8 but wraps around the edges.
    // This is useful for cylinder mapping where the x-coordinate wraps around
    // the width of the cylinder and the y-coordinate wraps around the height.
    // This converts a tile2x2 to a wrapped x,y version.
  public:
    using Data = fl::pair<vec2i16, uint8_t>;  // absolute position, alpha

    Tile2x2_u8_wrap() = default;
    Tile2x2_u8_wrap(const Tile2x2_u8 &from, uint16_t width);
    Tile2x2_u8_wrap(const Tile2x2_u8 &from, uint16_t width, uint16_t height);

    // Returns the absolute position and the alpha.
    Data &at(uint16_t x, uint16_t y);
    const Data &at(uint16_t x, uint16_t y) const;

  private:
    Data tile[2][2] = {}; // zero filled.
};

} // namespace fl
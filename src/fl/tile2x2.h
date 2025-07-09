#pragma once

#include "fl/stdint.h"

#include "fl/geometry.h"
#include "fl/namespace.h"
#include "fl/pair.h"
#include "fl/span.h"
#include "fl/xymap.h"
#include "fl/vector.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;
class XYRasterU8Sparse;


class Tile2x2_u8 {

  public:
    static void Rasterize(const span<const Tile2x2_u8> &tiles,
                          XYRasterU8Sparse *output);

    Tile2x2_u8() = default;
    Tile2x2_u8(const vec2<i16> &origin) : mOrigin(origin) {}
    Tile2x2_u8(const Tile2x2_u8 &) = default;
    Tile2x2_u8 &operator=(const Tile2x2_u8 &) = default;
    Tile2x2_u8(Tile2x2_u8 &&) = default;

    void scale(u8 scale);

    void setOrigin(i16 x, i16 y) { mOrigin = vec2<i16>(x, y); }

    u8 &operator()(int x, int y) { return at(x, y); }
    u8 &at(int x, int y) { return mTile[y][x]; }
    const u8 &at(int x, int y) const { return mTile[y][x]; }

    u8 &lower_left() { return at(0, 0); }
    u8 &upper_left() { return at(0, 1); }
    u8 &lower_right() { return at(1, 0); }
    u8 &upper_right() { return at(1, 1); }

    const u8 &lower_left() const { return at(0, 0); }
    const u8 &upper_left() const { return at(0, 1); }
    const u8 &lower_right() const { return at(1, 0); }
    const u8 &upper_right() const { return at(1, 1); }

    u8 maxValue() const;

    static Tile2x2_u8 MaxTile(const Tile2x2_u8 &a, const Tile2x2_u8 &b);

    vec2<i16> origin() const { return mOrigin; }

    /// bounds => [begin_x, end_x) (where end_x is exclusive)
    rect<i16> bounds() const;

    // Draws the subpixel tile to the led array.
    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;

    // Inlined, yet customizable drawing access. This will only send you pixels
    // that are within the bounds of the XYMap.
    template <typename XYVisitor>
    void draw(const XYMap &xymap, XYVisitor &visitor) const {
        for (u16 x = 0; x < 2; ++x) {
            for (u16 y = 0; y < 2; ++y) {
                u8 value = at(x, y);
                if (value > 0) {
                    int xx = mOrigin.x + x;
                    int yy = mOrigin.y + y;
                    if (xymap.has(xx, yy)) {
                        int index = xymap(xx, yy);
                        visitor.draw(vec2<i16>(xx, yy), index, value);
                    }
                }
            }
        }
    }

  private:
    u8 mTile[2][2] = {};
    // Subpixels can be rendered outside the viewport so this must be signed.
    vec2<i16> mOrigin;
};

class Tile2x2_u8_wrap {
    // This is a class that is like a Tile2x2_u8 but wraps around the edges.
    // This is useful for cylinder mapping where the x-coordinate wraps around
    // the width of the cylinder and the y-coordinate wraps around the height.
    // This converts a tile2x2 to a wrapped x,y version.
  public:
    using Entry = fl::pair<vec2i16, u8>;  // absolute position, alpha
    using Data = Entry[2][2];

    Tile2x2_u8_wrap();
    Tile2x2_u8_wrap(const Tile2x2_u8 &from, u16 width);
    Tile2x2_u8_wrap(const Tile2x2_u8 &from, u16 width, u16 height);

    Tile2x2_u8_wrap(const Data& data);

    // Returns the absolute position and the alpha.
    Entry &at(u16 x, u16 y);
    const Entry &at(u16 x, u16 y) const;

    // Interpolates between two wrapped tiles and returns up to 2 interpolated tiles
    static vector_fixed<Tile2x2_u8_wrap, 2> Interpolate(const Tile2x2_u8_wrap& a, const Tile2x2_u8_wrap& b, float t);

  private:
    Data mData = {};
};

} // namespace fl

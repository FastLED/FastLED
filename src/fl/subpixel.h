
#pragma once

#include <stdint.h>

#include "fl/point.h"

#include "fl/draw_mode.h"
#include "fl/namespace.h"
#include "fl/slice.h"
#include "fl/grid.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;
class Raster;

class SubPixel2x2 {

  public:

    static void DrawRaster(const Slice<SubPixel2x2> &tiles, Raster* output);

    SubPixel2x2() = default;
    SubPixel2x2(const point_xy<uint16_t> &origin) : mOrigin(origin) {}
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

    point_xy<uint16_t> origin() const { return mOrigin; }

    // Draws the subpixel tile to the led array.
    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;

  private:
    uint8_t mTile[2][2] = {};
    point_xy<uint16_t> mOrigin;
};


class Raster {
    public:
        Raster(const point_xy<uint16_t>& origin, uint16_t width, uint16_t height) {
            reset(origin, width, height);
        }

        Raster(const Raster&) = delete;
        void reset(const point_xy<uint16_t>& origin, uint16_t width, uint16_t height) {
            mGrid.reset(width, height);
            mOrigin = origin;
        }

        uint8_t& at(uint16_t x, uint16_t y) {
            return mGrid.at(x, y);
        }
        
    private:
        Grid<uint8_t> mGrid;
        point_xy<uint16_t> mOrigin;
};

} // namespace fl
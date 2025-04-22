
#pragma once

#include <stdint.h>

#include "fl/grid.h"
#include "fl/namespace.h"
#include "fl/point.h"
#include "fl/slice.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;
class XYDrawUint8Visitor;
class SubPixel2x2;

class Raster {
  public:
    Raster(int width, int height) {
        mGrid.reset(width, height);
        mOrigin = point_xy<int>(0, 0);
        mWidthHeight = point_xy<int>(width, height);
        mInitialized = true;

    }
    // Raster(uint16_t width, uint16_t height) {
    //     reset(point_xy<int>(0, 0), width, height);
    // }
    // Raster(const point_xy<int> &origin, uint16_t width, uint16_t height) {
    //     reset(origin, width, height);
    // }
    Raster(const Raster &) = delete;
    void reset(const point_xy<int> &origin, uint16_t width,
               uint16_t height) {
        mGrid.reset(width, height);
        mOrigin = origin;
        mInitialized = true;
    }

    // builder pattern
    Raster& setOrigin(const point_xy<int> &origin) {
        mOrigin = origin;
        return *this;
    }
    Raster& setSize(uint16_t width, uint16_t height) {
        mGrid.reset(width, height);
        return *this;
    }

    Raster& reset() {
        mGrid.reset(width(), height());
        return *this;
    }

    // Renders the subpixel tiles to the raster. Any previous data is cleared.
    // Memory will only be allocated if the size of the raster increased.
    void rasterize(const Slice<const SubPixel2x2> &tiles);
    uint8_t &at(uint16_t x, uint16_t y) { return mGrid.at(x, y); }
    const uint8_t &at(uint16_t x, uint16_t y) const { return mGrid.at(x, y); }

    const point_xy<int> origin() const { return mOrigin; }
    point_xy<int> global_min() const { return mOrigin; }
    point_xy<int> global_max() const {
        return mOrigin + point_xy<int>(mGrid.width(), mGrid.height());
    }

    rect_xy<int> bounds() const {
        point_xy<int> min = origin();
        point_xy<int> max = global_max();
        return rect_xy<int>(min, max);
    }

    uint16_t width() const { return mGrid.width(); }
    uint16_t height() const { return mGrid.height(); }

    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;

    // Uses the visitor pattern to abstract away the drawing. The values sent
    // to the visitor will always be within the valid range as specified
    // by the xymap.
    void draw(const XYMap &xymap, XYDrawUint8Visitor *visitor) const;

  private:
    bool mInitialized = false;
    Grid<uint8_t> mGrid;
    point_xy<int> mOrigin;
    point_xy<int> mWidthHeight;  // 0,0 if unset.
};

} // namespace fl
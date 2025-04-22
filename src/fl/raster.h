
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

class XYRaster {
  public:
    XYRaster() = default;

    XYRaster(int width, int height) {
        // mGrid.reset(width, height);
        mOrigin = point_xy<int>(0, 0);
        mWidthHeight = point_xy<int>(
            width, height); // constrains the raster to prevent overflow.
        // mInitialized = true;  // technically we are not initialized yet and
        // the grid still needs to be allocated.
    }

    XYRaster(const XYRaster &) = delete;
    void reset(const point_xy<int> &origin, uint16_t width, uint16_t height) {
        mGrid.reset(width, height);
        mOrigin = origin;
        mInitialized = true;
    }

    // builder pattern
    XYRaster &setOrigin(const point_xy<int> &origin) {
        mOrigin = origin;
        return *this;
    }
    XYRaster &setSize(uint16_t width, uint16_t height) {
        mGrid.reset(width, height);
        return *this;
    }

    XYRaster &reset() {
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

    // Inlined, yet customizable drawing access. This will only send you pixels
    // that are within the bounds of the XYMap.
    template <typename XYVisitor>
    void draw(const XYMap &xymap, XYVisitor& visitor) const {
        const uint16_t w = width();
        const uint16_t h = height();
        const point_xy<int> origin = this->origin();
        for (uint16_t x = 0; x < w; ++x) {
            for (uint16_t y = 0; y < h; ++y) {
                uint16_t xx = x + origin.x;
                uint16_t yy = y + origin.y;
                if (!xymap.has(xx, yy)) {
                    continue;
                }
                uint32_t index = xymap(xx, yy);
                uint8_t value = at(x, y);
                if (value > 0) {  // Something wrote here.
                    point_xy<int> pt = {xx, yy};
                    visitor.draw(pt, index, value);
                }
            }
        }
    }

  private:
    bool mInitialized = false;
    Grid<uint8_t> mGrid;
    point_xy<int> mOrigin;
    point_xy<int> mWidthHeight; // 0,0 if unset.
};

} // namespace fl
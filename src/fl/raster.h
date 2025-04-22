
#pragma once

#include <stdint.h>

#include "fl/grid.h"
#include "fl/namespace.h"
#include "fl/point.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class XYMap;


struct DrawUint8Visitor {
    virtual ~DrawUint8Visitor() = default;
    virtual void draw(const point_xy<uint16_t> &pt, uint8_t value) = 0;
};

class Raster {
  public:
    Raster() = default;
    Raster(const point_xy<uint16_t> &origin, uint16_t width, uint16_t height) {
        reset(origin, width, height);
    }
    Raster(const Raster &) = delete;
    void reset(const point_xy<uint16_t> &origin, uint16_t width,
               uint16_t height) {
        mGrid.reset(width, height);
        mOrigin = origin;
    }
    uint8_t &at(uint16_t x, uint16_t y) { return mGrid.at(x, y); }
    const uint8_t &at(uint16_t x, uint16_t y) const { return mGrid.at(x, y); }

    const point_xy<uint16_t> origin() const { return mOrigin; }
    point_xy<uint16_t> global_min() const { return mOrigin; }
    point_xy<uint16_t> global_max() const {
        return mOrigin + point_xy<uint16_t>(mGrid.width(), mGrid.height());
    }

    rect_xy<uint16_t> bounds() const {
        point_xy<uint16_t> min = origin();
        point_xy<uint16_t> max = global_max();
        return rect_xy<uint16_t>(min, max);
    }

    uint16_t width() const { return mGrid.width(); }
    uint16_t height() const { return mGrid.height(); }

    void draw(const CRGB &color, const XYMap &xymap, CRGB *out) const;

    // Uses the visitor pattern to abstract away the drawing. The values sent
    // to the visitor will always be within the valid range as specified
    // by the xymap.
    void draw(const XYMap& xymap, DrawUint8Visitor* visitor) const ;

  private:
    Grid<uint8_t> mGrid;
    point_xy<uint16_t> mOrigin;
};

} // namespace fl

#pragma once

#include <stdint.h>

#include "fl/point.h"
#include "fl/grid.h"

namespace fl {

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
    
    point_xy<uint16_t> global_min() const {
        return mOrigin;
    }
    point_xy<uint16_t> global_max() const {
        return mOrigin + point_xy<uint16_t>(mGrid.width(), mGrid.height());
    }

  private:
    Grid<uint8_t> mGrid;
    point_xy<uint16_t> mOrigin;
};

} // namespace fl
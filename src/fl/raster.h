
#pragma once

#include <stdint.h>

#include "fl/point.h"
#include "fl/grid.h"

namespace fl {

class Raster {
  public:
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

  private:
    Grid<uint8_t> mGrid;
    point_xy<uint16_t> mOrigin;
};

} // namespace fl
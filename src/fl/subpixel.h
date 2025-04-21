
#pragma once

#include "fl/point.h"
#include <stdint.h>

namespace fl {
class SubPixel {

  public:
    SubPixel(const point_xy<int> &origin) : mOrigin(origin) {}
    SubPixel(const SubPixel &) = default;
    SubPixel &operator=(const SubPixel &) = default;
    SubPixel(SubPixel &&) = default;

    uint8_t &operator()(int x, int y) { return at(x, y); }
    uint8_t &at(int x, int y) { return mTile[y][x]; }

    uint8_t &lower_left() { return at(0, 0); }
    uint8_t &upper_left() { return at(0, 1); }
    uint8_t &lower_right() { return at(1, 0); }
    uint8_t &upper_right() { return at(1, 1); }

    point_xy<int> origin() const  { return mOrigin; }

  private:
    uint8_t mTile[2][2] = {};
    point_xy<int> mOrigin;
};

} // namespace fl
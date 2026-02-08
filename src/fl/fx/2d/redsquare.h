/// @brief   Implements a simple red square effect for 2D LED grids.

#pragma once

#include "fl/fastled.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/fx/fx2d.h"

namespace fl {

FASTLED_SHARED_PTR(RedSquare);

class RedSquare : public Fx2d {
  public:
    struct Math {
        template <typename T> static T Min(T a, T b) { return a < b ? a : b; }
    };

    RedSquare(const XYMap& xymap) : Fx2d(xymap) {}

    void draw(DrawContext context) override {
        u16 width = getWidth();
        u16 height = getHeight();
        u16 square_size = Math::Min(width, height) / 2;
        u16 start_x = (width - square_size) / 2;
        u16 start_y = (height - square_size) / 2;

        for (u16 x = 0; x < width; x++) {
            for (u16 y = 0; y < height; y++) {
                u16 idx = mXyMap.mapToIndex(x, y);
                if (idx < mXyMap.getTotal()) {
                    if (x >= start_x && x < start_x + square_size &&
                        y >= start_y && y < start_y + square_size) {
                        context.leds[idx] = CRGB::Red;
                    } else {
                        context.leds[idx] = CRGB::Black;
                    }
                }
            }
        }
    }

    fl::string fxName() const override { return "red_square"; }
};

} // namespace fl

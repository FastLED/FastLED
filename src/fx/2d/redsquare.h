/// @brief   Implements a simple red square effect for 2D LED grids.

#pragma once

#include "FastLED.h"
#include "fl/memory.h"
#include "fx/fx2d.h"

namespace fl {

FASTLED_SMART_PTR(RedSquare);

class RedSquare : public Fx2d {
  public:
    struct Math {
        template <typename T> static T Min(T a, T b) { return a < b ? a : b; }
    };

    RedSquare(const XYMap& xymap) : Fx2d(xymap) {}

    void draw(DrawContext context) override {
        uint16_t width = getWidth();
        uint16_t height = getHeight();
        uint16_t square_size = Math::Min(width, height) / 2;
        uint16_t start_x = (width - square_size) / 2;
        uint16_t start_y = (height - square_size) / 2;

        for (uint16_t x = 0; x < width; x++) {
            for (uint16_t y = 0; y < height; y++) {
                uint16_t idx = mXyMap.mapToIndex(x, y);
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

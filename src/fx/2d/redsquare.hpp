/// @brief   Implements a simple red square effect for 2D LED grids.

#pragma once

#include "FastLED.h"
#include "fx/fx2d.h"
#include "ref.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(RedSquare);

class RedSquare : public FxGrid {
  public:
    struct Math {
        template <typename T> static T min(T a, T b) { return a < b ? a : b; }
    };

    RedSquare(XYMap xymap) : FxGrid(xymap) {}

    void lazyInit() override {}

    void draw(DrawContext context) override {
        uint16_t width = getWidth();
        uint16_t height = getHeight();
        uint16_t square_size = Math::min(width, height) / 2;
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

    const char *fxName(int) const override { return "red_square"; }
};

FASTLED_NAMESPACE_END

/// @file    NoisePlusPalette.hpp
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.hpp

#pragma once

#include <stdint.h>

#include "FastLED.h"
#include "fx/fx2d.h"
#include "lib8tion/random8.h"
#include "noise.h"
#include "ptr.h"
#include "xymap.h"
#include "bilinear_expansion.h"
// #include <iostream>

FASTLED_NAMESPACE_BEGIN

// Uses bilearn filtering to double the size of the grid.
class GridDoubler : public FxGrid {
  public:
    GridDoubler(XYMap xymap, FxGrid *fx) : FxGrid(xymap), mDelegate(fx) {}

    // Note: This GridDoubler implementation assumes that the delegate's
    // dimensions are exactly half of the output dimensions. While it can
    // technically work with non-power-of-two sizes, it may produce unexpected
    // results or visual artifacts if the input and output dimensions are not in
    // a perfect 1:2 ratio. For best results, use this with delegate grids that
    // have dimensions that are powers of two, ensuring the output dimensions
    // are also powers of two.
    void lazyInit() override {}
    void draw(DrawContext context) override {
        if (!mSurface) {
            mSurface.reset(new CRGB[mDelegate->getNumLeds()]);
        }
        DrawContext delegateContext = context;
        delegateContext.leds = mSurface.get();
        mDelegate->draw(delegateContext);

        //bilinearExpand(context.leds, mSurface.get(), 16, 16, 22, 22);
        bilinearExpandPowerOf2(context.leds, mSurface.get(), 16, 16, mXyMap);
        // justDrawIt(context.leds, mSurface.get(), 16, 16);
        // std::cout << "dumping" << std::endl;
    }

    const char *fxName() const override { return "GridDoubler"; }

  private:
    void justDrawIt(CRGB *output, const CRGB *input, uint16_t width,
                    uint16_t height) {
        uint16_t n = mXyMap.getTotal();
        uint16_t total_in = width * height;
        for (uint16_t w = 0; w < width; w++) {
            for (uint16_t h = 0; h < height; h++) {
                uint16_t idx = mXyMap.mapToIndex(w, h);
                if (idx < n) {
                    output[idx] = input[w * height + h];
                }
            }
        }
    }
    FxGrid *mDelegate;
    scoped_array<CRGB> mSurface;
};

FASTLED_NAMESPACE_END

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

// Optimized for 2^n grid sizes in terms of both memory and performance.
// If you are somehow running this on AVR then you probably want this if
// you can make your grid size a power of 2.
#define FASTLED_GRID_EXPANDER_STRATEGY_ALWAYS_POWER_OF_2 0 // 0 for always power of 2.
// Uses more memory than FASTLED_GRID_EXPANDER_STRATEGY_ALWAYS_POWER_OF_2 but can handle
// arbitrary grid sizes.
#define FASTLED_GRID_EXPANDER_STRATEGY_HIGH_PRECISION    1 // 1 for always choose high precision.
// Uses the most memory but is faster for 2^n grid sizes which are common.
#define FASTLED_GRID_EXPANDER_STRATEGY_DECIDE_AT_RUNTIME 2 // 2 for runtime decision.

#ifndef FASTLED_GRID_EXPANDER_STRATEGY
#define FASTLED_GRID_EXPANDER_STRATEGY FASTLED_GRID_EXPANDER_STRATEGY_DECIDE_AT_RUNTIME
#endif

FASTLED_NAMESPACE_BEGIN

// Uses bilearn filtering to double the size of the grid.
class GridDoubler : public FxGrid {
  public:
    GridDoubler(XYMap xymap, FxGrid *fx) : FxGrid(xymap), mDelegate(fx) {
        // Turn off re-mapping of the delegate's XYMap, since bilinearExpand needs to
        // work in screen coordinates. The final mapping will for this class will
        // still be performed.
        mDelegate->getXYMap().setRectangularGrid();
    }
    void lazyInit() override {}
    void draw(DrawContext context) override {
        if (!mSurface) {
            mSurface.reset(new CRGB[mDelegate->getNumLeds()]);
        }
        DrawContext delegateContext = context;
        delegateContext.leds = mSurface.get();
        mDelegate->draw(delegateContext);

        uint16_t in_w = mDelegate->getWidth();
        uint16_t in_h = mDelegate->getHeight();
        uint16_t out_w = getWidth();
        uint16_t out_h = getHeight();
        expand(mSurface.get(), context.leds, in_w, in_h, mXyMap);
    }

    void expand(const CRGB *input, CRGB *output, uint16_t width, uint16_t height, XYMap mXyMap) {
        #if FASTLED_GRID_EXPANDER_STRATEGY == FASTLED_GRID_EXPANDER_STRATEGY_ALWAYS_POWER_OF_2
        bilinearExpandPowerOf2(input, output, width, height, mXyMap);
        #elif FASTLED_GRID_EXPANDER_STRATEGY == FASTLED_GRID_EXPANDER_STRATEGY_HIGH_PRECISION
        bilinearExpandArbitrary(input, output, width, height, mXyMap);
        #elif FASTLED_GRID_EXPANDER_STRATEGY == FASTLED_GRID_EXPANDER_STRATEGY_DECIDE_AT_RUNTIME
        bilinearExpand(input, output, width, height, mXyMap);
        #else
        #error "Invalid FASTLED_GRID_EXPANDER_STRATEGY"
        #endif
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

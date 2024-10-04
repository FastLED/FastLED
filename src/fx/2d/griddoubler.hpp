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

        bilinearExpand(context.leds, mSurface.get(), 16, 16);
        // justDrawIt(context.leds, mSurface.get(), 16, 16);
        std::cout << "dumping" << std::endl;
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

    void bilinearExpand(CRGB *output, const CRGB *input, uint16_t width,
                        uint16_t height) {
        uint16_t inputWidth = width / 2;
        uint16_t inputHeight = height / 2;
        uint16_t n = mXyMap.getTotal();

        for (uint16_t y = 0; y < height; y++) {
            for (uint16_t x = 0; x < width; x++) {
                float fx = (x * (inputWidth - 1.0f)) / (width - 1.0f);
                float fy = (y * (inputHeight - 1.0f)) / (height - 1.0f);
                uint16_t ix = static_cast<uint16_t>(fx);
                uint16_t iy = static_cast<uint16_t>(fy);
                float dx = fx - ix;
                float dy = fy - iy;

                // Corrected index calculations
                uint16_t ix1 = (ix + 1 < inputWidth) ? ix + 1 : ix;
                uint16_t iy1 = (iy + 1 < inputHeight) ? iy + 1 : iy;

                uint16_t i00 = iy * inputWidth + ix;
                uint16_t i10 = iy * inputWidth + ix1;
                uint16_t i01 = iy1 * inputWidth + ix;
                uint16_t i11 = iy1 * inputWidth + ix1;

                CRGB c00 = input[i00];
                CRGB c10 = input[i10];
                CRGB c01 = input[i01];
                CRGB c11 = input[i11];

                CRGB result;
                result.r =
                    bilinearInterpolate(c00.r, c10.r, c01.r, c11.r, dx, dy);
                result.g =
                    bilinearInterpolate(c00.g, c10.g, c01.g, c11.g, dx, dy);
                result.b =
                    bilinearInterpolate(c00.b, c10.b, c01.b, c11.b, dx, dy);

                uint16_t idx = mXyMap.mapToIndex(x, y);
                if (idx < n) {
                    output[idx] = result;
                }
            }
        }
    }

    uint8_t bilinearInterpolate(uint8_t v00, uint8_t v10, uint8_t v01,
                                uint8_t v11, float dx, float dy) {
        float a = v00 * (1 - dx) * (1 - dy);
        float b = v10 * dx * (1 - dy);
        float c = v01 * (1 - dx) * dy;
        float d = v11 * dx * dy;
        float result = a + b + c + d;
        return static_cast<uint8_t>(std::min(std::max(result, 0.0f), 255.0f));
    }

    FxGrid *mDelegate;
    scoped_array<CRGB> mSurface;
};

FASTLED_NAMESPACE_END

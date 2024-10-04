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

        //bilinearExpand(context.leds, mSurface.get(), 16, 16, 22, 22);
        bilinearExpandPowerOf2(context.leds, mSurface.get(), 16, 16);
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
    void bilinearExpand(CRGB *output, const CRGB *input, uint16_t inputWidth,
                        uint16_t inputHeight, uint16_t outputWidth,
                        uint16_t outputHeight) {
        uint16_t n = mXyMap.getTotal();
        const uint16_t scale_factor =
            256; // Using 8 bits for the fractional part

        for (uint16_t y = 0; y < outputHeight; y++) {
            for (uint16_t x = 0; x < outputWidth; x++) {
                // Calculate the corresponding position in the input grid
                uint32_t fx = ((uint32_t)x * (inputWidth - 1) * scale_factor) /
                              (outputWidth - 1);
                uint32_t fy = ((uint32_t)y * (inputHeight - 1) * scale_factor) /
                              (outputHeight - 1);

                uint16_t ix = fx / scale_factor; // Integer part of x
                uint16_t iy = fy / scale_factor; // Integer part of y
                uint16_t dx = fx % scale_factor; // Fractional part of x
                uint16_t dy = fy % scale_factor; // Fractional part of y

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
                                uint8_t v11, uint16_t dx, uint16_t dy) {
        uint16_t dx_inv = 256 - dx;
        uint16_t dy_inv = 256 - dy;

        uint32_t w00 = (uint32_t)dx_inv * dy_inv;
        uint32_t w10 = (uint32_t)dx * dy_inv;
        uint32_t w01 = (uint32_t)dx_inv * dy;
        uint32_t w11 = (uint32_t)dx * dy;

        uint32_t sum = v00 * w00 + v10 * w10 + v01 * w01 + v11 * w11;

        // Normalize the result by dividing by 65536 (shift right by 16 bits),
        // with rounding
        uint8_t result = (uint8_t)((sum + 32768) >> 16);

        return result;
    }

    void bilinearExpandPowerOf2(CRGB *output, const CRGB *input, uint8_t width,
                                uint8_t height) {
        uint8_t inputWidth = width / 2;
        uint8_t inputHeight = height / 2;
        uint16_t n = mXyMap.getTotal();

        for (uint8_t y = 0; y < height; y++) {
            for (uint8_t x = 0; x < width; x++) {
                // Use 8-bit fixed-point arithmetic with 8 fractional bits
                // (scale factor of 256)
                uint16_t fx =
                    ((uint16_t)x * (inputWidth - 1) * 256) / (width - 1);
                uint16_t fy =
                    ((uint16_t)y * (inputHeight - 1) * 256) / (height - 1);

                uint8_t ix = fx >> 8; // Integer part
                uint8_t iy = fy >> 8;
                uint8_t dx = fx & 0xFF; // Fractional part
                uint8_t dy = fy & 0xFF;

                uint8_t ix1 = (ix + 1 < inputWidth) ? ix + 1 : ix;
                uint8_t iy1 = (iy + 1 < inputHeight) ? iy + 1 : iy;

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

    uint8_t bilinearInterpolatePowerOf2(uint8_t v00, uint8_t v10, uint8_t v01,
                                        uint8_t v11, uint8_t dx, uint8_t dy) {
        uint16_t dx_inv = 256 - dx;
        uint16_t dy_inv = 256 - dy;

        // Weights are 16-bit to handle values up to 256 * 256 = 65536
        uint32_t w00 = (uint32_t)dx_inv * dy_inv; // Max 65536
        uint32_t w10 = (uint32_t)dx * dy_inv;
        uint32_t w01 = (uint32_t)dx_inv * dy;
        uint32_t w11 = (uint32_t)dx * dy;

        // Sum is 32-bit to prevent overflow when multiplying by pixel values
        uint32_t sum = v00 * w00 + v10 * w10 + v01 * w01 + v11 * w11;

        // Normalize the result by dividing by 65536 (shift right by 16 bits),
        // with rounding
        uint8_t result = (uint8_t)((sum + 32768) >> 16);

        return result;
    }

    FxGrid *mDelegate;
    scoped_array<CRGB> mSurface;
};

FASTLED_NAMESPACE_END

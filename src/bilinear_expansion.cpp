/// @file    NoisePlusPalette.hpp
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.hpp

#include <stdint.h>

#include "bilinear_expansion.h"
#include "crgb.h"
#include "namespace.h"
#include "xymap.h"


FASTLED_NAMESPACE_BEGIN


uint8_t bilinearInterpolate(uint8_t v00, uint8_t v10, uint8_t v01,
                            uint8_t v11, uint16_t dx, uint16_t dy);

uint8_t bilinearInterpolatePowerOf2(uint8_t v00, uint8_t v10, uint8_t v01,
                                    uint8_t v11, uint8_t dx, uint8_t dy);


void bilinearExpandArbitrary(const CRGB *input, CRGB *output, uint16_t inputWidth,
                             uint16_t inputHeight, XYMap xyMap) {
    uint16_t n = xyMap.getTotal();
    uint16_t outputWidth = xyMap.getWidth();
    uint16_t outputHeight = xyMap.getHeight();
    const uint16_t scale_factor = 256; // Using 8 bits for the fractional part

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
            result.r = bilinearInterpolate(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g = bilinearInterpolate(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b = bilinearInterpolate(c00.b, c10.b, c01.b, c11.b, dx, dy);

            uint16_t idx = xyMap.mapToIndex(x, y);
            if (idx < n) {
                output[idx] = result;
            }
        }
    }
}
uint8_t bilinearInterpolate(uint8_t v00, uint8_t v10, uint8_t v01, uint8_t v11,
                            uint16_t dx, uint16_t dy) {
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

void bilinearExpandPowerOf2(const CRGB *input, CRGB *output, uint8_t inputWidth, uint8_t inputHeight, XYMap xyMap) {
    uint8_t width = xyMap.getWidth();
    uint8_t height = xyMap.getHeight();
    if (width != xyMap.getWidth() || height != xyMap.getHeight()) {
        // xyMap has width and height that do not fit in an uint16_t.
        return;
    }
    uint16_t n = xyMap.getTotal();

    for (uint8_t y = 0; y < height; y++) {
        for (uint8_t x = 0; x < width; x++) {
            // Use 8-bit fixed-point arithmetic with 8 fractional bits
            // (scale factor of 256)
            uint16_t fx = ((uint16_t)x * (inputWidth - 1) * 256) / (width - 1);
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
            result.r = bilinearInterpolatePowerOf2(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g = bilinearInterpolatePowerOf2(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b = bilinearInterpolatePowerOf2(c00.b, c10.b, c01.b, c11.b, dx, dy);

            uint16_t idx = xyMap.mapToIndex(x, y);
            if (idx < n) {
                output[idx] = result;
            }
        }
    }
}

uint8_t bilinearInterpolatePowerOf2(uint8_t v00, uint8_t v10, uint8_t v01,
                                    uint8_t v11, uint8_t dx, uint8_t dy) {
    uint16_t dx_inv = 256 - dx;  // 0 to 256
    uint16_t dy_inv = 256 - dy;  // 0 to 256

    // Scale down weights to fit into uint16_t
    uint16_t w00 = (dx_inv * dy_inv) >> 8; // Max value 256
    uint16_t w10 = (dx * dy_inv) >> 8;
    uint16_t w01 = (dx_inv * dy) >> 8;
    uint16_t w11 = (dx * dy) >> 8;

    // Sum of weights should be approximately 256
    uint16_t weight_sum = w00 + w10 + w01 + w11;

    // Compute the weighted sum of pixel values
    uint16_t sum = v00 * w00 + v10 * w10 + v01 * w01 + v11 * w11;

    // Normalize the result
    uint8_t result = (sum + (weight_sum >> 1)) / weight_sum;

    return result;
}


/// @file    NoisePlusPalette.hpp
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.hpp

#include "bilinear_expansion.h"
#include "crgb.h"
#include "namespace.h"
#include "xymap.h"
#include <stdint.h>

FASTLED_NAMESPACE_BEGIN

// Floating-point version of bilinear interpolation
uint8_t bilinearInterpolateFloat(uint8_t v00, uint8_t v10, uint8_t v01,
                                 uint8_t v11, float dx, float dy) {
    float dx_inv = 1.0f - dx;
    float dy_inv = 1.0f - dy;

    // Calculate the weights for each corner
    float w00 = dx_inv * dy_inv;
    float w10 = dx * dy_inv;
    float w01 = dx_inv * dy;
    float w11 = dx * dy;

    // Compute the weighted sum
    float sum = v00 * w00 + v10 * w10 + v01 * w01 + v11 * w11;

    // Clamp the result to [0, 255] and round
    uint8_t result = static_cast<uint8_t>(sum + 0.5f);

    return result;
}

// Floating-point version for arbitrary grid sizes
void bilinearExpandArbitraryFloat(const CRGB *input, CRGB *output,
                                  uint16_t inputWidth, uint16_t inputHeight,
                                  XYMap xyMap) {
    uint16_t n = xyMap.getTotal();
    uint16_t outputWidth = xyMap.getWidth();
    uint16_t outputHeight = xyMap.getHeight();

    for (uint16_t y = 0; y < outputHeight; y++) {
        for (uint16_t x = 0; x < outputWidth; x++) {
            // Map output pixel to input grid position
            float fx = static_cast<float>(x) * (inputWidth - 1) / (outputWidth - 1);
            float fy = static_cast<float>(y) * (inputHeight - 1) / (outputHeight - 1);

            uint16_t ix = static_cast<uint16_t>(fx);
            uint16_t iy = static_cast<uint16_t>(fy);
            float dx = fx - ix;
            float dy = fy - iy;

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
            result.r = bilinearInterpolateFloat(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g = bilinearInterpolateFloat(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b = bilinearInterpolateFloat(c00.b, c10.b, c01.b, c11.b, dx, dy);

            uint16_t idx = xyMap.mapToIndex(x, y);
            if (idx < n) {
                output[idx] = result;
            }
        }
    }
}

// Floating-point version for power-of-two grid sizes
void bilinearExpandFloat(const CRGB *input, CRGB *output,
                                 uint8_t inputWidth, uint8_t inputHeight,
                                 XYMap xyMap) {
    uint8_t outputWidth = xyMap.getWidth();
    uint8_t outputHeight = xyMap.getHeight();
    if (outputWidth != xyMap.getWidth() || outputHeight != xyMap.getHeight()) {
        // xyMap has width and height that do not fit in a uint8_t.
        return;
    }
    uint16_t n = xyMap.getTotal();

    for (uint8_t y = 0; y < outputHeight; y++) {
        for (uint8_t x = 0; x < outputWidth; x++) {
            // Map output pixel to input grid position
            float fx = static_cast<float>(x) * (inputWidth - 1) / (outputWidth - 1);
            float fy = static_cast<float>(y) * (inputHeight - 1) / (outputHeight - 1);

            uint8_t ix = static_cast<uint8_t>(fx);
            uint8_t iy = static_cast<uint8_t>(fy);
            float dx = fx - ix;
            float dy = fy - iy;

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
            result.r = bilinearInterpolateFloat(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g = bilinearInterpolateFloat(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b = bilinearInterpolateFloat(c00.b, c10.b, c01.b, c11.b, dx, dy);

            uint16_t idx = xyMap.mapToIndex(x, y);
            if (idx < n) {
                output[idx] = result;
            }
        }
    }
}

FASTLED_NAMESPACE_END


FASTLED_NAMESPACE_END

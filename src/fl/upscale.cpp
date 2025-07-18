/// @file    bilinear_expansion.cpp
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix

#include "fl/stdint.h"

#include "crgb.h"
#include "fl/namespace.h"
#include "fl/upscale.h"
#include "fl/xymap.h"

namespace fl {

u8 bilinearInterpolate(u8 v00, u8 v10, u8 v01, u8 v11,
                            u16 dx, u16 dy);

u8 bilinearInterpolatePowerOf2(u8 v00, u8 v10, u8 v01,
                                    u8 v11, u8 dx, u8 dy);

void upscaleRectangular(const CRGB *input, CRGB *output, u16 inputWidth,
                        u16 inputHeight, u16 outputWidth, u16 outputHeight) {
    const u16 scale_factor = 256; // Using 8 bits for the fractional part

    for (u16 y = 0; y < outputHeight; y++) {
        for (u16 x = 0; x < outputWidth; x++) {
            // Calculate the corresponding position in the input grid
            u32 fx = ((u32)x * (inputWidth - 1) * scale_factor) /
                          (outputWidth - 1);
            u32 fy = ((u32)y * (inputHeight - 1) * scale_factor) /
                          (outputHeight - 1);

            u16 ix = fx / scale_factor; // Integer part of x
            u16 iy = fy / scale_factor; // Integer part of y
            u16 dx = fx % scale_factor; // Fractional part of x
            u16 dy = fy % scale_factor; // Fractional part of y

            u16 ix1 = (ix + 1 < inputWidth) ? ix + 1 : ix;
            u16 iy1 = (iy + 1 < inputHeight) ? iy + 1 : iy;

            // Direct array access - no XY mapping overhead
            u16 i00 = iy * inputWidth + ix;
            u16 i10 = iy * inputWidth + ix1;
            u16 i01 = iy1 * inputWidth + ix;
            u16 i11 = iy1 * inputWidth + ix1;

            CRGB c00 = input[i00];
            CRGB c10 = input[i10];
            CRGB c01 = input[i01];
            CRGB c11 = input[i11];

            CRGB result;
            result.r = bilinearInterpolate(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g = bilinearInterpolate(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b = bilinearInterpolate(c00.b, c10.b, c01.b, c11.b, dx, dy);

            // Direct array access - no XY mapping overhead
            u16 idx = y * outputWidth + x;
            output[idx] = result;
        }
    }
}

void upscaleRectangularPowerOf2(const CRGB *input, CRGB *output, u8 inputWidth,
                                u8 inputHeight, u8 outputWidth, u8 outputHeight) {
    for (u8 y = 0; y < outputHeight; y++) {
        for (u8 x = 0; x < outputWidth; x++) {
            // Use 8-bit fixed-point arithmetic with 8 fractional bits
            // (scale factor of 256)
            u16 fx = ((u16)x * (inputWidth - 1) * 256) / (outputWidth - 1);
            u16 fy = ((u16)y * (inputHeight - 1) * 256) / (outputHeight - 1);

            u8 ix = fx >> 8; // Integer part
            u8 iy = fy >> 8;
            u8 dx = fx & 0xFF; // Fractional part
            u8 dy = fy & 0xFF;

            u8 ix1 = (ix + 1 < inputWidth) ? ix + 1 : ix;
            u8 iy1 = (iy + 1 < inputHeight) ? iy + 1 : iy;

            // Direct array access - no XY mapping overhead
            u16 i00 = iy * inputWidth + ix;
            u16 i10 = iy * inputWidth + ix1;
            u16 i01 = iy1 * inputWidth + ix;
            u16 i11 = iy1 * inputWidth + ix1;

            CRGB c00 = input[i00];
            CRGB c10 = input[i10];
            CRGB c01 = input[i01];
            CRGB c11 = input[i11];

            CRGB result;
            result.r =
                bilinearInterpolatePowerOf2(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g =
                bilinearInterpolatePowerOf2(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b =
                bilinearInterpolatePowerOf2(c00.b, c10.b, c01.b, c11.b, dx, dy);

            // Direct array access - no XY mapping overhead
            u16 idx = y * outputWidth + x;
            output[idx] = result;
        }
    }
}

void upscaleArbitrary(const CRGB *input, CRGB *output, u16 inputWidth,
                      u16 inputHeight, const XYMap& xyMap) {
    u16 n = xyMap.getTotal();
    u16 outputWidth = xyMap.getWidth();
    u16 outputHeight = xyMap.getHeight();
    const u16 scale_factor = 256; // Using 8 bits for the fractional part

    for (u16 y = 0; y < outputHeight; y++) {
        for (u16 x = 0; x < outputWidth; x++) {
            // Calculate the corresponding position in the input grid
            u32 fx = ((u32)x * (inputWidth - 1) * scale_factor) /
                          (outputWidth - 1);
            u32 fy = ((u32)y * (inputHeight - 1) * scale_factor) /
                          (outputHeight - 1);

            u16 ix = fx / scale_factor; // Integer part of x
            u16 iy = fy / scale_factor; // Integer part of y
            u16 dx = fx % scale_factor; // Fractional part of x
            u16 dy = fy % scale_factor; // Fractional part of y

            u16 ix1 = (ix + 1 < inputWidth) ? ix + 1 : ix;
            u16 iy1 = (iy + 1 < inputHeight) ? iy + 1 : iy;

            u16 i00 = iy * inputWidth + ix;
            u16 i10 = iy * inputWidth + ix1;
            u16 i01 = iy1 * inputWidth + ix;
            u16 i11 = iy1 * inputWidth + ix1;

            CRGB c00 = input[i00];
            CRGB c10 = input[i10];
            CRGB c01 = input[i01];
            CRGB c11 = input[i11];

            CRGB result;
            result.r = bilinearInterpolate(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g = bilinearInterpolate(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b = bilinearInterpolate(c00.b, c10.b, c01.b, c11.b, dx, dy);

            u16 idx = xyMap.mapToIndex(x, y);
            if (idx < n) {
                output[idx] = result;
            }
        }
    }
}
u8 bilinearInterpolate(u8 v00, u8 v10, u8 v01, u8 v11,
                            u16 dx, u16 dy) {
    u16 dx_inv = 256 - dx;
    u16 dy_inv = 256 - dy;

    u32 w00 = (u32)dx_inv * dy_inv;
    u32 w10 = (u32)dx * dy_inv;
    u32 w01 = (u32)dx_inv * dy;
    u32 w11 = (u32)dx * dy;

    u32 sum = v00 * w00 + v10 * w10 + v01 * w01 + v11 * w11;

    // Normalize the result by dividing by 65536 (shift right by 16 bits),
    // with rounding
    u8 result = (u8)((sum + 32768) >> 16);

    return result;
}

void upscalePowerOf2(const CRGB *input, CRGB *output, u8 inputWidth,
                     u8 inputHeight, const XYMap& xyMap) {
    u8 width = xyMap.getWidth();
    u8 height = xyMap.getHeight();
    if (width != xyMap.getWidth() || height != xyMap.getHeight()) {
        // xyMap has width and height that do not fit in an u16.
        return;
    }
    u16 n = xyMap.getTotal();

    for (u8 y = 0; y < height; y++) {
        for (u8 x = 0; x < width; x++) {
            // Use 8-bit fixed-point arithmetic with 8 fractional bits
            // (scale factor of 256)
            u16 fx = ((u16)x * (inputWidth - 1) * 256) / (width - 1);
            u16 fy =
                ((u16)y * (inputHeight - 1) * 256) / (height - 1);

            u8 ix = fx >> 8; // Integer part
            u8 iy = fy >> 8;
            u8 dx = fx & 0xFF; // Fractional part
            u8 dy = fy & 0xFF;

            u8 ix1 = (ix + 1 < inputWidth) ? ix + 1 : ix;
            u8 iy1 = (iy + 1 < inputHeight) ? iy + 1 : iy;

            u16 i00 = iy * inputWidth + ix;
            u16 i10 = iy * inputWidth + ix1;
            u16 i01 = iy1 * inputWidth + ix;
            u16 i11 = iy1 * inputWidth + ix1;

            CRGB c00 = input[i00];
            CRGB c10 = input[i10];
            CRGB c01 = input[i01];
            CRGB c11 = input[i11];

            CRGB result;
            result.r =
                bilinearInterpolatePowerOf2(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g =
                bilinearInterpolatePowerOf2(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b =
                bilinearInterpolatePowerOf2(c00.b, c10.b, c01.b, c11.b, dx, dy);

            u16 idx = xyMap.mapToIndex(x, y);
            if (idx < n) {
                output[idx] = result;
            }
        }
    }
}

u8 bilinearInterpolatePowerOf2(u8 v00, u8 v10, u8 v01,
                                    u8 v11, u8 dx, u8 dy) {
    u16 dx_inv = 256 - dx; // 0 to 256
    u16 dy_inv = 256 - dy; // 0 to 256

    // Scale down weights to fit into u16
    u16 w00 = (dx_inv * dy_inv) >> 8; // Max value 256
    u16 w10 = (dx * dy_inv) >> 8;
    u16 w01 = (dx_inv * dy) >> 8;
    u16 w11 = (dx * dy) >> 8;

    // Sum of weights should be approximately 256
    u16 weight_sum = w00 + w10 + w01 + w11;

    // Compute the weighted sum of pixel values
    u16 sum = v00 * w00 + v10 * w10 + v01 * w01 + v11 * w11;

    // Normalize the result
    u8 result = (sum + (weight_sum >> 1)) / weight_sum;

    return result;
}

// Floating-point version of bilinear interpolation
u8 upscaleFloat(u8 v00, u8 v10, u8 v01,
                                 u8 v11, float dx, float dy) {
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
    u8 result = static_cast<u8>(sum + 0.5f);

    return result;
}

// Floating-point version for arbitrary grid sizes
void upscaleArbitraryFloat(const CRGB *input, CRGB *output, u16 inputWidth,
                           u16 inputHeight, const XYMap& xyMap) {
    u16 n = xyMap.getTotal();
    u16 outputWidth = xyMap.getWidth();
    u16 outputHeight = xyMap.getHeight();

    for (u16 y = 0; y < outputHeight; y++) {
        for (u16 x = 0; x < outputWidth; x++) {
            // Map output pixel to input grid position
            float fx =
                static_cast<float>(x) * (inputWidth - 1) / (outputWidth - 1);
            float fy =
                static_cast<float>(y) * (inputHeight - 1) / (outputHeight - 1);

            u16 ix = static_cast<u16>(fx);
            u16 iy = static_cast<u16>(fy);
            float dx = fx - ix;
            float dy = fy - iy;

            u16 ix1 = (ix + 1 < inputWidth) ? ix + 1 : ix;
            u16 iy1 = (iy + 1 < inputHeight) ? iy + 1 : iy;

            u16 i00 = iy * inputWidth + ix;
            u16 i10 = iy * inputWidth + ix1;
            u16 i01 = iy1 * inputWidth + ix;
            u16 i11 = iy1 * inputWidth + ix1;

            CRGB c00 = input[i00];
            CRGB c10 = input[i10];
            CRGB c01 = input[i01];
            CRGB c11 = input[i11];

            CRGB result;
            result.r =
                upscaleFloat(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g =
                upscaleFloat(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b =
                upscaleFloat(c00.b, c10.b, c01.b, c11.b, dx, dy);

            u16 idx = xyMap.mapToIndex(x, y);
            if (idx < n) {
                output[idx] = result;
            }
        }
    }
}

// Floating-point version for power-of-two grid sizes
void upscaleFloat(const CRGB *input, CRGB *output, u8 inputWidth,
                  u8 inputHeight, const XYMap& xyMap) {
    u8 outputWidth = xyMap.getWidth();
    u8 outputHeight = xyMap.getHeight();
    if (outputWidth != xyMap.getWidth() || outputHeight != xyMap.getHeight()) {
        // xyMap has width and height that do not fit in a u8.
        return;
    }
    u16 n = xyMap.getTotal();

    for (u8 y = 0; y < outputHeight; y++) {
        for (u8 x = 0; x < outputWidth; x++) {
            // Map output pixel to input grid position
            float fx =
                static_cast<float>(x) * (inputWidth - 1) / (outputWidth - 1);
            float fy =
                static_cast<float>(y) * (inputHeight - 1) / (outputHeight - 1);

            u8 ix = static_cast<u8>(fx);
            u8 iy = static_cast<u8>(fy);
            float dx = fx - ix;
            float dy = fy - iy;

            u8 ix1 = (ix + 1 < inputWidth) ? ix + 1 : ix;
            u8 iy1 = (iy + 1 < inputHeight) ? iy + 1 : iy;

            u16 i00 = iy * inputWidth + ix;
            u16 i10 = iy * inputWidth + ix1;
            u16 i01 = iy1 * inputWidth + ix;
            u16 i11 = iy1 * inputWidth + ix1;

            CRGB c00 = input[i00];
            CRGB c10 = input[i10];
            CRGB c01 = input[i01];
            CRGB c11 = input[i11];

            CRGB result;
            result.r =
                upscaleFloat(c00.r, c10.r, c01.r, c11.r, dx, dy);
            result.g =
                upscaleFloat(c00.g, c10.g, c01.g, c11.g, dx, dy);
            result.b =
                upscaleFloat(c00.b, c10.b, c01.b, c11.b, dx, dy);

            u16 idx = xyMap.mapToIndex(x, y);
            if (idx < n) {
                output[idx] = result;
            }
        }
    }
}

} // namespace fl


#include "bilinear_compression.h"

#include "crgb.h"
#include "fl/math_macros.h"

namespace fl {

void downscaleBilinear(const CRGB *src, uint16_t srcWidth, uint16_t srcHeight,
                       CRGB *dst, uint16_t dstWidth, uint16_t dstHeight) {
    // Use 8 bits for fixed-point fractional precision.
    const uint16_t SHIFT = 8;
    const uint16_t FP_ONE = 1 << SHIFT; // 256 in fixed-point
    const uint16_t WEIGHT_SHIFT =
        SHIFT * 2; // 16 bits (for the product of two fixed-point numbers)
    const uint16_t rounding = 1 << (WEIGHT_SHIFT - 1);

    // Compute scale factors in fixed-point: factor = (srcDimension << SHIFT) /
    // dstDimension.
    uint16_t scaleX = (srcWidth << SHIFT) / dstWidth;
    uint16_t scaleY = (srcHeight << SHIFT) / dstHeight;

    // Loop over each pixel in the destination image.
    for (int y = 0; y < dstHeight; ++y) {
        // Compute the corresponding source y coordinate in fixed-point.
        int srcYFixed = y * scaleY;
        int y0 = srcYFixed >> SHIFT;          // integer part for row index
        int yFrac = srcYFixed & (FP_ONE - 1); // fractional part
        int y1 =
            MIN(y0 + 1, srcHeight - 1); // ensure we don't exceed bounds

        for (int x = 0; x < dstWidth; ++x) {
            // Compute the corresponding source x coordinate in fixed-point.
            int srcXFixed = x * scaleX;
            int x0 = srcXFixed >> SHIFT; // integer part for column index
            int xFrac = srcXFixed & (FP_ONE - 1); // fractional part
            int x1 = MIN(x0 + 1, srcWidth - 1);

            // Compute the weights for the four neighboring pixels.
            int w00 = (FP_ONE - xFrac) * (FP_ONE - yFrac);
            int w10 = xFrac * (FP_ONE - yFrac);
            int w01 = (FP_ONE - xFrac) * yFrac;
            int w11 = xFrac * yFrac;

            // Apply fixed-point weighted sum for each color channel.
            int r = (w00 * src[y0 * srcWidth + x0].r +
                     w10 * src[y0 * srcWidth + x1].r +
                     w01 * src[y1 * srcWidth + x0].r +
                     w11 * src[y1 * srcWidth + x1].r + rounding) >>
                    WEIGHT_SHIFT;

            int g = (w00 * src[y0 * srcWidth + x0].g +
                     w10 * src[y0 * srcWidth + x1].g +
                     w01 * src[y1 * srcWidth + x0].g +
                     w11 * src[y1 * srcWidth + x1].g + rounding) >>
                    WEIGHT_SHIFT;

            int b = (w00 * src[y0 * srcWidth + x0].b +
                     w10 * src[y0 * srcWidth + x1].b +
                     w01 * src[y1 * srcWidth + x0].b +
                     w11 * src[y1 * srcWidth + x1].b + rounding) >>
                    WEIGHT_SHIFT;

            // Store the computed pixel in the destination image.
            dst[y * dstWidth + x] = CRGB(static_cast<unsigned char>(r),
                                         static_cast<unsigned char>(g),
                                         static_cast<unsigned char>(b));
        }
    }
}

} // namespace fl

// BETA - NOT TESTED!!!
// VIBE CODED WITH AI

#include "fl/downscale.h"

#include "crgb.h"
#include "fl/assert.h"
#include "fl/math_macros.h"
#include "fl/xymap.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshift-count-overflow"

namespace fl {

void downscaleHalf(const CRGB *src, uint16_t srcWidth, uint16_t srcHeight,
                   CRGB *dst) {
    uint16_t dstWidth = srcWidth / 2;
    uint16_t dstHeight = srcHeight / 2;

    for (uint16_t y = 0; y < dstHeight; ++y) {
        for (uint16_t x = 0; x < dstWidth; ++x) {
            // Map to top-left of the 2x2 block in source
            uint16_t srcX = x * 2;
            uint16_t srcY = y * 2;

            // Fetch 2x2 block
            const CRGB &p00 = src[(srcY)*srcWidth + (srcX)];
            const CRGB &p10 = src[(srcY)*srcWidth + (srcX + 1)];
            const CRGB &p01 = src[(srcY + 1) * srcWidth + (srcX)];
            const CRGB &p11 = src[(srcY + 1) * srcWidth + (srcX + 1)];

            // Average each color channel
            uint16_t r =
                (p00.r + p10.r + p01.r + p11.r + 2) / 4; // +2 for rounding
            uint16_t g = (p00.g + p10.g + p01.g + p11.g + 2) / 4;
            uint16_t b = (p00.b + p10.b + p01.b + p11.b + 2) / 4;

            // Store result
            dst[y * dstWidth + x] = CRGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
    }
}

void downscaleHalf(const CRGB *src, const XYMap &srcXY, CRGB *dst,
                   const XYMap &dstXY) {
    uint16_t dstWidth = dstXY.getWidth();
    uint16_t dstHeight = dstXY.getHeight();

    FASTLED_ASSERT(srcXY.getWidth() == dstXY.getWidth() * 2,
                   "Source width must be double the destination width");
    FASTLED_ASSERT(srcXY.getHeight() == dstXY.getHeight() * 2,
                   "Source height must be double the destination height");

    for (uint16_t y = 0; y < dstHeight; ++y) {
        for (uint16_t x = 0; x < dstWidth; ++x) {
            // Map to top-left of the 2x2 block in source
            uint16_t srcX = x * 2;
            uint16_t srcY = y * 2;

            // Fetch 2x2 block
            const CRGB &p00 = src[srcXY.mapToIndex(srcX, srcY)];
            const CRGB &p10 = src[srcXY.mapToIndex(srcX + 1, srcY)];
            const CRGB &p01 = src[srcXY.mapToIndex(srcX, srcY + 1)];
            const CRGB &p11 = src[srcXY.mapToIndex(srcX + 1, srcY + 1)];

            // Average each color channel
            uint16_t r =
                (p00.r + p10.r + p01.r + p11.r + 2) / 4; // +2 for rounding
            uint16_t g = (p00.g + p10.g + p01.g + p11.g + 2) / 4;
            uint16_t b = (p00.b + p10.b + p01.b + p11.b + 2) / 4;

            // Store result
            dst[dstXY.mapToIndex(x, y)] =
                CRGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
    }
}

void downscaleArbitrary(const CRGB *src, const XYMap &srcXY, CRGB *dst,
                        const XYMap &dstXY) {
    const uint16_t srcWidth = srcXY.getWidth();
    const uint16_t srcHeight = srcXY.getHeight();
    const uint16_t dstWidth = dstXY.getWidth();
    const uint16_t dstHeight = dstXY.getHeight();

    const uint32_t FP_ONE = 256; // Q8.8 fixed-point multiplier

    FASTLED_ASSERT(dstWidth <= srcWidth,
                   "Destination width must be <= source width");
    FASTLED_ASSERT(dstHeight <= srcHeight,
                   "Destination height must be <= source height");

    for (uint16_t dy = 0; dy < dstHeight; ++dy) {
        // Fractional boundaries in Q8.8
        uint32_t dstY0 = (dy * srcHeight * FP_ONE) / dstHeight;
        uint32_t dstY1 = ((dy + 1) * srcHeight * FP_ONE) / dstHeight;

        for (uint16_t dx = 0; dx < dstWidth; ++dx) {
            uint32_t dstX0 = (dx * srcWidth * FP_ONE) / dstWidth;
            uint32_t dstX1 = ((dx + 1) * srcWidth * FP_ONE) / dstWidth;

            uint64_t rSum = 0, gSum = 0, bSum = 0;
            uint32_t totalWeight = 0;

            // Find covered source pixels
            uint16_t srcY_start = dstY0 / FP_ONE;
            uint16_t srcY_end = (dstY1 + FP_ONE - 1) / FP_ONE; // ceil

            uint16_t srcX_start = dstX0 / FP_ONE;
            uint16_t srcX_end = (dstX1 + FP_ONE - 1) / FP_ONE; // ceil

            for (uint16_t sy = srcY_start; sy < srcY_end; ++sy) {
                // Calculate vertical overlap in Q8.8
                uint32_t sy0 = sy * FP_ONE;
                uint32_t sy1 = (sy + 1) * FP_ONE;
                uint32_t y_overlap = MIN(dstY1, sy1) - MAX(dstY0, sy0);
                if (y_overlap == 0)
                    continue;

                for (uint16_t sx = srcX_start; sx < srcX_end; ++sx) {
                    uint32_t sx0 = sx * FP_ONE;
                    uint32_t sx1 = (sx + 1) * FP_ONE;
                    uint32_t x_overlap = MIN(dstX1, sx1) - MAX(dstX0, sx0);
                    if (x_overlap == 0)
                        continue;

                    uint32_t weight = (x_overlap * y_overlap + (FP_ONE >> 1)) >>
                                      8; // Q8.8 * Q8.8 → Q16.16 → Q8.8

                    const CRGB &p = src[srcXY.mapToIndex(sx, sy)];
                    rSum += p.r * weight;
                    gSum += p.g * weight;
                    bSum += p.b * weight;
                    totalWeight += weight;
                }
            }

            // Final division, rounding
            uint8_t r =
                totalWeight ? (rSum + (totalWeight >> 1)) / totalWeight : 0;
            uint8_t g =
                totalWeight ? (gSum + (totalWeight >> 1)) / totalWeight : 0;
            uint8_t b =
                totalWeight ? (bSum + (totalWeight >> 1)) / totalWeight : 0;

            dst[dstXY.mapToIndex(dx, dy)] = CRGB(r, g, b);
        }
    }
}

void downscale(const CRGB *src, const XYMap &srcXY, CRGB *dst,
               const XYMap &dstXY) {
    uint16_t srcWidth = srcXY.getWidth();
    uint16_t srcHeight = srcXY.getHeight();
    uint16_t dstWidth = dstXY.getWidth();
    uint16_t dstHeight = dstXY.getHeight();

    FASTLED_ASSERT(dstWidth <= srcWidth,
                   "Destination width must be <= source width");
    FASTLED_ASSERT(dstHeight <= srcHeight,
                   "Destination height must be <= source height");
    const bool destination_is_half_of_source =
        (dstWidth * 2 == srcWidth) && (dstHeight * 2 == srcHeight);
    // Attempt to use the downscaleHalf function if the destination is half the
    // size of the source.
    if (destination_is_half_of_source) {
        const bool both_rectangles = (srcXY.getType() == XYMap::kLineByLine) &&
                                     (dstXY.getType() == XYMap::kLineByLine);
        if (both_rectangles) {
            // If both source and destination are rectangular, we can use the
            // optimized version
            downscaleHalf(src, srcWidth, srcHeight, dst);
        } else {
            // Otherwise, we need to use the mapped version
            downscaleHalf(src, srcXY, dst, dstXY);
        }
        return;
    }

    downscaleArbitrary(src, srcXY, dst, dstXY);
}

} // namespace fl

#pragma GCC diagnostic pop

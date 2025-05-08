
// BETA - NOT TESTED!!!
// VIBE CODED WITH AI


#include "fl/downscale.h"

#include "crgb.h"
#include "fl/math_macros.h"
#include "fl/xymap.h"
#include "fl/assert.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshift-count-overflow"

namespace fl {

void downscaleHalf(const CRGB *src, uint16_t srcWidth, uint16_t srcHeight, CRGB *dst) {
    uint16_t dstWidth = srcWidth / 2;
    uint16_t dstHeight = srcHeight / 2;

    for (uint16_t y = 0; y < dstHeight; ++y) {
        for (uint16_t x = 0; x < dstWidth; ++x) {
            // Map to top-left of the 2x2 block in source
            uint16_t srcX = x * 2;
            uint16_t srcY = y * 2;

            // Fetch 2x2 block
            const CRGB &p00 = src[(srcY) * srcWidth + (srcX)];
            const CRGB &p10 = src[(srcY) * srcWidth + (srcX + 1)];
            const CRGB &p01 = src[(srcY + 1) * srcWidth + (srcX)];
            const CRGB &p11 = src[(srcY + 1) * srcWidth + (srcX + 1)];

            // Average each color channel
            uint16_t r = (p00.r + p10.r + p01.r + p11.r + 2) / 4; // +2 for rounding
            uint16_t g = (p00.g + p10.g + p01.g + p11.g + 2) / 4;
            uint16_t b = (p00.b + p10.b + p01.b + p11.b + 2) / 4;

            // Store result
            dst[y * dstWidth + x] = CRGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
    }
}

void downscaleHalf(const CRGB* src, const XYMap& srcXY, CRGB* dst, const XYMap& dstXY) {
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
            uint16_t r = (p00.r + p10.r + p01.r + p11.r + 2) / 4; // +2 for rounding
            uint16_t g = (p00.g + p10.g + p01.g + p11.g + 2) / 4;
            uint16_t b = (p00.b + p10.b + p01.b + p11.b + 2) / 4;

            // Store result
            dst[dstXY.mapToIndex(x, y)] = CRGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
    }
}

void downscale(const CRGB* src, const XYMap& srcXY, CRGB* dst, const XYMap& dstXY) {
    uint16_t srcWidth = srcXY.getWidth();
    uint16_t srcHeight = srcXY.getHeight();
    uint16_t dstWidth = dstXY.getWidth();
    uint16_t dstHeight = dstXY.getHeight();

    FASTLED_ASSERT(dstWidth <= srcWidth, "Destination width must be <= source width");
    FASTLED_ASSERT(dstHeight <= srcHeight, "Destination height must be <= source height");

    #ifndef FASTLED_TESTING
    // Attempt to use the downscaleHalf function if the destination is half the size of the source
    // We don't do this in testing mode so that we can exactly test the bilinear downscaling.
    if (dstWidth * 2 == srcWidth && dstHeight * 2 == srcHeight) {
        const bool both_rectangles = (srcXY.getType() == XYMap::kLineByLine) && (dstXY.getType() == XYMap::kLineByLine);
        if (both_rectangles) {
            // If both source and destination are rectangular, we can use the optimized version
            downscaleHalf(src, srcWidth, srcHeight, dst);
        } else {
            // Otherwise, we need to use the mapped version
            downscaleHalf(src, srcXY, dst, dstXY);
        }
        return;
    }
    #endif

    for (uint16_t dy = 0; dy < dstHeight; ++dy) {
        // Calculate source region for this destination row
        uint16_t srcY0 = (dy * srcHeight) / dstHeight;
        uint16_t srcY1 = ((dy + 1) * srcHeight) / dstHeight;
        if (srcY1 > srcHeight) srcY1 = srcHeight;

        for (uint16_t dx = 0; dx < dstWidth; ++dx) {
            // Calculate source region for this destination column
            uint16_t srcX0 = (dx * srcWidth) / dstWidth;
            uint16_t srcX1 = ((dx + 1) * srcWidth) / dstWidth;
            if (srcX1 > srcWidth) srcX1 = srcWidth;

            uint32_t rSum = 0, gSum = 0, bSum = 0;
            uint16_t count = 0;

            for (uint16_t sy = srcY0; sy < srcY1; ++sy) {
                for (uint16_t sx = srcX0; sx < srcX1; ++sx) {
                    const CRGB& p = src[srcXY.mapToIndex(sx, sy)];
                    rSum += p.r;
                    gSum += p.g;
                    bSum += p.b;
                    ++count;
                }
            }

            // Add half the count for rounding
            uint8_t r = (rSum + count / 2) / count;
            uint8_t g = (gSum + count / 2) / count;
            uint8_t b = (bSum + count / 2) / count;

            dst[dstXY.mapToIndex(dx, dy)] = CRGB(r, g, b);
        }
    }
}

} // namespace fl


#pragma GCC diagnostic pop

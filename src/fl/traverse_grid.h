#pragma once

/*
Amanatides–Woo grid traversal algorithm in C++.
Given a line defined by two points, this algorithm traverses the grid cells
intersecting the line and calls a visitor function for each cell.
*/

#include "fl/math.h"
#include "fl/point.h"
#include "fl/int.h"

namespace fl {

/// @brief Traverse a grid segment by selecting the cells that are crossed. This
/// version will select the fastest integer implementation based on the length
/// of the segment. Most of the time it will call traverseGridSegment16() since
/// segment spans are typically < 256 pixels.
/// @tparam GridVisitor
/// @param start start point
/// @param end  end point
/// @param visitor called for each cell visited.
/// @details Fully tested.
template <typename GridVisitor>
void traverseGridSegment(const vec2f &start, const vec2f &end,
                         GridVisitor &visitor);

/// @brief Traverse a grid segment using fixed-point 8.8 arithmetic.
/// @tparam GridVisitor
/// @param start start point
/// @param end  end point
/// @param visitor called for each cell visited.
/// @details UNTESTED!!!!
template <typename GridVisitor>
void traverseGridSegment16(const vec2f &start, const vec2f &end,
                           GridVisitor &visitor);

// @brief Traverse a grid segment using fixed-point 24.8 arithmetic.
/// @tparam GridVisitor
/// @param start start point
/// @param end end point
/// @param visitor called for each cell visited.
/// @details UNTESTED!!!!
template <typename GridVisitor>
void traverseGridSegment32(const vec2f &start, const vec2f &end,
                           GridVisitor &visitor);

/// @brief Traverse a grid segment using floating point arithmetic. Useful for
/// testing.
/// @tparam GridVisitor
/// @param start start point
/// @param end  end point
/// @param visitor called for each cell visited.
/// @details Fully tested.
template <typename GridVisitor>
void traverseGridSegmentFloat(const vec2f &start, const vec2f &end,
                              GridVisitor &visitor);

////////////////////////// IMPLEMENTATION DETAILS //////////////////////////

/// @brief Traverse a grid segment using fixed-point 8.8 arithmetic.
/// @tparam GridVisitor
/// @param start start point
/// @param end  end point
/// @param visitor called for each cell visited.
/// @details Fully tested.
template <typename GridVisitor>
inline void traverseGridSegmentFloat(const vec2f &start, const vec2f &end,
                                     GridVisitor &visitor) {
    int x0 = static_cast<int>(fl::floor(start.x));
    int y0 = static_cast<int>(fl::floor(start.y));
    int x1 = static_cast<int>(fl::floor(end.x));
    int y1 = static_cast<int>(fl::floor(end.y));

    int stepX = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
    int stepY = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;

    float dx = end.x - start.x;
    float dy = end.y - start.y;

    float tDeltaX = (dx != 0.0f) ? ABS(1.0f / dx) : FLT_MAX;
    float tDeltaY = (dy != 0.0f) ? ABS(1.0f / dy) : FLT_MAX;

    float nextX = (stepX > 0) ? (fl::floor(start.x) + 1) : fl::floor(start.x);
    float nextY = (stepY > 0) ? (fl::floor(start.y) + 1) : fl::floor(start.y);

    float tMaxX = (dx != 0.0f) ? ABS((nextX - start.x) / dx) : FLT_MAX;
    float tMaxY = (dy != 0.0f) ? ABS((nextY - start.y) / dy) : FLT_MAX;

    float maxT = 1.0f;

    int currentX = x0;
    int currentY = y0;

    while (true) {
        visitor.visit(currentX, currentY);
        float t = MIN(tMaxX, tMaxY);
        if (t > maxT)
            break;

        if (tMaxX < tMaxY) {
            tMaxX += tDeltaX;
            currentX += stepX;
        } else {
            tMaxY += tDeltaY;
            currentY += stepY;
        }
    }

    // Ensure the end cell (x1, y1) is visited at least once
    if (currentX != x1 || currentY != y1) {
        visitor.visit(x1, y1);
    }
}

/// @brief Traverse a grid segment using fixed-point 8.8 arithmetic.
/// @tparam GridVisitor
/// @param start start point
/// @param end  end point
/// @param visitor called for each cell visited.
/// @details UNTESTED!!!!
template <typename GridVisitor>
inline void traverseGridSegment16(const vec2f &start, const vec2f &end,
                                  GridVisitor &visitor) {
    const i16 FP_SHIFT = 8;
    const i16 FP_ONE = 1 << FP_SHIFT;
    // const i16 FP_MASK = FP_ONE - 1;

    // Convert to fixed-point (Q8.8), signed
    i16 startX_fp = static_cast<i16>(start.x * FP_ONE);
    i16 startY_fp = static_cast<i16>(start.y * FP_ONE);
    i16 endX_fp = static_cast<i16>(end.x * FP_ONE);
    i16 endY_fp = static_cast<i16>(end.y * FP_ONE);

    i16 x0 = startX_fp >> FP_SHIFT;
    i16 y0 = startY_fp >> FP_SHIFT;
    i16 x1 = endX_fp >> FP_SHIFT;
    i16 y1 = endY_fp >> FP_SHIFT;

    i16 stepX = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
    i16 stepY = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;

    i16 deltaX_fp = endX_fp - startX_fp;
    i16 deltaY_fp = endY_fp - startY_fp;

    u16 absDeltaX_fp =
        (deltaX_fp != 0) ? static_cast<u16>(
                               ABS((i32(FP_ONE) << FP_SHIFT) / deltaX_fp))
                         : UINT16_MAX;
    u16 absDeltaY_fp =
        (deltaY_fp != 0) ? static_cast<u16>(
                               ABS((i32(FP_ONE) << FP_SHIFT) / deltaY_fp))
                         : UINT16_MAX;

    i16 nextX_fp = (stepX > 0) ? ((x0 + 1) << FP_SHIFT) : (x0 << FP_SHIFT);
    i16 nextY_fp = (stepY > 0) ? ((y0 + 1) << FP_SHIFT) : (y0 << FP_SHIFT);

    u16 tMaxX_fp =
        (deltaX_fp != 0)
            ? static_cast<u16>(
                  ABS(i32(nextX_fp - startX_fp)) * absDeltaX_fp >> FP_SHIFT)
            : UINT16_MAX;
    u16 tMaxY_fp =
        (deltaY_fp != 0)
            ? static_cast<u16>(
                  ABS(i32(nextY_fp - startY_fp)) * absDeltaY_fp >> FP_SHIFT)
            : UINT16_MAX;

    const u16 maxT_fp = FP_ONE;

    i16 currentX = x0;
    i16 currentY = y0;

    while (true) {
        visitor.visit(currentX, currentY);

        u16 t_fp = (tMaxX_fp < tMaxY_fp) ? tMaxX_fp : tMaxY_fp;
        if (t_fp > maxT_fp)
            break;

        if (tMaxX_fp < tMaxY_fp) {
            tMaxX_fp += absDeltaX_fp;
            currentX += stepX;
        } else {
            tMaxY_fp += absDeltaY_fp;
            currentY += stepY;
        }
    }

    // Ensure the end cell (x1, y1) is visited at least once
    if (currentX != x1 || currentY != y1) {
        visitor.visit(x1, y1);
    }
}

// @brief Traverse a grid segment using fixed-point 24.8 arithmetic.
/// @tparam GridVisitor
/// @param start start point
/// @param end end point
/// @param visitor called for each cell visited.
/// @details UNTESTED!!!!
template <typename GridVisitor>
inline void traverseGridSegment32(const vec2f &start, const vec2f &end,
                                  GridVisitor &visitor) {
    const i32 FP_SHIFT = 8;
    const i32 FP_ONE = 1 << FP_SHIFT;
    // const i32 FP_MASK = FP_ONE - 1;

    // Convert to fixed-point (Q24.8) signed
    i32 startX_fp = static_cast<i32>(start.x * FP_ONE);
    i32 startY_fp = static_cast<i32>(start.y * FP_ONE);
    i32 endX_fp = static_cast<i32>(end.x * FP_ONE);
    i32 endY_fp = static_cast<i32>(end.y * FP_ONE);

    i32 x0 = startX_fp >> FP_SHIFT;
    i32 y0 = startY_fp >> FP_SHIFT;
    i32 x1 = endX_fp >> FP_SHIFT;
    i32 y1 = endY_fp >> FP_SHIFT;

    i32 stepX = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
    i32 stepY = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;

    i32 deltaX_fp = endX_fp - startX_fp;
    i32 deltaY_fp = endY_fp - startY_fp;

    u32 absDeltaX_fp =
        (deltaX_fp != 0) ? static_cast<u32>(
                               ABS((fl::i64(FP_ONE) << FP_SHIFT) / deltaX_fp))
                         : UINT32_MAX;
    u32 absDeltaY_fp =
        (deltaY_fp != 0) ? static_cast<u32>(
                               ABS((fl::i64(FP_ONE) << FP_SHIFT) / deltaY_fp))
                         : UINT32_MAX;

    i32 nextX_fp = (stepX > 0) ? ((x0 + 1) << FP_SHIFT) : (x0 << FP_SHIFT);
    i32 nextY_fp = (stepY > 0) ? ((y0 + 1) << FP_SHIFT) : (y0 << FP_SHIFT);

    u32 tMaxX_fp =
        (deltaX_fp != 0)
            ? static_cast<u32>(
                  ABS(fl::i64(nextX_fp - startX_fp)) * absDeltaX_fp >> FP_SHIFT)
            : UINT32_MAX;
    u32 tMaxY_fp =
        (deltaY_fp != 0)
            ? static_cast<u32>(
                  ABS(fl::i64(nextY_fp - startY_fp)) * absDeltaY_fp >> FP_SHIFT)
            : UINT32_MAX;

    const u32 maxT_fp = FP_ONE;

    i32 currentX = x0;
    i32 currentY = y0;

    while (true) {
        visitor.visit(currentX, currentY);

        u32 t_fp = (tMaxX_fp < tMaxY_fp) ? tMaxX_fp : tMaxY_fp;
        if (t_fp > maxT_fp)
            break;

        if (tMaxX_fp < tMaxY_fp) {
            tMaxX_fp += absDeltaX_fp;
            currentX += stepX;
        } else {
            tMaxY_fp += absDeltaY_fp;
            currentY += stepY;
        }
    }

    if (currentX != x1 || currentY != y1) {
        visitor.visit(x1, y1);
    }
}

template <typename GridVisitor>
inline void traverseGridSegment(const vec2f &start, const vec2f &end,
                                GridVisitor &visitor) {
    float dx = ABS(end.x - start.x);
    float dy = ABS(end.y - start.y);
    float maxRange = MAX(dx, dy);

    // if (maxRange < 256.0f) {
    //     // Use Q8.8 (16-bit signed) if within ±127
    //     traverseGridSegment16(start, end, visitor);
    // }
    // else if (maxRange < 16777216.0f) {
    //     // Use Q24.8 (32-bit signed) if within ±8 million
    //     traverseGridSegment32(start, end, visitor);
    // }
    // else {
    //     // Fall back to floating-point
    //     traverseGridSegment(start, end, visitor);
    // }

    if (maxRange < 256.0f) {
        // Use Q8.8 (16-bit signed) if within ±127
        traverseGridSegment16(start, end, visitor);
    } else {
        // Use Q24.8 (32-bit signed) if within ±8 million
        traverseGridSegment32(start, end, visitor);
    }
}

} // namespace fl

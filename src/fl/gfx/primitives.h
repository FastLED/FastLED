#pragma once

#include "fl/gfx/canvas.h"  // IWYU pragma: keep
#include "fl/gfx/detail/distance_lut.h"  // IWYU pragma: keep
#include "fl/gfx/detail/integer_math.h"  // IWYU pragma: keep
#include "fl/stl/fixed_point/s16x16.h"
#include "fl/stl/math.h"
#include "fl/scale8.h"

namespace fl {
namespace gfx {

/// sqrt is provided by fl::sqrt overloads in math.h for:
/// - All floating-point types (float, double)
/// - All fixed-point types (s8x8, s12x4, s16x16, s24x8, u8x8, u12x4, u16x16, u24x8)
/// - Integer types (via cast to float)
/// Using: using fl::sqrt; (in drawStrokeLine template to find overloads via ADL)

/// Convert a normalized [0,1] Coord value to u8 (0-255) for nscale8.
/// Uses integer arithmetic for fixed-point types; float for float/double.
/// Generic: for fixed-point types (those with .to_int())
template<typename T>
inline fl::u8 coordToU8(T alpha) {
    return (fl::u8)fl::clamp((alpha * T(255.0f) + T(0.5f)).to_int(), 0, 255);
}

// Specialization for float
template<>
inline fl::u8 coordToU8<float>(float alpha) {
    return (fl::u8)fl::round(alpha * 255.0f);
}

// Specialization for double
template<>
inline fl::u8 coordToU8<double>(double alpha) {
    return (fl::u8)fl::round(alpha * 255.0);
}

// Specialization for int
template<>
inline fl::u8 coordToU8<int>(int alpha) {
    return (fl::u8)fl::clamp(alpha, 0, 255);
}

// Specialization for s16x16: uses from_raw, no float at all
template<>
inline fl::u8 coordToU8<fl::s16x16>(fl::s16x16 alpha) {
    constexpr fl::s16x16 c255 = fl::s16x16::from_raw(255 * fl::s16x16::SCALE);
    constexpr fl::s16x16 half = fl::s16x16::from_raw(1 << 15);
    return (fl::u8)fl::clamp((alpha * c255 + half).to_int(), 0, 255);
}

/// Convert an integer to Coord without float intermediate.
/// For s16x16: uses from_raw (pure integer shift). For float: implicit cast.
template<typename T>
inline T fromInt(int n) { return T(static_cast<float>(n)); }

template<>
inline fl::s16x16 fromInt<fl::s16x16>(int n) {
    return fl::s16x16::from_raw(n * fl::s16x16::SCALE);
}

/// Convert rational p/q to Coord without float.
/// For s16x16: integer multiply+divide. For float: float divide.
template<typename T>
inline T fromFrac(int p, int q) { return T(static_cast<float>(p) / static_cast<float>(q)); }

template<>
inline fl::s16x16 fromFrac<fl::s16x16>(int p, int q) {
    return fl::s16x16::from_raw((p * fl::s16x16::SCALE) / q);
}

/// Helper to convert any coordinate type to int
/// Supports: s16x16 (via to_int()), float, int, and other arithmetic types
template<typename T>
inline int toInt(const T& val) {
    return val.to_int();  // For s16x16 and similar
}

// Specialization for float
template<>
inline int toInt<float>(const float& val) {
    return static_cast<int>(val);
}

// Specialization for int (passthrough)
template<>
inline int toInt<int>(const int& val) {
    return val;
}

// Specialization for double
template<>
inline int toInt<double>(const double& val) {
    return static_cast<int>(val);
}

/// Internal helper: add pixel to rectangular buffer with bounds checking
/// Direct row-major indexing: pixels[y * width + x]
template<typename PixelT>
inline void addPixelToBuffer(PixelT* pixels, int width, int height,
                              int x, int y, const PixelT& color) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        pixels[y * width + x] += color;
    }
}

namespace detail {

// void_t for C++11 SFINAE (detects member existence)
template<typename...> struct voider { typedef void type; };

// Trait: does this Coord type need division instead of reciprocal multiply?
// Default (float, double, int — no FRAC_BITS member): use multiply
template<typename Coord, typename = void>
struct needs_division : fl::false_type {};

// Fixed-point types with FRAC_BITS < 16: reciprocal too imprecise, use division
template<typename Coord>
struct needs_division<Coord,
    typename voider<fl::bool_constant<(Coord::FRAC_BITS < 16)>>::type>
    : fl::bool_constant<(Coord::FRAC_BITS < 16)> {};

// Tag-dispatch: high precision → multiply by precomputed reciprocal
template<typename Coord>
inline Coord aaRatioDispatch(Coord num, Coord, Coord inv_denom, fl::false_type) {
    return num * inv_denom;
}

// Tag-dispatch: low precision → direct division (more accurate)
template<typename Coord>
inline Coord aaRatioDispatch(Coord num, Coord denom, Coord, fl::true_type) {
    return num / denom;
}

}  // namespace detail

/// Compute num/denom using the best strategy for the Coord type:
/// - float, s16x16, s8x24 (FRAC_BITS >= 16): multiply by precomputed reciprocal
/// - s8x8, s12x4, s24x8 (FRAC_BITS < 16): direct division
template<typename Coord>
inline Coord aaRatio(Coord num, Coord denom, Coord inv_denom) {
    return detail::aaRatioDispatch(num, denom, inv_denom,
        typename detail::needs_division<Coord>::type());
}

/// ============================================================================
/// CANVAS API (Primary - Cache Optimal)
/// ============================================================================
/// Graphics operations work on rectangular Canvas objects with direct row-major indexing.
/// All drawing functions use row-major indexing for cache-optimal performance.

/// @brief Wu antialiased line drawing
/// @param canvas Canvas with pixel buffer and dimensions
/// @param x0, y0, x1, y1 Line start and end coordinates (any arithmetic type)
/// @param color Pixel color
template<typename PixelT, typename Coord>
void drawLine(Canvas<PixelT>& canvas, const PixelT& color,
              Coord x0, Coord y0, Coord x1, Coord y1);

template<typename PixelT, typename Coord>
void drawDisc(Canvas<PixelT>& canvas, const PixelT& color,
              Coord cx, Coord cy, Coord r);

template<typename PixelT, typename Coord>
void drawRing(Canvas<PixelT>& canvas, const PixelT& color,
              Coord cx, Coord cy, Coord r, Coord thickness);

template<typename PixelT, typename Coord>
void drawStrokeLine(Canvas<PixelT>& canvas, const PixelT& color,
                    Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness,
                    LineCap cap);

/// ============================================================================
/// LEGACY RASTERTARGET API (Deprecated - for backward compatibility)
/// ============================================================================

/// ============================================================================
/// CANVAS IMPLEMENTATIONS
/// ============================================================================

template<typename PixelT, typename Coord>
inline void drawLine(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord x0, Coord y0, Coord x1, Coord y1) {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;
    Coord fx0 = x0, fy0 = y0, fx1 = x1, fy1 = y1;

    bool steep = fl::abs(fy1 - fy0) > fl::abs(fx1 - fx0);
    if (steep) {
        fl::swap(fx0, fy0);
        fl::swap(fx1, fy1);
    }
    if (fx0 > fx1) {
        fl::swap(fx0, fx1);
        fl::swap(fy0, fy1);
    }

    Coord dx = fx1 - fx0;
    Coord dy = fy1 - fy0;
    Coord zero = fromFrac<Coord>(0, 1);
    Coord gradient = (dx == zero) ? zero : dy / dx;

    // First endpoint
    Coord half = fromFrac<Coord>(1, 2);
    Coord xend_intermediate = fl::floor(fx0 + half);
    Coord yend = fy0 + gradient * (xend_intermediate - fx0);
    Coord yend_intermediate = fl::floor(yend);
    Coord xgap = fromInt<Coord>(1) - (fx0 + half - fl::floor(fx0 + half));
    Coord xpxl1 = xend_intermediate;
    Coord ypxl1 = yend_intermediate;

    // Second endpoint
    Coord xend2_intermediate = fl::floor(fx1 + half);
    Coord yend2 = fy1 + gradient * (xend2_intermediate - fx1);
    Coord yend2_intermediate = fl::floor(yend2);
    Coord xgap2 = (fx1 + half) - fl::floor(fx1 + half);
    Coord xpxl2 = xend2_intermediate;
    Coord ypxl2 = yend2_intermediate;

    Coord loopStart = xpxl1 + fromInt<Coord>(1);
    Coord loopEnd = xpxl2;

    Coord one = fromInt<Coord>(1);
    if (steep) {
        // STEEP path: x and y coordinates swapped in pixel writes
        {
            Coord yend_floor = fl::floor(yend);
            Coord yf = yend - yend_floor;
            Coord b1 = (one - yf) * xgap;
            Coord b2 = yf * xgap;
            PixelT c = color;
            c.nscale8(coordToU8(b1));
            addPixelToBuffer(pixels, width, height, toInt(ypxl1),     toInt(xpxl1), c);
            c = color;
            c.nscale8(coordToU8(b2));
            addPixelToBuffer(pixels, width, height, toInt(ypxl1+one), toInt(xpxl1), c);
        }
        Coord intery = yend + gradient;
        for (Coord x = loopStart; x < loopEnd; x = x + one) {
            Coord intery_floor = fl::floor(intery);
            Coord yint = intery_floor;
            Coord yf = intery - intery_floor;
            PixelT c = color;
            c.nscale8(coordToU8(one - yf));
            addPixelToBuffer(pixels, width, height, toInt(yint),     toInt(x), c);
            c = color;
            c.nscale8(coordToU8(yf));
            addPixelToBuffer(pixels, width, height, toInt(yint+one), toInt(x), c);
            intery = intery + gradient;
        }
        {
            Coord yend2_floor = fl::floor(yend2);
            Coord yf = yend2 - yend2_floor;
            Coord b1 = (one - yf) * xgap2;
            Coord b2 = yf * xgap2;
            PixelT c = color;
            c.nscale8(coordToU8(b1));
            addPixelToBuffer(pixels, width, height, toInt(ypxl2),     toInt(xpxl2), c);
            c = color;
            c.nscale8(coordToU8(b2));
            addPixelToBuffer(pixels, width, height, toInt(ypxl2+one), toInt(xpxl2), c);
        }
    } else {
        // FLAT path: normal coordinate order
        {
            Coord yend_floor = fl::floor(yend);
            Coord yf = yend - yend_floor;
            Coord b1 = (one - yf) * xgap;
            Coord b2 = yf * xgap;
            PixelT c = color;
            c.nscale8(coordToU8(b1));
            addPixelToBuffer(pixels, width, height, toInt(xpxl1), toInt(ypxl1),     c);
            c = color;
            c.nscale8(coordToU8(b2));
            addPixelToBuffer(pixels, width, height, toInt(xpxl1), toInt(ypxl1+one), c);
        }
        Coord intery = yend + gradient;
        for (Coord x = loopStart; x < loopEnd; x = x + one) {
            Coord intery_floor = fl::floor(intery);
            Coord yint = intery_floor;
            Coord yf = intery - intery_floor;
            PixelT c = color;
            c.nscale8(coordToU8(one - yf));
            addPixelToBuffer(pixels, width, height, toInt(x), toInt(yint),     c);
            c = color;
            c.nscale8(coordToU8(yf));
            addPixelToBuffer(pixels, width, height, toInt(x), toInt(yint+one), c);
            intery = intery + gradient;
        }
        {
            Coord yend2_floor = fl::floor(yend2);
            Coord yf = yend2 - yend2_floor;
            Coord b1 = (one - yf) * xgap2;
            Coord b2 = yf * xgap2;
            PixelT c = color;
            c.nscale8(coordToU8(b1));
            addPixelToBuffer(pixels, width, height, toInt(xpxl2), toInt(ypxl2),     c);
            c = color;
            c.nscale8(coordToU8(b2));
            addPixelToBuffer(pixels, width, height, toInt(xpxl2), toInt(ypxl2+one), c);
        }
    }
}

template<typename PixelT, typename Coord>
inline void drawDisc(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord cx, Coord cy, Coord r) {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;
    Coord soft = fromFrac<Coord>(1, 2);
    Coord r_outer = r + soft;
    Coord r_inner = r - soft;

    Coord r_outer_ceil = fl::ceil(r_outer);
    Coord cx_floor = fl::floor(cx);
    Coord cy_floor = fl::floor(cy);

    Coord ri = r_outer_ceil + fromInt<Coord>(1);

    Coord zero = fromFrac<Coord>(0, 1);
    Coord w = fromInt<Coord>(width);
    Coord h = fromInt<Coord>(height);
    Coord one = fromInt<Coord>(1);

    Coord xmin = cx_floor - ri;
    Coord xmax = cx_floor + ri;
    if (xmin < zero) xmin = zero;
    if (xmax >= w) xmax = w - one;

    Coord ymin = cy_floor - ri;
    Coord ymax = cy_floor + ri;
    if (ymin < zero) ymin = zero;
    if (ymax >= h) ymax = h - one;

    if (xmin > xmax || ymin > ymax) return;

    Coord r_inner2 = r_inner * r_inner;
    Coord r_outer2 = r_outer * r_outer;
    Coord band = r_outer2 - r_inner2;

    if (band <= zero) return;

    Coord inv_band = one / band;

    for (Coord py = ymin; py <= ymax; py = py + one) {
        for (Coord px = xmin; px <= xmax; px = px + one) {
            Coord dx = px - cx;
            Coord dy = py - cy;
            Coord d2 = dx * dx + dy * dy;

            if (d2 < r_outer2) {
                if (d2 < r_inner2) {
                    addPixelToBuffer(pixels, width, height, toInt(px), toInt(py), color);
                } else {
                    Coord alpha = aaRatio(r_outer2 - d2, band, inv_band);
                    PixelT c = color;
                    c.nscale8(coordToU8(alpha));
                    addPixelToBuffer(pixels, width, height, toInt(px), toInt(py), c);
                }
            }
        }
    }
}

template<typename PixelT, typename Coord>
inline void drawRing(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord cx, Coord cy, Coord r, Coord thickness) {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;
    Coord soft = fromFrac<Coord>(1, 2);
    Coord zero = fromFrac<Coord>(0, 1);
    Coord one = fromInt<Coord>(1);

    Coord r_ii = r - soft;
    Coord r_io = r + soft;
    if (r_ii < zero) r_ii = zero;
    if (r_io < zero) r_io = zero;
    Coord r_oi = r + thickness - soft;
    Coord r_oo = r + thickness + soft;

    Coord ii2 = r_ii * r_ii;
    Coord io2 = r_io * r_io;
    Coord oi2 = r_oi * r_oi;
    Coord oo2 = r_oo * r_oo;

    Coord inner_band = io2 - ii2;
    Coord outer_band = oo2 - oi2;

    Coord r_oo_ceil = fl::ceil(r_oo);
    Coord cx_floor = fl::floor(cx);
    Coord cy_floor = fl::floor(cy);

    Coord ri = r_oo_ceil + one;

    Coord w = fromInt<Coord>(width);
    Coord h = fromInt<Coord>(height);

    Coord xmin = cx_floor - ri;
    Coord xmax = cx_floor + ri;
    Coord ymin = cy_floor - ri;
    Coord ymax = cy_floor + ri;

    if (xmin < zero) xmin = zero;
    if (xmax >= w) xmax = w - one;
    if (ymin < zero) ymin = zero;
    if (ymax >= h) ymax = h - one;

    if (xmin > xmax || ymin > ymax) return;

    Coord inv_inner_band = (inner_band > zero) ? one / inner_band : zero;
    Coord inv_outer_band = (outer_band > zero) ? one / outer_band : zero;

    for (Coord py = ymin; py <= ymax; py = py + one) {
        for (Coord px = xmin; px <= xmax; px = px + one) {
            Coord dx = px - cx;
            Coord dy = py - cy;
            Coord d2 = dx * dx + dy * dy;

            if (d2 < oo2) {
                if (d2 < ii2) {
                    // Inside hole - transparent
                } else if (d2 < io2) {
                    // Inner AA fringe
                    if (inner_band > zero) {
                        Coord alpha = aaRatio(d2 - ii2, inner_band, inv_inner_band);
                        PixelT c = color;
                        c.nscale8(coordToU8(alpha));
                        addPixelToBuffer(pixels, width, height, toInt(px), toInt(py), c);
                    }
                } else if (d2 < oi2) {
                    // Full brightness band
                    addPixelToBuffer(pixels, width, height, toInt(px), toInt(py), color);
                } else {
                    // Outer AA fringe
                    if (outer_band > zero) {
                        Coord alpha = aaRatio(oo2 - d2, outer_band, inv_outer_band);
                        PixelT c = color;
                        c.nscale8(coordToU8(alpha));
                        addPixelToBuffer(pixels, width, height, toInt(px), toInt(py), c);
                    }
                }
            }
        }
    }
}

template<typename PixelT, typename Coord>
inline void drawStrokeLine(Canvas<PixelT>& canvas, const PixelT& color,
                           Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness,
                           LineCap cap) {
    // Keep all computations in the coordinate type domain
    // This preserves full precision for the input type (e.g., s16x16 maintains 16-bit fractional precision)
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;

    Coord dx = x1 - x0;
    Coord dy = y1 - y0;

    // Check for zero-length line using zero-constructed value
    Coord zero = fromFrac<Coord>(0, 1);
    if (dx == zero && dy == zero) return;

    Coord len2 = dx * dx + dy * dy;
    Coord r_max = thickness / fromInt<Coord>(2);  // Half thickness = radius

    // Floor pixel coordinates (keep in Coord type for precision)
    Coord x0_floored = fl::floor(x0);
    Coord y0_floored = fl::floor(y0);
    Coord x1_floored = fl::floor(x1);
    Coord y1_floored = fl::floor(y1);
    Coord margin = r_max + fromFrac<Coord>(3, 2);

    // Compute bounds in Coord type
    Coord xmin_coord = (x0_floored < x1_floored ? x0_floored : x1_floored) - margin;
    Coord xmax_coord = (x0_floored > x1_floored ? x0_floored : x1_floored) + margin;
    Coord ymin_coord = (y0_floored < y1_floored ? y0_floored : y1_floored) - margin;
    Coord ymax_coord = (y0_floored > y1_floored ? y0_floored : y1_floored) + margin;

    // Clamp bounds to canvas dimensions
    Coord width_coord = fromInt<Coord>(width);
    Coord height_coord = fromInt<Coord>(height);
    if (xmin_coord < zero) xmin_coord = zero;
    if (xmax_coord >= width_coord) xmax_coord = width_coord - fromInt<Coord>(1);
    if (ymin_coord < zero) ymin_coord = zero;
    if (ymax_coord >= height_coord) ymax_coord = height_coord - fromInt<Coord>(1);

    if (xmin_coord > xmax_coord || ymin_coord > ymax_coord) return;

    Coord r_max2 = r_max * r_max;
    Coord one = fromInt<Coord>(1);
    Coord inv_r_max2 = one / r_max2;

    for (Coord py = ymin_coord; py <= ymax_coord; py = py + one) {
        for (Coord px = xmin_coord; px <= xmax_coord; px = px + one) {
            // Perpendicular distance from (px, py) to the line
            Coord px_rel = px - x0;
            Coord py_rel = py - y0;

            // Project point onto line: t = dot(point-start, direction) / len2
            Coord dot_prod = px_rel * dx + py_rel * dy;
            Coord t = dot_prod / len2;

            // Project onto line
            Coord proj_x = x0 + t * dx;
            Coord proj_y = y0 + t * dy;

            // Distance from point to projection
            Coord dist_x = px - proj_x;
            Coord dist_y = py - proj_y;
            Coord dist2 = dist_x * dist_x + dist_y * dist_y;

            if (dist2 < r_max2) {
                int px_int = toInt(px);
                int py_int = toInt(py);
                if (t >= zero && t <= one) {
                    // On segment — LUT-based sqrt falloff
                    fl::u8 idx = coordToU8(aaRatio(dist2, r_max2, inv_r_max2));
                    PixelT c = color;
                    c.nscale8(FL_PGM_READ_BYTE_NEAR(&detail::distanceAA_LUT[idx]));
                    addPixelToBuffer(pixels, width, height, px_int, py_int, c);
                } else if (cap == LineCap::ROUND) {
                    // Hemispherical cap: distance from nearest endpoint
                    Coord ex = (t < zero) ? (px - x0) : (px - x1);
                    Coord ey = (t < zero) ? (py - y0) : (py - y1);
                    Coord ed2 = ex*ex + ey*ey;
                    if (ed2 < r_max2) {
                        fl::u8 idx = coordToU8(aaRatio(ed2, r_max2, inv_r_max2));
                        PixelT c = color;
                        c.nscale8(FL_PGM_READ_BYTE_NEAR(&detail::distanceAA_LUT[idx]));
                        addPixelToBuffer(pixels, width, height, px_int, py_int, c);
                    }
                } else if (cap == LineCap::SQUARE) {
                    // Rectangular extension: extend t range by r_max/len
                    Coord len = sqrt(len2);  // Uses gfx::sqrt for s16x16, std::sqrt for float
                    Coord t_ext = r_max / len;
                    if (t >= -t_ext && t <= one + t_ext) {
                        fl::u8 idx = coordToU8(aaRatio(dist2, r_max2, inv_r_max2));
                        PixelT c = color;
                        c.nscale8(FL_PGM_READ_BYTE_NEAR(&detail::distanceAA_LUT[idx]));
                        addPixelToBuffer(pixels, width, height, px_int, py_int, c);
                    }
                }
                // FLAT: pixels outside [0,1] are silently dropped (current behavior)
            }
        }
    }
}

}  // namespace gfx
}  // namespace fl

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
    return (fl::u8)fl::clamp((alpha * c255 + half).to_int(), (fl::i32)0, (fl::i32)255);
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

// ============================================================================
// Integer-optimized helpers for disc/ring/line drawing
// ============================================================================

namespace detail {

/// Convert any Coord type to 8.8 fixed-point (fl::i32).
/// Generic fallback: use to_float() conversion.
template<typename Coord>
inline fl::i32 toFixed8(Coord val) {
    return static_cast<fl::i32>(val.to_float() * 256.0f);
}

template<>
inline fl::i32 toFixed8<float>(float val) {
    return static_cast<fl::i32>(val * 256.0f);
}

template<>
inline fl::i32 toFixed8<double>(double val) {
    return static_cast<fl::i32>(val * 256.0);
}

template<>
inline fl::i32 toFixed8<int>(int val) {
    return static_cast<fl::i32>(val) << 8;
}

template<>
inline fl::i32 toFixed8<fl::s16x16>(fl::s16x16 val) {
    // Q16.16 → Q8.8: shift right by 8
    return static_cast<fl::i32>(val.raw() >> 8);
}

/// Precompute right-shift and reciprocal multiplier for AA division.
/// Converts 32-bit AA division: (diff * 255u) / band  (~60 cycles on AVR)
/// into a multiply-by-reciprocal: ((diff >> shift) * inv) >> 8  (~14 cycles on AVR)
/// where inv = round(255 * 256 / scaled).
inline void computeBandShift(fl::i32 band, fl::u8 &shift_out, fl::u16 &inv_out) {
    fl::u8 sh = 0;
    fl::u32 tmp = static_cast<fl::u32>(band);
    while (tmp > 255u) { tmp >>= 1; ++sh; }
    shift_out = sh;
    // Reciprocal: (255 * 256 + scaled/2) / scaled — rounds to nearest
    fl::u8 scaled = static_cast<fl::u8>(tmp);
    inv_out = static_cast<fl::u16>((65280u + (scaled >> 1)) / scaled);
}

/// Disc context: bundles per-circle constants into a struct passed by reference.
/// This reduces function-call overhead on register-poor architectures (AVR: 12 params
/// → 1 pointer), matching the struct-based approach used in hand-optimized code.
template<typename PixelT>
struct DiscCtx {
    fl::i32 xdelta0;
    int xmin, xmax;
    fl::i32 rin2, rout2;
    fl::u8 band_shift;
    fl::u16 band_inv;      // reciprocal: (255 * 256 + scaled/2) / scaled
    PixelT color;
};

/// Ring context: bundles all per-circle constants into a struct passed by reference.
/// This reduces function-call overhead on register-poor architectures (AVR: 16 params
/// → 1 pointer), matching the struct-based approach used in hand-optimized code.
template<typename PixelT>
struct RingCtx {
    fl::i32 xdelta0;
    int xmin, xmax;
    fl::i32 ii2, io2, oi2, oo2;
    fl::u8 inner_shift, outer_shift;
    fl::u16 inner_inv, outer_inv;  // reciprocals: (255 * 256 + scaled/2) / scaled
    PixelT color;
};

/// Render one scanline of a disc using incremental d².
/// Uses (n+1)² = n² + 2n + 1 identity — zero multiplies in the inner loop.
/// Phase-based while loops: outside → AA fringe → solid → AA fringe → outside.
/// AA fringe uses precomputed shift+scaled divisor for cheap 16÷8 division.
template<typename PixelT>
inline void renderDiscRow(PixelT* buf, int w, int py,
                          fl::i32 d2_row,
                          const DiscCtx<PixelT>& f) {
    PixelT* ptr = &buf[py * w + f.xmin];
    fl::i32 d2 = d2_row;
    fl::i32 xd = f.xdelta0;
    int px = f.xmin;
    // Phase 1: Skip outside pixels (left, d2 decreasing)
    while (px <= f.xmax && d2 >= f.rout2) {
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 2: Outer AA fringe (left)
    while (px <= f.xmax && d2 >= f.rin2 && d2 < f.rout2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(f.rout2 - d2) >> f.band_shift);
        fl::u8 br = static_cast<fl::u8>((diff * f.band_inv) >> 8);
        PixelT c = f.color; c.nscale8(br); *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 3: Full brightness interior (d2 hits minimum, then increases)
    while (px <= f.xmax && d2 < f.rin2) {
        *ptr += f.color;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 4: Outer AA fringe (right, d2 increasing)
    while (px <= f.xmax && d2 < f.rout2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(f.rout2 - d2) >> f.band_shift);
        fl::u8 br = static_cast<fl::u8>((diff * f.band_inv) >> 8);
        PixelT c = f.color; c.nscale8(br); *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
}

/// Render one scanline of a ring using incremental d² with phase-based scanning.
/// Seven sequential while-loops classify pixels into zones without per-pixel branching.
/// AA fringe uses precomputed shift+scaled divisor for cheap 16÷8 division.
/// Zones (left to right): outside → outer-AA → solid → inner-AA → hole →
///                         inner-AA → solid → outer-AA → outside.
template<typename PixelT>
inline void renderRingRow(PixelT* buf, int w, int py,
                          fl::i32 d2_row,
                          const RingCtx<PixelT>& g) {
    PixelT* ptr = &buf[py * w + g.xmin];
    fl::i32 d2 = d2_row;
    fl::i32 xd = g.xdelta0;
    int px = g.xmin;
    // Preamble: left exterior (d2 >= oo2)
    while (px <= g.xmax && d2 >= g.oo2) {
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 1: outer-left AA fringe (oi2 <= d2 < oo2)
    while (px <= g.xmax && d2 >= g.oi2 && d2 < g.oo2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(g.oo2 - d2) >> g.outer_shift);
        fl::u8 br = static_cast<fl::u8>((diff * g.outer_inv) >> 8);
        PixelT c = g.color; c.nscale8(br); *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 2: full-brightness band left (io2 <= d2 < oi2)
    while (px <= g.xmax && d2 >= g.io2 && d2 < g.oi2) {
        *ptr += g.color;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 3: inner-left AA fringe (ii2 <= d2 < io2)
    while (px <= g.xmax && d2 >= g.ii2 && d2 < g.io2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(d2 - g.ii2) >> g.inner_shift);
        fl::u8 br = static_cast<fl::u8>((diff * g.inner_inv) >> 8);
        PixelT c = g.color; c.nscale8(br); *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 4: transparent hole (d2 < ii2)
    while (px <= g.xmax && d2 < g.ii2) {
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 5: inner-right AA fringe (ii2 <= d2 < io2)
    while (px <= g.xmax && d2 >= g.ii2 && d2 < g.io2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(d2 - g.ii2) >> g.inner_shift);
        fl::u8 br = static_cast<fl::u8>((diff * g.inner_inv) >> 8);
        PixelT c = g.color; c.nscale8(br); *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 6: full-brightness band right (io2 <= d2 < oi2)
    while (px <= g.xmax && d2 >= g.io2 && d2 < g.oi2) {
        *ptr += g.color;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 7: outer-right AA fringe (oi2 <= d2 < oo2)
    while (px <= g.xmax && d2 >= g.oi2 && d2 < g.oo2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(g.oo2 - d2) >> g.outer_shift);
        fl::u8 br = static_cast<fl::u8>((diff * g.outer_inv) >> 8);
        PixelT c = g.color; c.nscale8(br); *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
}

}  // namespace detail

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

// ---------------------------------------------------------------------------
// drawLine: Integer Wu antialiased line (8.8 fixed-point internally)
// Uses int loop counter and bit-shift for AA — no Coord ops in inner loop.
// ---------------------------------------------------------------------------
template<typename PixelT, typename Coord>
inline void drawLine(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord x0, Coord y0, Coord x1, Coord y1) {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;

    // Convert to 8.8 fixed-point
    fl::i32 fx0 = detail::toFixed8(x0);
    fl::i32 fy0 = detail::toFixed8(y0);
    fl::i32 fx1 = detail::toFixed8(x1);
    fl::i32 fy1 = detail::toFixed8(y1);

    bool steep = fl::abs(fy1 - fy0) > fl::abs(fx1 - fx0);
    if (steep) { fl::swap(fx0, fy0); fl::swap(fx1, fy1); }
    if (fx0 > fx1) { fl::swap(fx0, fx1); fl::swap(fy0, fy1); }

    fl::i32 dx8 = fx1 - fx0;
    fl::i32 dy8 = fy1 - fy0;

    // Gradient per pixel step in 8.8: (dy * 256) / dx
    fl::i32 gradient = (dx8 == 0) ? 0 : ((dy8 << 8) / dx8);

    // First endpoint: round x to nearest pixel boundary
    fl::i32 xend = (fx0 + 128) & ~0xFF;
    fl::i32 yend = fy0 + ((gradient * (xend - fx0)) >> 8);
    fl::i32 xgap = 256 - ((fx0 + 128) & 0xFF);  // rfpart(x0 + 0.5)
    int xpxl1 = xend >> 8;
    int ypxl1 = yend >> 8;
    fl::u8 yfrac1 = static_cast<fl::u8>(yend & 0xFF);

    // Second endpoint
    fl::i32 xend2 = (fx1 + 128) & ~0xFF;
    fl::i32 yend2 = fy1 + ((gradient * (xend2 - fx1)) >> 8);
    fl::i32 xgap2 = (fx1 + 128) & 0xFF;  // fpart(x1 + 0.5)
    int xpxl2 = xend2 >> 8;
    int ypxl2 = yend2 >> 8;
    fl::u8 yfrac2 = static_cast<fl::u8>(yend2 & 0xFF);

    // Draw first endpoint
    {
        fl::u8 b1 = static_cast<fl::u8>(((255 - yfrac1) * xgap) >> 8);
        fl::u8 b2 = static_cast<fl::u8>((yfrac1 * xgap) >> 8);
        if (steep) {
            PixelT c = color; c.nscale8(b1);
            addPixelToBuffer(pixels, width, height, ypxl1, xpxl1, c);
            c = color; c.nscale8(b2);
            addPixelToBuffer(pixels, width, height, ypxl1 + 1, xpxl1, c);
        } else {
            PixelT c = color; c.nscale8(b1);
            addPixelToBuffer(pixels, width, height, xpxl1, ypxl1, c);
            c = color; c.nscale8(b2);
            addPixelToBuffer(pixels, width, height, xpxl1, ypxl1 + 1, c);
        }
    }

    // Draw second endpoint
    {
        fl::u8 b1 = static_cast<fl::u8>(((255 - yfrac2) * xgap2) >> 8);
        fl::u8 b2 = static_cast<fl::u8>((yfrac2 * xgap2) >> 8);
        if (steep) {
            PixelT c = color; c.nscale8(b1);
            addPixelToBuffer(pixels, width, height, ypxl2, xpxl2, c);
            c = color; c.nscale8(b2);
            addPixelToBuffer(pixels, width, height, ypxl2 + 1, xpxl2, c);
        } else {
            PixelT c = color; c.nscale8(b1);
            addPixelToBuffer(pixels, width, height, xpxl2, ypxl2, c);
            c = color; c.nscale8(b2);
            addPixelToBuffer(pixels, width, height, xpxl2, ypxl2 + 1, c);
        }
    }

    // Main loop: integer counter, 8.8 intery accumulation
    fl::i32 intery = yend + gradient;
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            int y = intery >> 8;
            fl::u8 frac = static_cast<fl::u8>(intery & 0xFF);
            PixelT c = color; c.nscale8(255 - frac);
            addPixelToBuffer(pixels, width, height, y, x, c);
            c = color; c.nscale8(frac);
            addPixelToBuffer(pixels, width, height, y + 1, x, c);
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            int y = intery >> 8;
            fl::u8 frac = static_cast<fl::u8>(intery & 0xFF);
            PixelT c = color; c.nscale8(255 - frac);
            addPixelToBuffer(pixels, width, height, x, y, c);
            c = color; c.nscale8(frac);
            addPixelToBuffer(pixels, width, height, x, y + 1, c);
            intery += gradient;
        }
    }
}

// ---------------------------------------------------------------------------
// drawDisc: Incremental d² scanline approach
// Converts to 8.8 fixed-point, uses (n+1)² = n² + 2n + 1 identity.
// Only 4 multiplies to seed recurrences; inner loops are pure addition.
// ---------------------------------------------------------------------------
template<typename PixelT, typename Coord>
inline void drawDisc(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord cx, Coord cy, Coord r) {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;

    fl::i32 cx8 = detail::toFixed8(cx);
    fl::i32 cy8 = detail::toFixed8(cy);
    fl::i32 r8 = detail::toFixed8(r);

    fl::i32 rin8 = r8 - 128;   // r - 0.5 in 8.8
    fl::i32 rout8 = r8 + 128;  // r + 0.5 in 8.8
    // Do NOT clamp rin8 — negative rin8 squares correctly for band computation

    fl::i32 rin2 = rin8 * rin8;
    fl::i32 rout2 = rout8 * rout8;
    fl::i32 band = rout2 - rin2;
    if (band <= 0) return;

    int ri = (rout8 >> 8) + 1;
    int cxi = cx8 >> 8;
    int cyi = cy8 >> 8;

    int xmin = cxi - ri; if (xmin < 0) xmin = 0;
    int xmax = cxi + ri; if (xmax >= width) xmax = width - 1;
    if (xmin > xmax) return;
    if (cyi + ri < 0 || cyi - ri >= height) return;

    // Seed x-axis recurrence (only multiplies in the function)
    fl::i32 dx8 = (static_cast<fl::i32>(xmin) << 8) - cx8;
    fl::i32 dx2 = dx8 * dx8;

    // Bundle per-circle constants into struct (reduces parameter passing on AVR)
    detail::DiscCtx<PixelT> fc;
    fc.xdelta0 = 512 * dx8 + 65536;
    fc.xmin = xmin; fc.xmax = xmax;
    fc.rin2 = rin2; fc.rout2 = rout2;
    detail::computeBandShift(band, fc.band_shift, fc.band_inv);
    fc.color = color;

    fl::i32 cyfrac = (static_cast<fl::i32>(cyi) << 8) - cy8;
    fl::i32 d2c = dx2 + cyfrac * cyfrac;

    // Render center row
    if (cyi >= 0 && cyi < height)
        detail::renderDiscRow(pixels, width, cyi, d2c, fc);

    // Top/bottom rows via separate y recurrences
    fl::i32 botd2 = d2c, botdelta = 512 * cyfrac + 65536;
    fl::i32 topd2 = d2c, topdelta = -512 * cyfrac + 65536;
    botd2 += botdelta; botdelta += 131072;
    topd2 += topdelta; topdelta += 131072;

    for (int dy = 1; dy <= ri; ++dy) {
        bool cbot = (botd2 - dx2 <= rout2);
        bool ctop = (topd2 - dx2 <= rout2);
        if (!cbot && !ctop) break;
        int pyb = cyi + dy, pyt = cyi - dy;
        if (cbot && pyb >= 0 && pyb < height)
            detail::renderDiscRow(pixels, width, pyb, botd2, fc);
        if (ctop && pyt >= 0 && pyt < height)
            detail::renderDiscRow(pixels, width, pyt, topd2, fc);
        botd2 += botdelta; botdelta += 131072;
        topd2 += topdelta; topdelta += 131072;
    }
}

// ---------------------------------------------------------------------------
// drawRing: Incremental d² with per-pixel zone classification
// Same recurrence as drawDisc but classifies pixels into 5 ring zones.
// Zero multiplies in inner loop; only additions, comparisons, and branches.
// ---------------------------------------------------------------------------
template<typename PixelT, typename Coord>
inline void drawRing(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord cx, Coord cy, Coord r, Coord thickness) {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;

    fl::i32 cx8 = detail::toFixed8(cx);
    fl::i32 cy8 = detail::toFixed8(cy);
    fl::i32 r8 = detail::toFixed8(r);
    fl::i32 t8 = detail::toFixed8(thickness);

    fl::i32 r_ii8 = r8 - 128;            // inner-inner: r - 0.5
    fl::i32 r_io8 = r8 + 128;            // inner-outer: r + 0.5
    fl::i32 r_oi8 = r8 + t8 - 128;       // outer-inner: r + thickness - 0.5
    fl::i32 r_oo8 = r8 + t8 + 128;       // outer-outer: r + thickness + 0.5

    // Clamp inner radii (match original behavior for small r)
    if (r_ii8 < 0) r_ii8 = 0;
    if (r_io8 < 0) r_io8 = 0;

    fl::i32 ii2 = r_ii8 * r_ii8;
    fl::i32 io2 = r_io8 * r_io8;
    fl::i32 oi2 = r_oi8 * r_oi8;
    fl::i32 oo2 = r_oo8 * r_oo8;

    fl::i32 inner_band = io2 - ii2;
    fl::i32 outer_band = oo2 - oi2;

    int ri = (r_oo8 >> 8) + 1;
    int cxi = cx8 >> 8;
    int cyi = cy8 >> 8;

    int xmin = cxi - ri; if (xmin < 0) xmin = 0;
    int xmax = cxi + ri; if (xmax >= width) xmax = width - 1;
    if (xmin > xmax) return;
    if (cyi + ri < 0 || cyi - ri >= height) return;

    fl::i32 dx8 = (static_cast<fl::i32>(xmin) << 8) - cx8;
    fl::i32 dx2 = dx8 * dx8;

    // Bundle per-circle constants into struct (reduces parameter passing on AVR)
    detail::RingCtx<PixelT> gc;
    gc.xdelta0 = 512 * dx8 + 65536;
    gc.xmin = xmin; gc.xmax = xmax;
    gc.ii2 = ii2; gc.io2 = io2; gc.oi2 = oi2; gc.oo2 = oo2;
    if (inner_band > 0) detail::computeBandShift(inner_band, gc.inner_shift, gc.inner_inv);
    else { gc.inner_shift = 0; gc.inner_inv = 65280u; }
    if (outer_band > 0) detail::computeBandShift(outer_band, gc.outer_shift, gc.outer_inv);
    else { gc.outer_shift = 0; gc.outer_inv = 65280u; }
    gc.color = color;

    fl::i32 cyfrac = (static_cast<fl::i32>(cyi) << 8) - cy8;
    fl::i32 d2c = dx2 + cyfrac * cyfrac;

    if (cyi >= 0 && cyi < height)
        detail::renderRingRow(pixels, width, cyi, d2c, gc);

    fl::i32 botd2 = d2c, botdelta = 512 * cyfrac + 65536;
    fl::i32 topd2 = d2c, topdelta = -512 * cyfrac + 65536;
    botd2 += botdelta; botdelta += 131072;
    topd2 += topdelta; topdelta += 131072;

    for (int dy = 1; dy <= ri; ++dy) {
        bool cbot = (botd2 - dx2 <= oo2);
        bool ctop = (topd2 - dx2 <= oo2);
        if (!cbot && !ctop) break;
        int pyb = cyi + dy, pyt = cyi - dy;
        if (cbot && pyb >= 0 && pyb < height)
            detail::renderRingRow(pixels, width, pyb, botd2, gc);
        if (ctop && pyt >= 0 && pyt < height)
            detail::renderRingRow(pixels, width, pyt, topd2, gc);
        botd2 += botdelta; botdelta += 131072;
        topd2 += topdelta; topdelta += 131072;
    }
}

// ---------------------------------------------------------------------------
// drawStrokeLine: Cross-product approach with incremental updates
// Perpendicular distance via cross-product; along-axis check via dot-product.
// Only 4 Coord multiplies to seed, then pure Coord additions in inner loop.
// Division replaced by multiply-by-reciprocal for AA computation.
// ---------------------------------------------------------------------------
template<typename PixelT, typename Coord>
inline void drawStrokeLine(Canvas<PixelT>& canvas, const PixelT& color,
                           Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness,
                           LineCap cap) {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;

    Coord dx = x1 - x0;
    Coord dy = y1 - y0;
    Coord zero = fromFrac<Coord>(0, 1);
    if (dx == zero && dy == zero) return;

    Coord one = fromInt<Coord>(1);
    Coord len2 = dx * dx + dy * dy;
    using fl::sqrt;
    Coord len = sqrt(len2);
    Coord r_max = thickness / fromInt<Coord>(2);
    Coord threshold = r_max * len;  // |cross| < threshold ↔ within stroke width

    if (threshold <= zero) return;

    Coord inv_threshold = one / threshold;

    // Bounding box
    int x0i = toInt(fl::floor(x0)), y0i = toInt(fl::floor(y0));
    int x1i = toInt(fl::floor(x1)), y1i = toInt(fl::floor(y1));
    int margin = toInt(fl::ceil(r_max)) + 2;
    int xmin = (x0i < x1i ? x0i : x1i) - margin;
    int xmax = (x0i > x1i ? x0i : x1i) + margin;
    int ymin = (y0i < y1i ? y0i : y1i) - margin;
    int ymax = (y0i > y1i ? y0i : y1i) + margin;

    if (xmin < 0) xmin = 0;
    if (xmax >= width) xmax = width - 1;
    if (ymin < 0) ymin = 0;
    if (ymax >= height) ymax = height - 1;
    if (xmin > xmax || ymin > ymax) return;

    // Precompute for caps
    Coord r_max2 = r_max * r_max;
    Coord inv_r_max2 = one / r_max2;
    Coord dot_ext = (cap == LineCap::SQUARE) ? threshold : zero;

    // Seed row-start cross and dot (4 Coord multiplies total, done once)
    Coord rx_base = fromInt<Coord>(xmin) - x0;
    Coord ry_row = fromInt<Coord>(ymin) - y0;
    Coord cross_row = rx_base * dy - ry_row * dx;
    Coord dot_row = rx_base * dx + ry_row * dy;

    for (int py = ymin; py <= ymax; ++py) {
        Coord cross = cross_row;
        Coord dot = dot_row;
        PixelT* ptr = &pixels[py * width + xmin];

        for (int px = xmin; px <= xmax; ++px) {
            Coord abs_cross = (cross < zero) ? (zero - cross) : cross;

            if (abs_cross < threshold) {
                bool on_segment = (dot >= zero && dot <= len2);

                if (on_segment) {
                    // AA based on perpendicular distance
                    Coord ratio = abs_cross * inv_threshold;
                    fl::u8 linear_idx = coordToU8(ratio);
                    fl::u8 sq_idx = static_cast<fl::u8>(
                        static_cast<fl::u16>(linear_idx) * linear_idx / 255u);
                    PixelT c = color;
                    c.nscale8(FL_PGM_READ_BYTE_NEAR(&detail::distanceAA_LUT[sq_idx]));
                    *ptr += c;
                } else if (cap == LineCap::ROUND) {
                    // Hemispherical cap: distance to nearest endpoint
                    Coord ex = (dot < zero) ? (fromInt<Coord>(px) - x0)
                                            : (fromInt<Coord>(px) - x1);
                    Coord ey = (dot < zero) ? (fromInt<Coord>(py) - y0)
                                            : (fromInt<Coord>(py) - y1);
                    Coord ed2 = ex * ex + ey * ey;
                    if (ed2 < r_max2) {
                        fl::u8 idx = coordToU8(aaRatio(ed2, r_max2, inv_r_max2));
                        PixelT c = color;
                        c.nscale8(FL_PGM_READ_BYTE_NEAR(&detail::distanceAA_LUT[idx]));
                        *ptr += c;
                    }
                } else if (cap == LineCap::SQUARE) {
                    // Rectangular extension
                    if (dot >= zero - dot_ext && dot <= len2 + dot_ext) {
                        Coord ratio = abs_cross * inv_threshold;
                        fl::u8 linear_idx = coordToU8(ratio);
                        fl::u8 sq_idx = static_cast<fl::u8>(
                            static_cast<fl::u16>(linear_idx) * linear_idx / 255u);
                        PixelT c = color;
                        c.nscale8(FL_PGM_READ_BYTE_NEAR(&detail::distanceAA_LUT[sq_idx]));
                        *ptr += c;
                    }
                }
                // FLAT: pixels outside [0,1] are silently dropped
            }

            // Incremental update: only Coord additions per pixel
            cross = cross + dy;
            dot = dot + dx;
            ++ptr;
        }

        // Incremental row update: only Coord additions
        cross_row = cross_row - dx;
        dot_row = dot_row + dy;
    }
}

}  // namespace gfx
}  // namespace fl

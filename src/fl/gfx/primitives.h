#pragma once

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"

#include "fl/gfx/canvas.h"  // IWYU pragma: keep
#include "fl/gfx/draw_mode.h"
#include "fl/gfx/poly_abuffer.h"  // IWYU pragma: keep
#include "fl/math/distance_lut.h"  // IWYU pragma: keep
#include "fl/math/integer_math.h"  // IWYU pragma: keep
#include "fl/math/fixed_point/s16x16.h"
#include "fl/math/math.h"
#include "fl/math/scale8.h"

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
inline fl::u8 coordToU8(T alpha) FL_NO_EXCEPT {
    return (fl::u8)fl::clamp((alpha * T(255.0f) + T(0.5f)).to_int(), 0, 255);
}

// Specialization for float
template<>
inline fl::u8 coordToU8<float>(float alpha) FL_NO_EXCEPT {
    return (fl::u8)fl::round(alpha * 255.0f);
}

// Specialization for double
template<>
inline fl::u8 coordToU8<double>(double alpha) FL_NO_EXCEPT {
    return (fl::u8)fl::round(alpha * 255.0);
}

// Specialization for int
template<>
inline fl::u8 coordToU8<int>(int alpha) FL_NO_EXCEPT {
    return (fl::u8)fl::clamp(alpha, 0, 255);
}

// Specialization for s16x16: uses from_raw, no float at all
template<>
inline fl::u8 coordToU8<fl::s16x16>(fl::s16x16 alpha) FL_NO_EXCEPT {
    constexpr fl::s16x16 c255 = fl::s16x16::from_raw(255 * fl::s16x16::SCALE);
    // 1L: the shift must be at least as wide as the result (32768 overflows
    // a 16-bit int; `1 << 15` is UB on AVR, where int is 16-bit).
    constexpr fl::s16x16 half = fl::s16x16::from_raw(1L << 15);
    return (fl::u8)fl::clamp((alpha * c255 + half).to_int(), (fl::i32)0, (fl::i32)255);
}

/// Convert an integer to Coord without float intermediate.
/// For s16x16: uses from_raw (pure integer shift). For float: implicit cast.
template<typename T>
inline T fromInt(int n) FL_NO_EXCEPT { return T(static_cast<float>(n)); }

template<>
inline fl::s16x16 fromInt<fl::s16x16>(int n) FL_NO_EXCEPT {
    return fl::s16x16::from_raw(n * fl::s16x16::SCALE);
}

/// Convert rational p/q to Coord without float.
/// For s16x16: integer multiply+divide. For float: float divide.
template<typename T>
inline T fromFrac(int p, int q) FL_NO_EXCEPT { return T(static_cast<float>(p) / static_cast<float>(q)); }

template<>
inline fl::s16x16 fromFrac<fl::s16x16>(int p, int q) FL_NO_EXCEPT {
    return fl::s16x16::from_raw((p * fl::s16x16::SCALE) / q);
}

/// Divide Coord by 2 using shift (avoids expensive division on embedded).
/// s16x16: raw right-shift (1 instruction). float: multiply by 0.5.
template<typename T>
inline T halfOf(T val) FL_NO_EXCEPT { return val / fromInt<T>(2); }

template<>
inline fl::s16x16 halfOf<fl::s16x16>(fl::s16x16 val) FL_NO_EXCEPT {
    return fl::s16x16::from_raw(val.raw() >> 1);
}

template<>
inline float halfOf<float>(float val) { return val * 0.5f; }

template<>
inline double halfOf<double>(double val) { return val * 0.5; }

template<>
inline int halfOf<int>(int val) FL_NO_EXCEPT { return val >> 1; }

/// Helper to convert any coordinate type to int
/// Supports: s16x16 (via to_int()), float, int, and other arithmetic types
template<typename T>
inline int toInt(const T& val) FL_NO_EXCEPT {
    return val.to_int();  // For s16x16 and similar
}

// Specialization for float
template<>
inline int toInt<float>(const float& val) FL_NO_EXCEPT {
    return static_cast<int>(val);
}

// Specialization for int (passthrough)
template<>
inline int toInt<int>(const int& val) FL_NO_EXCEPT {
    return val;
}

// Specialization for double
template<>
inline int toInt<double>(const double& val) FL_NO_EXCEPT {
    return static_cast<int>(val);
}

/// Internal helper: add or set pixel to rectangular buffer with bounds checking
/// Direct row-major indexing: pixels[y * width + x]
/// Templated on Overwrite for compile-time dispatch (no per-pixel branch).
template<typename PixelT, bool Overwrite>
inline void addPixelToBuffer(PixelT* pixels, int width, int height,
                              int x, int y, const PixelT& color) FL_NO_EXCEPT {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        if (Overwrite)
            pixels[y * width + x] = color;
        else
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
inline Coord aaRatioDispatch(Coord num, Coord, Coord inv_denom, fl::false_type) FL_NO_EXCEPT {
    return num * inv_denom;
}

// Tag-dispatch: low precision → direct division (more accurate)
template<typename Coord>
inline Coord aaRatioDispatch(Coord num, Coord denom, Coord, fl::true_type) FL_NO_EXCEPT {
    return num / denom;
}

}  // namespace detail

/// Compute num/denom using the best strategy for the Coord type:
/// - float, s16x16, s8x24 (FRAC_BITS >= 16): multiply by precomputed reciprocal
/// - s8x8, s12x4, s24x8 (FRAC_BITS < 16): direct division
template<typename Coord>
inline Coord aaRatio(Coord num, Coord denom, Coord inv_denom) FL_NO_EXCEPT {
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
inline fl::i32 toFixed8(Coord val) FL_NO_EXCEPT {
    return static_cast<fl::i32>(val.to_float() * 256.0f);
}

template<>
inline fl::i32 toFixed8<float>(float val) FL_NO_EXCEPT {
    return static_cast<fl::i32>(val * 256.0f);
}

template<>
inline fl::i32 toFixed8<double>(double val) FL_NO_EXCEPT {
    return static_cast<fl::i32>(val * 256.0);
}

template<>
inline fl::i32 toFixed8<int>(int val) FL_NO_EXCEPT {
    return static_cast<fl::i32>(static_cast<fl::u32>(val) << 8);
}

template<>
inline fl::i32 toFixed8<fl::s16x16>(fl::s16x16 val) FL_NO_EXCEPT {
    // Q16.16 → Q8.8: shift right by 8
    return static_cast<fl::i32>(val.raw() >> 8);
}

/// Precompute right-shift and reciprocal multiplier for AA division.
/// Converts 32-bit AA division: (diff * 255u) / band  (~60 cycles on AVR)
/// into a multiply-by-reciprocal: ((diff >> shift) * inv) >> 8  (~14 cycles on AVR)
/// where inv = round(255 * 256 / scaled).
inline void computeBandShift(fl::i32 band, fl::u8 &shift_out, fl::u16 &inv_out) FL_NO_EXCEPT {
    fl::u8 sh = 0;
    fl::u32 tmp = static_cast<fl::u32>(band);
    while (tmp > 255u) { tmp >>= 1; ++sh; }
    shift_out = sh;
    // Reciprocal: (255 * 256 + scaled/2) / scaled — rounds to nearest
    fl::u8 scaled = static_cast<fl::u8>(tmp);
    inv_out = static_cast<fl::u16>((65280u + (scaled >> 1)) / scaled);
}

/// Convert any Coord type to Q16.16 raw i32 for integer inner loops.
/// s16x16: direct raw access (already Q16.16, zero overhead).
/// float/double: multiply by 65536. int: shift left by 16.
template<typename Coord>
inline fl::i32 toQ16(Coord val) FL_NO_EXCEPT {
    return static_cast<fl::i32>(val.to_float() * 65536.0f);
}

template<>
inline fl::i32 toQ16<float>(float val) FL_NO_EXCEPT {
    return static_cast<fl::i32>(val * 65536.0f);
}

template<>
inline fl::i32 toQ16<double>(double val) FL_NO_EXCEPT {
    return static_cast<fl::i32>(val * 65536.0);
}

template<>
inline fl::i32 toQ16<int>(int val) FL_NO_EXCEPT {
    return static_cast<fl::i32>(val) << 16;
}

template<>
inline fl::i32 toQ16<fl::s16x16>(fl::s16x16 val) FL_NO_EXCEPT {
    return val.raw();
}

/// Maximum |vertex coordinate| accepted by drawPolygon, in Q16.16 units.
/// 8191 px keeps all Q16.16 values and edge deltas in i32 with headroom.
// 8191L: shifting a 16-bit int by its own width is UB (AVR int is 16-bit),
// where the constant silently collapses to 0 and every vertex clamps to the
// origin.
static const fl::i32 kPolyQ16Max = 8191L << 16;

/// Convert any Coord type to Q16.16, clamped to ±8191 px (NaN -> 0).
/// Shared with the ABuffer kernel (poly_abuffer.h, polyABToQ16) so both
/// backends clamp identically.
template<typename Coord>
inline fl::i32 toQ16Clamped(Coord val) FL_NO_EXCEPT {
    return polyABToQ16<Coord>(val);
}

/// Floor-divide a by positive b (rounds toward -inf).
inline fl::i64 floorDivI64(fl::i64 a, fl::i64 b) FL_NO_EXCEPT {
    fl::i64 q = a / b;
    if ((a % b) < 0) --q;
    return q;
}

/// Floor(v / 65536) for i32. An arithmetic right shift is floor division by
/// a power of two on every target this repository builds for (gcc, clang,
/// MSVC, avr-gcc — all define signed >> as sign-extending); the test suite
/// pins this against floorDivI64 across the full i32 range.
inline fl::i32 floorDiv16I32(fl::i32 v) FL_NO_EXCEPT { return v >> 16; }

/// Same for i64 (line crossings can exceed i32 for steep short edges).
inline fl::i64 floorDiv16I64(fl::i64 v) FL_NO_EXCEPT { return v >> 16; }

/// Per-edge scanline state for drawPolygon (even-odd fill, band model).
/// x is the crossing at the CURRENT row's center (Q16.16, floored — the
/// anchor polyLineT28 integrates from) and advances exactly per row via a
/// Bresenham-style remainder recurrence: x += step; t += rem; if (t >= dy)
/// { t -= dy; x += 1; } where step = floor(65536 * B / dy) and rem =
/// 65536 * B - step * dy with rem in [0, dy). Single-row edges (dy < 65536)
/// keep step = rem = t = 0: their span can still cross a row boundary (two
/// active rows), and the committed behavior is that x stays at the py0-row
/// value on the second row — the kernel then integrates the infinite line
/// through that (stale) center crossing, and the extent below must box THAT
/// line, not the segment's endpoints.
/// hw = ceil(32768 * |B| / dy) is the line's x half-span over a full row
/// (|x(tau) - x| <= hw for |tau| <= 32768), so the per-row extent
/// lx0 = (x - hw) >> 16, lx1 = (x + hw) >> 16 is division-free (shifts) and
/// a valid bound for every row the edge is active in: px outside [lx0, lx1]
/// gets the same full/zero shortcut a polyLineT28 call would compute
/// exactly. hw is derived from (step, rem) at setup — one i64 division per
/// edge, none per row. hw < 2^45 for i32 B/dy, so it needs i64.
/// x is i64: for a steep edge spanning < 1 px, the center crossing
/// extrapolates off the segment by up to 32768 * |B| / dy, which can exceed
/// i32 (verified reachable: ~2^45). t + rem < 2*dy < 2^31 (dy < 2^30), so
/// t/rem are i32-safe. An edge is active in every canvas row it geometrically
/// occupies (py0..py1, including the partial row containing each endpoint).
struct PolyEdge {
    fl::i64 x;      // crossing x at current row center, Q16.16
    fl::i64 step;   // per-row increment, Q16.16 (0 for single-row edges)
    fl::i32 rem;    // 65536*B - step*dy, in [0, dy) (0 for single-row)
    fl::i32 t;      // running remainder accumulator
    fl::i32 dy;     // span height, Q16.16 (> 0)
    fl::i32 B;      // dx in the top-to-bottom direction, Q16.16 (signed)
    fl::i32 ytop;   // top endpoint y, Q16.16
    fl::i32 ybot;   // bottom endpoint y, Q16.16
    fl::i64 hw;     // per-row x half-span, ceil(32768*|B|/dy) (see above)
    int py0;        // first active canvas row (row containing ytop)
    int py1;        // last active canvas row (row containing ybot - 1)
};

/// Full-pixel coverage in T units (see polyLineT28).
// 1L: `1 << 28` is UB with a 16-bit int (AVR); GCC 7.3 evaluates it to 0,
// which silently collapses all polygon coverage to zero (nothing renders).
static const fl::i32 kPolyOne = 1L << 28;

/// Multiply two unsigned values < 2^48, returning the 128-bit product as a
/// (hi, lo) u64 pair. 32-bit lanes — no __int128 (MSVC host portability).
inline void polyMul128(fl::u64 a, fl::u64 b, fl::u64 &hi, fl::u64 &lo) FL_NO_EXCEPT {
    const fl::u32 a0 = (fl::u32)a, a1 = (fl::u32)(a >> 32);
    const fl::u32 b0 = (fl::u32)b, b1 = (fl::u32)(b >> 32);
    const fl::u64 p0 = (fl::u64)a0 * b0;
    const fl::u64 p1 = (fl::u64)a0 * b1 + (fl::u64)a1 * b0;
    const fl::u64 low = p0 + (p1 << 32);
    hi = (fl::u64)a1 * b1 + (p1 >> 32) + (low < p0 ? 1u : 0u);
    lo = low;
}

/// 128-bit add: (hi, lo) += (ahi, alo).
inline void polyAdd128(fl::u64 &hi, fl::u64 &lo, fl::u64 ahi, fl::u64 alo) FL_NO_EXCEPT {
    const fl::u64 low = lo + alo;
    hi += ahi + (low < lo ? 1u : 0u);
    lo = low;
}

/// Sort a band's active edges (idx[0..nc)) by the committed
/// implementation's midpoint key: the FLOORED Q16.16 crossing at the band
/// midpoint,
///   key_e = floor((2*dy_e*x_e + B_e*mid) / (2*dy_e)),   mid = a + b
/// with a stable insertion sort (equal keys keep edge-index order). The key
/// is computed in i64 exactly as the committed code did, including its wrap
/// of 2*dy*x for steep single-row edges (|2*dy*x| can exceed 2^63: |x| up
/// to ~2^47, dy < 2^16 there) — the order that wrap produces is part of the
/// committed output, so the wrap is reproduced bit-for-bit via unsigned
/// arithmetic (same bits, no UB). Sorting by the exact rational x instead
/// would change span pairing whenever two midpoints share a floored key
/// (or the old key wrapped) and is NOT byte-identical. Cost: one i64 floor
/// division per active edge per band (the same the committed code paid);
/// the per-pixel kernel needs no division.
inline void polySortBand(fl::u8 *idx, fl::i64 *keys,
                         const PolyEdge *edges, int nc, fl::i32 mid) FL_NO_EXCEPT {
    for (int e = 0; e < nc; ++e) {
        const PolyEdge &ed = edges[idx[e]];
        // (2*dy) fits in i32; (2*dy)*x and the sum wrap mod 2^64 as in the
        // committed i64 expression. B*mid < 2^47 never wraps.
        const fl::u64 num = (fl::u64)(2 * (fl::i64)ed.dy) * (fl::u64)ed.x
                          + (fl::u64)(static_cast<fl::i64>(ed.B) * (fl::i64)mid);
        keys[e] = detail::floorDivI64(static_cast<fl::i64>(num),
                                      (fl::i64)2 * ed.dy);
    }
    for (int e = 1; e < nc; ++e) {
        const fl::i64 kv = keys[e];
        const fl::u8 iv = idx[e];
        int m = e - 1;
        while (m >= 0 && keys[m] > kv) {
            keys[m + 1] = keys[m];
            idx[m + 1] = idx[m];
            --m;
        }
        keys[m + 1] = kv;
        idx[m + 1] = iv;
    }
}

/// Count leading zeros of a non-zero 32-bit value (branch form: portable to
/// MSVC where __builtin_clz is unavailable, and branch-free on x86).
inline int polyClz32(fl::u32 v) FL_NO_EXCEPT {
    int n = 0;
    if ((v & 0xFFFF0000u) == 0) { n = 16; v <<= 16; }
    if ((v & 0xFF000000u) == 0) { n += 8; v <<= 8; }
    if ((v & 0xF0000000u) == 0) { n += 4; v <<= 4; }
    if ((v & 0xC0000000u) == 0) { n += 2; v <<= 2; }
    if ((v & 0x80000000u) == 0) { n += 1; }
    return n;
}

/// Index (0..127) of the highest set bit of a 128-bit value held in four
/// 32-bit lanes (m3 most significant), or -1 when the value is zero.
inline int polyTopBit128(fl::u32 m3, fl::u32 m2, fl::u32 m1,
                         fl::u32 m0) FL_NO_EXCEPT {
    if (m3) return 127 - polyClz32(m3);
    if (m2) return 95 - polyClz32(m2);
    if (m1) return 63 - polyClz32(m1);
    if (m0) return 31 - polyClz32(m0);
    return -1;
}

/// T = round_half_up(M / dv) clamped to kPolyOne, for M = (mhi, mlo) and
/// 0 < dv < 2^63 (polyLineT28's quotient step). Exact: the 64-bit
/// remainder state s of the shift-subtract below satisfies s < dv at the
/// start of every step, so s <- 2s + bit never reaches 2^64 and the loop
/// computes q = floor(M/dv), r = M mod dv. Requires dv > 0: polyLineT28
/// never passes 0 (D == 0 makes M == 0, which returns before the call).
/// Kept out of line
/// so polyLineT28's two call sites (normal and huge edges) share one copy:
/// on AVR that measured smaller AND faster than one inlined loop with a
/// runtime-selected divisor (the register allocator spills inside it).
/// External-linkage inline (not static), like the polyClz32/polyTopBit128
/// helpers it calls, so the inline polyLineT28 refers to the same entity
/// in every TU (ODR); kPolyOne is only read as a constant (no odr-use).
inline FL_NO_INLINE fl::i32 polyDivRoundT28(
    fl::u64 mhi, fl::u64 mlo, fl::u64 dv) FL_NO_EXCEPT {
    fl::u32 m3 = (fl::u32)(mhi >> 32), m2 = (fl::u32)mhi;
    fl::u32 m1 = (fl::u32)(mlo >> 32), m0 = (fl::u32)mlo;
    const int top = polyTopBit128(m3, m2, m1, m0);
    // The committed loop feeds bits 93..0 only; M bits above 93 are
    // never read, so cap the window top at 93. In-range inputs never
    // reach it (quotient < 2^29 and dv < 2^63 give M < 2^92).
    const int t = (top < 94) ? top : 93;
    // Left-align M by (127 - t) so its highest relevant bit reaches bit
    // 127 of the window: the loop then feeds numerator bits with the
    // fixed (w1 >> 63) extraction (constant-width shifts only — no
    // libgcc __*di3 helpers on AVR).
    const int k = 127 - t;
    const int kl = k >> 5;
    const int kb = k & 31;
    {
        fl::u32 n3, n2, n1, n0;
        if (kl >= 3)      { n3 = m0; n2 = 0; n1 = 0; n0 = 0; }
        else if (kl == 2) { n3 = m1; n2 = m0; n1 = 0; n0 = 0; }
        else if (kl == 1) { n3 = m2; n2 = m1; n1 = m0; n0 = 0; }
        else              { n3 = m3; n2 = m2; n1 = m1; n0 = m0; }
        if (kb) {
            const fl::u32 h3 = n2 >> (32 - kb);
            const fl::u32 h2 = n1 >> (32 - kb);
            const fl::u32 h1 = n0 >> (32 - kb);
            n3 = (n3 << kb) | h3;
            n2 = (n2 << kb) | h2;
            n1 = (n1 << kb) | h1;
            n0 = n0 << kb;
        }
        m3 = n3; m2 = n2; m1 = n1; m0 = n0;
    }
    // The shift-subtract, expressed in 32-bit lanes: each lane op is
    // the corresponding half of the u64 op it replaces (same carries,
    // same mod-2^64 wraps of the remainder and quotient states), but
    // avr-gcc inlines 32-bit shifts, compares and subtracts instead
    // of emitting __lshrdi3/__ashldi3/__cmpdi2/__subdi3 helper calls
    // per iteration — the dominant cost of this loop on AVR.
    const fl::u32 d1 = (fl::u32)(dv >> 32), d0 = (fl::u32)dv;
    fl::u32 s1 = 0, s0 = 0;  // remainder (high, low), always < dv
    fl::u32 q1 = 0, q0 = 0;  // (q<<1)|ge each step; keeps the <64 bits
    for (int i = t; i >= 0; --i) {
        const fl::u32 bit = m3 >> 31;
        m3 = (m3 << 1) | (m2 >> 31);
        m2 = (m2 << 1) | (m1 >> 31);
        m1 = (m1 << 1) | (m0 >> 31);
        m0 = m0 << 1;
        const fl::u32 sh = s1 << 1, sl = s0 << 1;
        s1 = sh | (s0 >> 31);
        s0 = sl | bit;
        const fl::u32 ge = (s1 > d1) || (s1 == d1 && s0 >= d0);
        if (ge) {
            const fl::u32 sb = s0 - d0;
            s1 -= d1 + (fl::u32)(sb > s0);
            s0 = sb;
        }
        const fl::u32 qh = q1 << 1, ql = q0 << 1;
        q1 = qh | (q0 >> 31);
        q0 = ql | ge;
    }
    // Round half-up: if 2r >= dv, q++ (r = (s1, s0)). The carry into
    // r1 comes out of s0's MSB; s1's MSB is the 2r overflow bit.
    const fl::u32 r1 = (s1 << 1) | (s0 >> 31), r0 = s0 << 1;
    if ((s1 >> 31) != 0 || r1 > d1 || (r1 == d1 && r0 >= d0)) {
        const fl::u32 qb = q0 + 1;
        q1 += (fl::u32)(qb < q0);  // carry: q0 wrapped past 0xFFFFFFFF
        q0 = qb;
    }
    fl::u64 q = ((fl::u64)q1 << 32) | q0;
    if (q > (fl::u64)kPolyOne) q = (fl::u64)kPolyOne;  // physical bound
    return (fl::i32)q;
}

/// Exact left-of-line coverage of one pixel for one polygon edge.
///
/// Line: x(tau) = x_q + (B/D) * tau with tau in Q16.16 relative to the row
/// center (the caller's anchor; drawPolygonCore always anchors at the row
/// center, which is what the floored-Q16.16 edge crossing stores);
/// u = D * (x_q - px * 65536) at the anchor. Returns T = round(2^28 * cov)
/// where cov = int_{ta}^{tb} clamp(x(tau) - px_left, 0, 1) dtau/65536, i.e.
/// the fraction of the pixel lying to the LEFT of the line over the row
/// sub-interval [ta, tb]. Pixel coverage between span lines L (left) and R
/// (right) is then T_left(R) - T_left(L).
///
/// In s = B * tau space the clamp breakpoints are integers (s = -u and
/// s = 2*D*32768 - u), so the integral is a sum of at most two
/// trapezoid/rectangle pieces:
///   M = (hi-lo) * ((u+lo)+(u+hi)) + (s_hi-lo2) * (4*D*32768)
///   T = round(M / (32*D*|B|))
/// M < 2^98, den = 32*D*S < 2^67; the exact quotient is < 2^31 and the
/// result is clamped to 2^28 = kPolyOne, the physical full-pixel bound.
///
/// The quotient is the exact round-half-up M / den for every den (#5286).
/// For den < 2^63 (every edge with dx*dy < 2^26 px^2, i.e. all normal
/// polygons) it is the committed 457b21d1b shift-subtract, which is exact
/// in that range; huge edges (den >= 2^63) divide M >> 4 by den >> 4
/// instead, see the comment at the den setup. In polyDivRoundT28 M is
/// left-aligned so its highest relevant bit sits at the top of the
/// 128-bit window, and the numerator is fed top-down by left-shifting the
/// window in place — constant-width shifts only, no FPU, no __int128, and
/// on AVR the whole loop stays in registers (no libgcc __*di3 helper
/// calls). An M == 0 numerator
/// (line does not cross the pixel within [ta, tb]) returns 0 immediately.
/// Result: [0, kPolyOne].
inline fl::i32 polyLineT28(fl::i32 B, fl::i32 D, fl::i64 u,
                           fl::i32 ta, fl::i32 tb) FL_NO_EXCEPT {
    if (B == 0) {
        // Vertical line: x is constant, cov = clamp(u0, 0, 1) * (tb-ta)/65536
        // with u0 = u/D exact (u = D * u0).
        fl::i64 w = u / D;
        if (w < 0) w = 0;
        else if (w > 65536) w = 65536;
        return (fl::i32)(((w * (tb - ta)) + 8) >> 4);
    }
    const fl::i64 S = (B < 0) ? -(fl::i64)B : (fl::i64)B;
    const fl::i64 sa = (fl::i64)B * ta;
    const fl::i64 sb = (fl::i64)B * tb;
    // s = B*tau runs over [sa, sb] as tau runs [ta, tb]; for B < 0 the
    // interval is reversed — use min/max (the area is orientation-free).
    const fl::i64 s_lo = (sa < sb) ? sa : sb;
    const fl::i64 s_hi = (sa < sb) ? sb : sa;
    const fl::i64 c0 = -u;
    const fl::i64 c1 = (fl::i64)2 * D * 32768 - u;
    fl::u64 mhi = 0, mlo = 0;
    const fl::i64 lo = (s_lo > c0) ? s_lo : c0;
    const fl::i64 hi = (s_hi < c1) ? s_hi : c1;
    if (hi > lo) {
        // (u+lo) and (u+hi) each lie in [0, 2*D*32768] (lo >= c0, hi <= c1),
        // so both factors are non-negative and < 2^48.
        polyMul128((fl::u64)(hi - lo),
                   (fl::u64)((u + lo) + (u + hi)), mhi, mlo);
    }
    const fl::i64 lo2 = (s_lo > c1) ? s_lo : c1;
    if (s_hi > lo2) {
        fl::u64 phi = 0, plo = 0;
        polyMul128((fl::u64)(s_hi - lo2),
                   (fl::u64)((fl::i64)4 * D * 32768), phi, plo);
        polyAdd128(mhi, mlo, phi, plo);
    }
    if (mhi == 0 && mlo == 0) return 0;  // line misses the pixel in [ta, tb]
    // den = 32 * D * S as a u64 pair (D*S < 2^62, so den_hi = DS >> 59 and
    // den < 2^67).
    const fl::u64 DS = (fl::u64)S * (fl::u64)D;
    const fl::u64 dhi = DS >> 59;
    const fl::u64 dlo = DS << 5;
    // The loop below is a 64-bit remainder state machine
    // s <- 2s + bit, s -= dv when s >= dv. It is an exact long division
    // only while dv < 2^63 (s < dv keeps 2s + bit below 2^64). The
    // committed implementation always fed den itself, so for den >= 2^63
    // the carry out of the remainder's MSB was dropped (den in
    // [2^63, 2^64)) or the quotient collapsed to 0/1 (den >= 2^64), giving
    // wrong coverage for edges of dx*dy >= 2^26 px^2 (#5286). Those huge
    // edges instead divide M' = M >> 4 by dv = den >> 4 = 2*DS < 2^63:
    //   floor(M' / dv) == floor(M / den)   (nested floors collapse), and
    //   with R = 16*r' + (M mod 16), 2R >= den  <=>  2r' + bit3(M) >= dv
    //   <=>  2r' >= dv, because 2r' and dv are both even.
    // So quotient AND round-half-up are exact with the unchanged loop and
    // rounding in polyDivRoundT28; only the huge-edge setup is new.
    if (dhi != 0 || (dlo >> 63) != 0) {
        const fl::u64 nlo = (mlo >> 4) | (mhi << 60);  // M < 2^98, so
        return polyDivRoundT28(mhi >> 4, nlo, DS << 1);  // M >> 4 < 2^94
    }
    // Fast path: if mhi == 0 the numerator is < 2^64, so the loop's q
    // accumulation (at most 64 steps) cannot wrap its u64 and the whole
    // loop reduces to one exact 64/64 division plus the same round-half-up
    // and physical clamp. On AVR the division lowers to __udivdi3 (already
    // linked for the sort keys). dlo != 0 keeps this total for degenerate
    // direct calls (the committed loop never divided; drawPolygon always
    // feeds den > 0).
    if (dlo != 0 && mhi == 0) {
        fl::u64 q = mlo / dlo;
        const fl::u64 r = mlo - q * dlo;  // mlo % dlo, one division
        if ((r >> 63) != 0 || (r << 1) >= dlo) ++q;
        if (q > (fl::u64)kPolyOne) q = (fl::u64)kPolyOne;
        return (fl::i32)q;
    }
    return polyDivRoundT28(mhi, mlo, dlo);
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
/// Templated on Overwrite for compile-time dispatch (no per-pixel branch).
/// Uses (n+1)² = n² + 2n + 1 identity — zero multiplies in the inner loop.
template<typename PixelT, bool Overwrite>
inline void renderDiscRow(PixelT* buf, int w, int py,
                          fl::i32 d2_row,
                          const DiscCtx<PixelT>& f) FL_NO_EXCEPT {
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
        PixelT c = f.color; c.nscale8(br);
        if (Overwrite) *ptr = c; else *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 3: Full brightness interior (d2 hits minimum, then increases)
    while (px <= f.xmax && d2 < f.rin2) {
        if (Overwrite) *ptr = f.color; else *ptr += f.color;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 4: Outer AA fringe (right, d2 increasing)
    while (px <= f.xmax && d2 < f.rout2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(f.rout2 - d2) >> f.band_shift);
        fl::u8 br = static_cast<fl::u8>((diff * f.band_inv) >> 8);
        PixelT c = f.color; c.nscale8(br);
        if (Overwrite) *ptr = c; else *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
}

/// Render one scanline of a ring using incremental d² with phase-based scanning.
/// Templated on Overwrite for compile-time dispatch (no per-pixel branch).
/// Zones (left to right): outside → outer-AA → solid → inner-AA → hole →
///                         inner-AA → solid → outer-AA → outside.
template<typename PixelT, bool Overwrite>
inline void renderRingRow(PixelT* buf, int w, int py,
                          fl::i32 d2_row,
                          const RingCtx<PixelT>& g) FL_NO_EXCEPT {
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
        PixelT c = g.color; c.nscale8(br);
        if (Overwrite) *ptr = c; else *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 2: full-brightness band left (io2 <= d2 < oi2)
    while (px <= g.xmax && d2 >= g.io2 && d2 < g.oi2) {
        if (Overwrite) *ptr = g.color; else *ptr += g.color;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 3: inner-left AA fringe (ii2 <= d2 < io2)
    while (px <= g.xmax && d2 >= g.ii2 && d2 < g.io2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(d2 - g.ii2) >> g.inner_shift);
        fl::u8 br = static_cast<fl::u8>((diff * g.inner_inv) >> 8);
        PixelT c = g.color; c.nscale8(br);
        if (Overwrite) *ptr = c; else *ptr += c;
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
        PixelT c = g.color; c.nscale8(br);
        if (Overwrite) *ptr = c; else *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 6: full-brightness band right (io2 <= d2 < oi2)
    while (px <= g.xmax && d2 >= g.io2 && d2 < g.oi2) {
        if (Overwrite) *ptr = g.color; else *ptr += g.color;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
    // Phase 7: outer-right AA fringe (oi2 <= d2 < oo2)
    while (px <= g.xmax && d2 >= g.oi2 && d2 < g.oo2) {
        fl::u16 diff = static_cast<fl::u16>(static_cast<fl::u32>(g.oo2 - d2) >> g.outer_shift);
        fl::u8 br = static_cast<fl::u8>((diff * g.outer_inv) >> 8);
        PixelT c = g.color; c.nscale8(br);
        if (Overwrite) *ptr = c; else *ptr += c;
        d2 += xd; xd += 131072; ++ptr; ++px;
    }
}

/// Stroke line context: bundles per-line constants into a struct passed by
/// reference. This reduces function-call overhead on register-poor architectures
/// (AVR: many i32 params → 1 pointer), matching the struct-based approach used
/// in drawDisc/drawRing.
template<typename PixelT>
struct StrokeCtx {
    fl::i32 threshold_q;
    fl::i32 len2_q;
    fl::i32 dx_q, dy_q;
    fl::u8 aa_shift;
    fl::u16 aa_inv;
    fl::i32 x0_8, y0_8, x1_8, y1_8;
    fl::i32 r_max2_8;
    fl::u8 cap_shift;
    fl::u16 cap_inv;
    fl::i32 dot_ext_q;
    LineCap cap;
    PixelT color;
    int xmin, xmax;
};

/// Render one scanline of a stroke line using phase-based scanning.
/// Templated on Overwrite for compile-time dispatch (no per-pixel branch).
/// AA uses precomputed shift+reciprocal — zero Coord multiplies.
template<typename PixelT, bool Overwrite>
inline void renderStrokeRow(PixelT* buf, int w, int py,
                            fl::i32 cross_start, fl::i32 dot_start,
                            const StrokeCtx<PixelT>& sc) FL_NO_EXCEPT {
    PixelT* ptr = &buf[py * w + sc.xmin];
    fl::i32 cross = cross_start;
    fl::i32 dot = dot_start;
    int px = sc.xmin;
    const fl::i32 thr = sc.threshold_q;
    const fl::i32 neg_thr = -thr;

    // Phase 1: Skip pixels outside the band
    while (px <= sc.xmax && (cross >= thr || cross < neg_thr)) {
        cross += sc.dy_q;
        dot += sc.dx_q;
        ++ptr; ++px;
    }

    // Hoist per-row endcap ey values (py is constant within a row)
    fl::i32 py8 = static_cast<fl::i32>(py) << 8;
    fl::i32 ey8_y0 = py8 - sc.y0_8;
    fl::i32 ey8_y1 = py8 - sc.y1_8;

    // Phase 2: Process visible pixels (|cross| < threshold)
    while (px <= sc.xmax && cross < thr && cross >= neg_thr) {
        fl::i32 abs_cross = (cross < 0) ? -cross : cross;
        bool on_segment = (dot >= 0 && dot <= sc.len2_q);

        if (on_segment) {
            // Integer AA: shift+multiply (no Coord multiply)
            fl::u16 shifted = static_cast<fl::u16>(
                static_cast<fl::u32>(abs_cross) >> sc.aa_shift);
            fl::u8 linear_idx = static_cast<fl::u8>((shifted * sc.aa_inv) >> 8);
            // Fast approximate x/255 ≈ (x + 1 + (x >> 8)) >> 8
            fl::u16 sq = static_cast<fl::u16>(linear_idx) * linear_idx;
            fl::u8 sq_idx = static_cast<fl::u8>((sq + 1 + (sq >> 8)) >> 8);
            PixelT c = sc.color;
            c.nscale8(FL_PGM_READ_BYTE_NEAR(&distanceAA_LUT[sq_idx]));
            if (Overwrite) *ptr = c; else *ptr += c;
        } else if (sc.cap == LineCap::ROUND) {
            // Endpoint distance in 8.8 fixed-point
            fl::i32 ex8;
            fl::i16 ey_s;
            if (dot < 0) {
                ex8 = (static_cast<fl::i32>(px) << 8) - sc.x0_8;
                ey_s = static_cast<fl::i16>(ey8_y0);
            } else {
                ex8 = (static_cast<fl::i32>(px) << 8) - sc.x1_8;
                ey_s = static_cast<fl::i16>(ey8_y1);
            }
            fl::i16 ex_s = static_cast<fl::i16>(ex8);
            // 16×16→32 multiply (4 MUL on AVR vs ~70 for 32×32)
            fl::i32 ed2 = static_cast<fl::i32>(ex_s) * ex_s +
                          static_cast<fl::i32>(ey_s) * ey_s;
            if (ed2 < sc.r_max2_8) {
                fl::u16 shifted = static_cast<fl::u16>(
                    static_cast<fl::u32>(ed2) >> sc.cap_shift);
                fl::u8 idx = static_cast<fl::u8>((shifted * sc.cap_inv) >> 8);
                PixelT c = sc.color;
                c.nscale8(FL_PGM_READ_BYTE_NEAR(&distanceAA_LUT[idx]));
                if (Overwrite) *ptr = c; else *ptr += c;
            }
        } else if (sc.cap == LineCap::SQUARE) {
            if (dot >= -sc.dot_ext_q && dot <= sc.len2_q + sc.dot_ext_q) {
                fl::u16 shifted = static_cast<fl::u16>(
                    static_cast<fl::u32>(abs_cross) >> sc.aa_shift);
                fl::u8 linear_idx = static_cast<fl::u8>((shifted * sc.aa_inv) >> 8);
                fl::u16 sq = static_cast<fl::u16>(linear_idx) * linear_idx;
                fl::u8 sq_idx = static_cast<fl::u8>((sq + 1 + (sq >> 8)) >> 8);
                PixelT c = sc.color;
                c.nscale8(FL_PGM_READ_BYTE_NEAR(&distanceAA_LUT[sq_idx]));
                if (Overwrite) *ptr = c; else *ptr += c;
            }
        }
        // FLAT: pixels outside [0, len2] are silently dropped

        cross += sc.dy_q;
        dot += sc.dx_q;
        ++ptr; ++px;
    }
    // Phase 3: remaining pixels are outside the band — nothing to do
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
              Coord x0, Coord y0, Coord x1, Coord y1,
              fl::DrawMode mode) FL_NO_EXCEPT;

template<typename PixelT, typename Coord>
void drawDisc(Canvas<PixelT>& canvas, const PixelT& color,
              Coord cx, Coord cy, Coord r,
              fl::DrawMode mode) FL_NO_EXCEPT;

template<typename PixelT, typename Coord>
void drawRing(Canvas<PixelT>& canvas, const PixelT& color,
              Coord cx, Coord cy, Coord r, Coord thickness,
              fl::DrawMode mode) FL_NO_EXCEPT;

template<typename PixelT, typename Coord>
void drawStrokeLine(Canvas<PixelT>& canvas, const PixelT& color,
                    Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness,
                    LineCap cap, fl::DrawMode mode) FL_NO_EXCEPT;

template<typename PixelT, typename Coord>
void drawPolygon(Canvas<PixelT>& canvas, const PixelT& color,
                 fl::span<const PolyPoint<Coord>> pts,
                 fl::DrawMode mode) FL_NO_EXCEPT;

template<typename PixelT, typename Coord, int N>
void drawPolygon(Canvas<PixelT>& canvas, const PixelT& color,
                 const PolyPoint<Coord> (&pts)[N],
                 fl::DrawMode mode) FL_NO_EXCEPT;

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
namespace detail {

template<typename PixelT, typename Coord, bool Overwrite>
inline void drawLineCore(Canvas<PixelT>& canvas, const PixelT& color,
                         Coord x0, Coord y0, Coord x1, Coord y1) FL_NO_EXCEPT {
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
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, ypxl1, xpxl1, c);
            c = color; c.nscale8(b2);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, ypxl1 + 1, xpxl1, c);
        } else {
            PixelT c = color; c.nscale8(b1);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, xpxl1, ypxl1, c);
            c = color; c.nscale8(b2);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, xpxl1, ypxl1 + 1, c);
        }
    }

    // Draw second endpoint
    {
        fl::u8 b1 = static_cast<fl::u8>(((255 - yfrac2) * xgap2) >> 8);
        fl::u8 b2 = static_cast<fl::u8>((yfrac2 * xgap2) >> 8);
        if (steep) {
            PixelT c = color; c.nscale8(b1);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, ypxl2, xpxl2, c);
            c = color; c.nscale8(b2);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, ypxl2 + 1, xpxl2, c);
        } else {
            PixelT c = color; c.nscale8(b1);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, xpxl2, ypxl2, c);
            c = color; c.nscale8(b2);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, xpxl2, ypxl2 + 1, c);
        }
    }

    // Main loop: integer counter, 8.8 intery accumulation
    fl::i32 intery = yend + gradient;
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            int y = intery >> 8;
            fl::u8 frac = static_cast<fl::u8>(intery & 0xFF);
            PixelT c = color; c.nscale8(255 - frac);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, y, x, c);
            c = color; c.nscale8(frac);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, y + 1, x, c);
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            int y = intery >> 8;
            fl::u8 frac = static_cast<fl::u8>(intery & 0xFF);
            PixelT c = color; c.nscale8(255 - frac);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, x, y, c);
            c = color; c.nscale8(frac);
            addPixelToBuffer<PixelT, Overwrite>(pixels, width, height, x, y + 1, c);
            intery += gradient;
        }
    }
}

template<typename PixelT, typename Coord, bool Overwrite>
inline void drawDiscCore(Canvas<PixelT>& canvas, const PixelT& color,
                         Coord cx, Coord cy, Coord r) FL_NO_EXCEPT;

template<typename PixelT, typename Coord, bool Overwrite>
inline void drawRingCore(Canvas<PixelT>& canvas, const PixelT& color,
                         Coord cx, Coord cy, Coord r, Coord thickness) FL_NO_EXCEPT;

template<typename PixelT, typename Coord, bool Overwrite>
inline void drawStrokeLineCore(Canvas<PixelT>& canvas, const PixelT& color,
                               Coord x0, Coord y0, Coord x1, Coord y1,
                               Coord thickness, LineCap cap) FL_NO_EXCEPT;

template<typename PixelT, typename Coord, bool Overwrite, int MAXN, int CHUNK>
inline void drawPolygonCore(Canvas<PixelT>& canvas, const PixelT& color,
                            fl::span<const PolyPoint<Coord>> pts) FL_NO_EXCEPT;

}  // namespace detail

template<typename PixelT, typename Coord>
inline void drawLine(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord x0, Coord y0, Coord x1, Coord y1,
                     fl::DrawMode mode) FL_NO_EXCEPT {
    if (mode == fl::DrawMode::DRAW_MODE_OVERWRITE)
        detail::drawLineCore<PixelT, Coord, true>(canvas, color, x0, y0, x1, y1);
    else
        detail::drawLineCore<PixelT, Coord, false>(canvas, color, x0, y0, x1, y1);
}

// ---------------------------------------------------------------------------
// drawDiscCore: Templated on Overwrite for compile-time dispatch.
// ---------------------------------------------------------------------------
template<typename PixelT, typename Coord, bool Overwrite>
inline void detail::drawDiscCore(Canvas<PixelT>& canvas, const PixelT& color,
                                 Coord cx, Coord cy, Coord r) FL_NO_EXCEPT {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;

    fl::i32 cx8 = detail::toFixed8(cx);
    fl::i32 cy8 = detail::toFixed8(cy);
    fl::i32 r8 = detail::toFixed8(r);

    fl::i32 rin8 = r8 - 128;   // r - 0.5 in 8.8
    fl::i32 rout8 = r8 + 128;  // r + 0.5 in 8.8

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

    fl::i32 dx8 = (static_cast<fl::i32>(xmin) << 8) - cx8;
    fl::i32 dx2 = dx8 * dx8;

    detail::DiscCtx<PixelT> fc;
    fc.xdelta0 = 512 * dx8 + 65536;
    fc.xmin = xmin; fc.xmax = xmax;
    fc.rin2 = rin2; fc.rout2 = rout2;
    detail::computeBandShift(band, fc.band_shift, fc.band_inv);
    fc.color = color;

    fl::i32 cyfrac = (static_cast<fl::i32>(cyi) << 8) - cy8;
    fl::i32 d2c = dx2 + cyfrac * cyfrac;

    if (cyi >= 0 && cyi < height)
        detail::renderDiscRow<PixelT, Overwrite>(pixels, width, cyi, d2c, fc);

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
            detail::renderDiscRow<PixelT, Overwrite>(pixels, width, pyb, botd2, fc);
        if (ctop && pyt >= 0 && pyt < height)
            detail::renderDiscRow<PixelT, Overwrite>(pixels, width, pyt, topd2, fc);
        botd2 += botdelta; botdelta += 131072;
        topd2 += topdelta; topdelta += 131072;
    }
}

template<typename PixelT, typename Coord>
inline void drawDisc(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord cx, Coord cy, Coord r,
                     fl::DrawMode mode) FL_NO_EXCEPT {
    if (mode == fl::DrawMode::DRAW_MODE_OVERWRITE)
        detail::drawDiscCore<PixelT, Coord, true>(canvas, color, cx, cy, r);
    else
        detail::drawDiscCore<PixelT, Coord, false>(canvas, color, cx, cy, r);
}

// ---------------------------------------------------------------------------
// drawRingCore: Templated on Overwrite for compile-time dispatch.
// ---------------------------------------------------------------------------
template<typename PixelT, typename Coord, bool Overwrite>
inline void detail::drawRingCore(Canvas<PixelT>& canvas, const PixelT& color,
                                 Coord cx, Coord cy, Coord r, Coord thickness) FL_NO_EXCEPT {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;

    fl::i32 cx8 = detail::toFixed8(cx);
    fl::i32 cy8 = detail::toFixed8(cy);
    fl::i32 r8 = detail::toFixed8(r);
    fl::i32 t8 = detail::toFixed8(thickness);

    fl::i32 r_ii8 = r8 - 128;
    fl::i32 r_io8 = r8 + 128;
    fl::i32 r_oi8 = r8 + t8 - 128;
    fl::i32 r_oo8 = r8 + t8 + 128;

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
        detail::renderRingRow<PixelT, Overwrite>(pixels, width, cyi, d2c, gc);

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
            detail::renderRingRow<PixelT, Overwrite>(pixels, width, pyb, botd2, gc);
        if (ctop && pyt >= 0 && pyt < height)
            detail::renderRingRow<PixelT, Overwrite>(pixels, width, pyt, topd2, gc);
        botd2 += botdelta; botdelta += 131072;
        topd2 += topdelta; topdelta += 131072;
    }
}

template<typename PixelT, typename Coord>
inline void drawRing(Canvas<PixelT>& canvas, const PixelT& color,
                     Coord cx, Coord cy, Coord r, Coord thickness,
                     fl::DrawMode mode) FL_NO_EXCEPT {
    if (mode == fl::DrawMode::DRAW_MODE_OVERWRITE)
        detail::drawRingCore<PixelT, Coord, true>(canvas, color, cx, cy, r, thickness);
    else
        detail::drawRingCore<PixelT, Coord, false>(canvas, color, cx, cy, r, thickness);
}

// ---------------------------------------------------------------------------
// drawStrokeLineCore: Templated on Overwrite for compile-time dispatch.
// ---------------------------------------------------------------------------
template<typename PixelT, typename Coord, bool Overwrite>
inline void detail::drawStrokeLineCore(Canvas<PixelT>& canvas, const PixelT& color,
                                       Coord x0, Coord y0, Coord x1, Coord y1,
                                       Coord thickness, LineCap cap) FL_NO_EXCEPT {
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;

    fl::i32 x0_8 = detail::toFixed8(x0), y0_8 = detail::toFixed8(y0);
    fl::i32 x1_8 = detail::toFixed8(x1), y1_8 = detail::toFixed8(y1);
    fl::i32 dx_8 = x1_8 - x0_8;
    fl::i32 dy_8 = y1_8 - y0_8;
    if (dx_8 == 0 && dy_8 == 0) return;

    fl::i32 len2_16 = dx_8 * dx_8 + dy_8 * dy_8;
    fl::i32 len_8 = static_cast<fl::i32>(
        fl::isqrt32(static_cast<fl::u32>(len2_16)));
    fl::i32 thickness_8 = detail::toFixed8(thickness);
    fl::i32 r_max_8 = thickness_8 >> 1;
    fl::i32 threshold_q = r_max_8 * len_8;

    if (threshold_q <= 0) return;

    int x0i = x0_8 >> 8, y0i = y0_8 >> 8;
    int x1i = x1_8 >> 8, y1i = y1_8 >> 8;
    int margin = ((r_max_8 + 255) >> 8) + 2;
    int xmin = (x0i < x1i ? x0i : x1i) - margin;
    int xmax = (x0i > x1i ? x0i : x1i) + margin;
    int ymin = (y0i < y1i ? y0i : y1i) - margin;
    int ymax = (y0i > y1i ? y0i : y1i) + margin;

    if (xmin < 0) xmin = 0;
    if (xmax >= width) xmax = width - 1;
    if (ymin < 0) ymin = 0;
    if (ymax >= height) ymax = height - 1;
    if (xmin > xmax || ymin > ymax) return;

    fl::i32 dx_q = dx_8 << 8;
    fl::i32 dy_q = dy_8 << 8;

    fl::i32 rx_base_8 = (static_cast<fl::i32>(xmin) << 8) - x0_8;
    fl::i32 ry_row_8 = (static_cast<fl::i32>(ymin) << 8) - y0_8;
    fl::i32 cross_row_q = rx_base_8 * dy_8 - ry_row_8 * dx_8;
    fl::i32 dot_row_q = rx_base_8 * dx_8 + ry_row_8 * dy_8;

    fl::i32 len2_q = len_8 * len_8;
    fl::i32 dot_ext_q = (cap == LineCap::SQUARE) ? threshold_q : 0;

    detail::StrokeCtx<PixelT> sc;
    sc.threshold_q = threshold_q;
    sc.len2_q = len2_q;
    sc.dx_q = dx_q;
    sc.dy_q = dy_q;
    sc.dot_ext_q = dot_ext_q;
    detail::computeBandShift(threshold_q, sc.aa_shift, sc.aa_inv);
    sc.x0_8 = x0_8; sc.y0_8 = y0_8;
    sc.x1_8 = x1_8; sc.y1_8 = y1_8;
    sc.r_max2_8 = r_max_8 * r_max_8;
    sc.cap_shift = 0; sc.cap_inv = 0;
    if (cap == LineCap::ROUND && sc.r_max2_8 > 0) {
        detail::computeBandShift(sc.r_max2_8, sc.cap_shift, sc.cap_inv);
    }
    sc.cap = cap;
    sc.color = color;
    sc.xmin = xmin; sc.xmax = xmax;

    for (int py = ymin; py <= ymax; ++py) {
        if (py >= 0 && py < height) {
            detail::renderStrokeRow<PixelT, Overwrite>(pixels, width, py,
                                                       cross_row_q, dot_row_q, sc);
        }
        cross_row_q -= sc.dx_q;
        dot_row_q += sc.dy_q;
    }
}

template<typename PixelT, typename Coord>
inline void drawStrokeLine(Canvas<PixelT>& canvas, const PixelT& color,
                           Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness,
                           LineCap cap, fl::DrawMode mode) FL_NO_EXCEPT {
    if (mode == fl::DrawMode::DRAW_MODE_OVERWRITE)
        detail::drawStrokeLineCore<PixelT, Coord, true>(canvas, color, x0, y0, x1, y1, thickness, cap);
    else
        detail::drawStrokeLineCore<PixelT, Coord, false>(canvas, color, x0, y0, x1, y1, thickness, cap);
}

// ---------------------------------------------------------------------------
// drawPolygonCore: even-odd fill with an exact per-pixel area model,
// organized as row bands.
//
// Each non-horizontal edge is active in every canvas row it geometrically
// occupies (py0 = the row containing ytop, py1 = the row containing ybot),
// and each row is split into sub-bands at vertex events (vertices whose y
// falls strictly inside the row). Within a band the crossing set is
// constant: an edge participates iff ytop < band_top && ybot > band_bottom
// (strict — at each vertex one edge ends and the other begins, so the
// crossing count is the same in every band of a closed polygon). Edges in a
// band are ordered by their floored Q16.16 x at the band midpoint (the
// committed implementation's midpoint key, polySortBand — stable on equal
// keys) and paired (0,1), (2,3),... For simple polygons this is exactly
// the even-odd rule; self-intersecting polygons follow the same
// deterministic span-pairing convention.
//
// The per-row scan is division-free: the crossing x (anchored at the row
// center, which polyLineT28 integrates from) advances by the Bresenham
// step/rem recurrence, and the per-row column extent is (x +/- hw) >> 16 —
// a shift — where hw = ceil(32768*|B|/dy) was computed once at setup.
// (MAXN, CHUNK) bound the
// stack: MAXN caps the vertex count, CHUNK the per-row accumulator width
// (CHUNK must be a power of two).
//
// Pixel coverage is accumulated additively over the row's bands into a
// CHUNK-px accumulator in T units (kPolyOne = 2^28 = full pixel), then
// quantized once per pixel: br = (acc*255 + ONE/2) >> 28. Coverage is
// exact for linear edges (see polyLineT28): for a simple polygon the
// per-pixel sum equals the true sub-pixel area, so total energy equals the
// shoelace area up to u8 rounding.
//
// Vertices beyond MAXN are ignored (the polygon closes from #MAXN-1 back to
// #0). Stack scales with the tier: vertices (8*MAXN B) + edges (56*MAXN B)
// + accb (4*CHUNK B) + per-row scratch + band prepass bidx/bnc/bfull
// ((MAXN+1)*(MAXN+5) B). Measured frames and the platform-dependent default
// tier are documented at FL_GFX_POLYGON_DEFAULT_MAXN in fl/gfx/canvas.h.
// ---------------------------------------------------------------------------
template<typename PixelT, typename Coord, bool Overwrite, int MAXN, int CHUNK>
inline void detail::drawPolygonCore(Canvas<PixelT>& canvas, const PixelT& color,
                                    fl::span<const PolyPoint<Coord>> pts) FL_NO_EXCEPT {
    FL_STATIC_ASSERT(MAXN >= 3 && MAXN <= 256, "drawPolygon: MAXN must be in [3, 256] (edge indices are u8)");
    FL_STATIC_ASSERT(CHUNK > 0 && (CHUNK & (CHUNK - 1)) == 0, "drawPolygon: CHUNK must be a power of two");
    const fl::i32 n = static_cast<fl::i32>(pts.size());
    if (n < 3) return;
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;
    if (width <= 0 || height <= 0) return;

    const int nv = (n < MAXN) ? n : MAXN;

    // Convert vertices to Q16.16 (clamped to ±8191 px — i32-safe)
    fl::i32 vx[MAXN];
    fl::i32 vy[MAXN];
    for (int i = 0; i < nv; ++i) {
        vx[i] = detail::toQ16Clamped(pts[i].x);
        vy[i] = detail::toQ16Clamped(pts[i].y);
    }

    // Build per-edge scanline state. Two i64 floor divisions per edge
    // (anchor crossing + step) are the only divisions in this function.
    PolyEdge edges[MAXN];
    int nedges = 0;
    int ymin = height, ymax = -1;
    for (int i = 0; i < nv; ++i) {
        const int j = (i + 1 == nv) ? 0 : (i + 1);
        fl::i32 dy = vy[j] - vy[i];
        if (dy == 0) continue;  // horizontal edge: ignored (even-odd convention)
        // Orient edge top-to-bottom: B measured from the top vertex
        fl::i32 ytop, xtop, B;
        if (dy > 0) { ytop = vy[i]; xtop = vx[i]; B = vx[j] - vx[i]; }
        else        { ytop = vy[j]; xtop = vx[j]; B = vx[i] - vx[j]; }
        const fl::i32 dy2 = (dy > 0) ? dy : -dy;  // span height, > 0
        const fl::i32 ybot = ytop + dy2;

        // Active pixel rows: the row containing ytop through the row
        // containing ybot (floor/ceil rule; rows are half-open so a vertex
        // on a row boundary belongs to exactly one edge's row range).
        int py0 = ytop >> 16;
        int py1 = (ybot - 1) >> 16;
        if (py0 < 0) py0 = 0;
        if (py1 >= height) py1 = height - 1;
        if (py0 > py1) continue;  // edge off-canvas vertically

        PolyEdge& ed = edges[nedges++];
        ed.py0 = py0;
        ed.py1 = py1;
        ed.dy = dy2;
        ed.B = B;
        ed.ytop = ytop;
        ed.ybot = ybot;

        // Exact crossing at py0's row center (one i64 floor division):
        // x = xtop + floor(w0 * B / dy2), w0 = (py0<<16) + 32768 - ytop
        const fl::i64 w0 = (static_cast<fl::i64>(py0) << 16) + 32768 - ytop;
        const fl::i64 prod = w0 * B;
        const fl::i64 q = detail::floorDivI64(prod, dy2);
        ed.x = xtop + q;
        // 65536*B = s*dy2 + rem, rem in [0, dy2). s is the per-row x
        // advance (used by the carry on multi-row edges only) and, with
        // rem, yields the row's x half-span hw = ceil(32768*|B|/dy2)
        // without a second division:
        //   B >= 0: 2*hw*dy2 = s*dy2 + rem ->
        //           hw = (s>>1) + (s&1) + (s even && rem > 0)
        //   B <  0: -s = ceil(65536*|B|/dy2) -> hw = ceil(-s/2)
        const fl::i64 p2 = static_cast<fl::i64>(65536) * B;
        const fl::i64 s = detail::floorDivI64(p2, dy2);
        const fl::i32 rem = static_cast<fl::i32>(p2 - s * dy2);
        ed.hw = (B >= 0)
            ? ((s >> 1) + (s & 1) + ((s & 1) == 0 && rem > 0 ? 1 : 0))
            : (((-s) >> 1) + ((-s) & 1));
        if (dy2 >= 65536) {
            ed.step = s;
            ed.rem = rem;
            ed.t = static_cast<fl::i32>(prod - q * dy2);  // in [0, dy2)
        } else {
            // Span < 1 px: may still span two canvas rows (a row-boundary
            // crossing), but the carry stays a no-op (step = rem = t = 0):
            // x keeps its py0-row value and the per-row extent (x +/- hw)
            // boxes the line the kernel integrates — see PolyEdge.
            ed.step = 0;
            ed.rem = 0;
            ed.t = 0;
        }
        if (ed.py0 < ymin) ymin = ed.py0;
        if (ed.py1 > ymax) ymax = ed.py1;
    }
    if (nedges == 0) return;

    // Scan rows
    fl::i32 ev[MAXN + 2];  // band boundaries: row ends + vertex y's in row
    fl::u8 idx[MAXN];      // a band's active edges, sorted by midpoint key
    fl::i64 keys[MAXN];    // per-edge midpoint sort keys (see polySortBand)
    fl::i32 lx0a[MAXN];    // per-row per-edge column extents (left, right)
    fl::i32 lx1a[MAXN];
    fl::i32 accb[CHUNK];   // per-row CHUNK-px coverage accumulator (T units)

    for (int py = ymin; py <= ymax; ++py) {
        const fl::i32 yc = (static_cast<fl::i32>(py) << 16) + 32768;

        // Band boundaries: the row ends plus every vertex strictly inside
        // the row (a vertex exactly on a row boundary coincides with a band
        // edge — the floor/ceil row rule already splits the edges there).
        int nev = 0;
        ev[nev++] = -32768;
        ev[nev++] = 32768;
        for (int i = 0; i < nv; ++i) {
            if ((vy[i] >> 16) == py && (vy[i] & 0xFFFF) != 0)
                ev[nev++] = vy[i] - yc;
        }
        // Insertion sort (nev <= MAXN + 2)
        for (int a = 1; a < nev; ++a) {
            const fl::i32 v = ev[a];
            int b = a - 1;
            while (b >= 0 && ev[b] > v) { ev[b + 1] = ev[b]; --b; }
            ev[b + 1] = v;
        }

        // Per-row column extents of each active edge (x is at the row
        // center): the line stays within x +/- hw over the row, so px
        // outside [lx0, lx1] is a full/zero shortcut for polyLineT28.
        // floor(v / 65536) == v >> 16 — shifts, not divisions. The loop
        // also yields the row extent actually touched (xmin/xmax): a span
        // pair's range is [min(L.lx0, R.lx0), max(L.lx1, R.lx1)] — it
        // can reach BETWEEN two off-canvas edges (e.g. a polygon covering
        // the whole canvas), so the bounds are the min lx0 / max lx1 over
        // ALL row-active edges, clamped once. Pairs partition the edges
        // (odd count: the unpaired last edge only widens the range), so
        // this superset can only add no-op chunks that the fill loop
        // skips; the exact-x ordering of polySortBand only affects span
        // pairing.
        fl::i32 minLx0 = 0, maxLx1 = 0;
        int nactive = 0;
        for (int e = 0; e < nedges; ++e) {
            const PolyEdge& ed = edges[e];
            if (py < ed.py0 || py > ed.py1) continue;
            lx0a[e] = static_cast<fl::i32>(detail::floorDiv16I64(ed.x - ed.hw));
            lx1a[e] = static_cast<fl::i32>(detail::floorDiv16I64(ed.x + ed.hw));
            if (nactive++ == 0) { minLx0 = lx0a[e]; maxLx1 = lx1a[e]; }
            else {
                if (lx0a[e] < minLx0) minLx0 = lx0a[e];
                if (lx1a[e] > maxLx1) maxLx1 = lx1a[e];
            }
        }
        int xmin = minLx0, xmax = maxLx1;
        if (nactive == 0) { xmin = width; xmax = -1; }
        else {
            if (xmin < 0) xmin = 0;
            if (xmax >= width) xmax = width - 1;
        }

        // Per-band active-edge list and its midpoint order (polySortBand)
        // depend only on the row and the band bounds a, b — never on the
        // column chunk. Compute them once per row (bidx/bnc/bfull) and
        // reuse for every chunk: the committed code rebuilt and re-sorted
        // each band for every chunk (one i64 sort key per active edge per
        // chunk — pure redundancy for CHUNK == 1, where there is one chunk
        // per pixel column). Rows whose extent is empty (off-canvas
        // polygon) skip the prepass entirely: nothing would be filled.
        fl::u8 bidx[MAXN + 1][MAXN];
        fl::u8 bnc[MAXN + 1];
        fl::i32 bfull[MAXN + 1];
        if (xmin <= xmax) {
            for (int k = 0; k + 1 < nev; ++k) {
                const fl::i32 a = ev[k], b = ev[k + 1];
                int nc = 0;
                if (a < b) {
                    for (int e = 0; e < nedges; ++e) {
                        const PolyEdge& ed = edges[e];
                        if (py >= ed.py0 && py <= ed.py1 &&
                            ed.ytop < yc + b && ed.ybot > yc + a)
                            idx[nc++] = (fl::u8)e;
                    }
                    if (nc >= 2) polySortBand(idx, keys, edges, nc, a + b);
                    // Full-pixel coverage over this band: (b - a) *
                    // kPolyOne >> 16 == (b - a) << 12 exactly (0 < b - a
                    // < 2^16: both bounds lie inside the row).
                    bfull[k] = (b - a) << 12;
                }
                bnc[k] = (fl::u8)nc;
                for (int e = 0; e < nc; ++e) bidx[k][e] = idx[e];
            }
        }

        // Fill CHUNK-px chunks: accumulate coverage over the bands, then
        // quantize once per pixel.
        // An empty row extent (xmin > xmax, e.g. every active edge right of
        // the canvas) skipped the prepass, so bnc/bidx are uninitialized:
        // start past xmax so the chunk loop never reads them.
        const int csFirst = (xmin <= xmax) ? (xmin & ~(CHUNK - 1)) : xmax + 1;
        for (int cs = csFirst; cs <= xmax; cs += CHUNK) {
            for (int i = 0; i < CHUNK; ++i) accb[i] = 0;
            const int ce = (cs + CHUNK <= xmax + 1) ? cs + CHUNK : xmax + 1;
            for (int k = 0; k + 1 < nev; ++k) {
                const int nc = bnc[k];
                if (nc < 2) continue;
                const fl::i32 a = ev[k], b = ev[k + 1];
                // Full-pixel coverage over this band (row prepass).
                const fl::i64 full = (fl::i64)bfull[k];
                for (int p = 0; p + 1 < nc; p += 2) {
                    const PolyEdge& L = edges[bidx[k][p]];
                    const PolyEdge& R = edges[bidx[k][p + 1]];
                    const fl::i32 Ll0 = lx0a[bidx[k][p]], Ll1 = lx1a[bidx[k][p]];
                    const fl::i32 Rl0 = lx0a[bidx[k][p + 1]], Rl1 = lx1a[bidx[k][p + 1]];
                    int pl = (Ll0 < Rl0) ? Ll0 : Rl0;
                    if (pl < cs) pl = cs;
                    int ph = (Ll1 > Rl1) ? Ll1 : Rl1;
                    if (ph >= ce) ph = ce - 1;
                    if (pl > ph) continue;
                    fl::i64 uL = (fl::i64)L.dy * (L.x - ((fl::i64)pl << 16));
                    fl::i64 uR = (fl::i64)R.dy * (R.x - ((fl::i64)pl << 16));
                    const fl::i64 suL = -((fl::i64)L.dy << 16);
                    const fl::i64 suR = -((fl::i64)R.dy << 16);
                    for (int px = pl; px <= ph; ++px) {
                        fl::i32 TL = (px > Ll1) ? 0
                            : (px < Ll0) ? (fl::i32)full
                            : polyLineT28(L.B, L.dy, uL, a, b);
                        fl::i32 TR = (px > Rl1) ? 0
                            : (px < Rl0) ? (fl::i32)full
                            : polyLineT28(R.B, R.dy, uR, a, b);
                        const fl::i32 d = TR - TL;
                        if (d > 0) accb[px - cs] += d;
                        uL += suL;
                        uR += suR;
                    }
                }
            }
            PixelT* row = pixels + py * width + cs;
            for (int i = 0; i < ce - cs; ++i) {
                const fl::i32 v = accb[i];
                if (v <= 0) continue;
                const fl::u8 br = (v >= kPolyOne)
                    ? 255
                    : static_cast<fl::u8>(((fl::u64)v * 255u
                        + (fl::u64)(kPolyOne >> 1)) >> 28);
                if (br == 0) continue;
                PixelT c = color;
                c.nscale8(br);
                if (Overwrite) row[i] = c;
                else row[i] += c;
            }
        }

        // Carry edge crossings to the next row's center (exact; t + rem
        // < 2*dy < 2^31, so the signed compare is safe)
        for (int e = 0; e < nedges; ++e) {
            PolyEdge& ed = edges[e];
            if (py >= ed.py0 && py <= ed.py1) {
                ed.x += ed.step;
                ed.t += ed.rem;
                if (ed.t >= ed.dy) {
                    ed.t -= ed.dy;
                    ++ed.x;
                }
            }
        }
    }
}

/// MAXN caps the vertex count and CHUNK the per-row accumulator width in
/// px (must be a power of two). The defaults (FL_GFX_POLYGON_DEFAULT_MAXN /
/// _CHUNK in fl/gfx/canvas.h) are 32 / 64 on FL_PLATFORM_HAS_LARGE_MEMORY
/// targets and 8 / 16 elsewhere, because the 32 / 64 frame does not fit in
/// an AVR Uno's 2 KB of SRAM (measured figures at the macro definition).
/// Smaller tiers shrink the stack -- the rendered pixels are identical for
/// polygons with at most MAXN vertices.
///
/// Fill selects the backend: PolyFill::Exact (default, exact per-pixel
/// area) or PolyFill::ABuffer (8x4 A-buffer sampling; CHUNK is ignored).
template<typename PixelT, typename Coord, int MAXN, int CHUNK, PolyFill Fill>
inline void drawPolygon(Canvas<PixelT>& canvas, const PixelT& color,
                        fl::span<const PolyPoint<Coord>> pts,
                        fl::DrawMode mode) FL_NO_EXCEPT {
    if (Fill == PolyFill::ABuffer) {
        if (mode == fl::DrawMode::DRAW_MODE_OVERWRITE)
            detail::drawPolygonABufferCore<PixelT, Coord, true, MAXN>(canvas, color, pts);
        else
            detail::drawPolygonABufferCore<PixelT, Coord, false, MAXN>(canvas, color, pts);
    } else if (mode == fl::DrawMode::DRAW_MODE_OVERWRITE)
        detail::drawPolygonCore<PixelT, Coord, true, MAXN, CHUNK>(canvas, color, pts);
    else
        detail::drawPolygonCore<PixelT, Coord, false, MAXN, CHUNK>(canvas, color, pts);
}

template<typename PixelT, typename Coord, int N, int MAXN, int CHUNK, PolyFill Fill>
inline void drawPolygon(Canvas<PixelT>& canvas, const PixelT& color,
                        const PolyPoint<Coord> (&pts)[N],
                        fl::DrawMode mode) FL_NO_EXCEPT {
    drawPolygon<PixelT, Coord, MAXN, CHUNK, Fill>(
        canvas, color, fl::span<const PolyPoint<Coord>>(pts, N), mode);
}

}  // namespace gfx
}  // namespace fl

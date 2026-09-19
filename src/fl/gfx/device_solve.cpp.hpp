// ok no header - implementation for fl/gfx/device_solve.h

#include "fl/gfx/device_solve.h"

#include "fl/stl/bit_cast.h"

namespace fl {

namespace {

// Helpers carry a `solve` qualifier: .cpp.hpp files share one translation
// unit under the unity build, so an anonymous namespace does not isolate them
// from same-named helpers in sibling files.

/// Largest coefficient magnitude the build accepts, in whole units.
///
/// Chosen so the i64 accumulator cannot overflow for *any* i32 input:
/// 3 * (kMaxCoefficient << 16) * 2^31 stays under 2^63. Real emitter
/// inverses sit near 1, and the largest column entry in a plausible profile
/// is the blue emitter's Z at about 13, so this is enormously permissive
/// while still being a proof rather than an assumption.
constexpr i32 kMaxCoefficient = 21845;  // floor(2^32 / 3) >> 16

constexpr i64 kI64Max = 9223372036854775807LL;

/// One, in s16.16.
constexpr i64 kQ16One = 65536;

/// Does `a * b` fit an i64?
///
/// By division rather than by bounding the inputs, because the inputs cannot
/// usefully be bounded here -- see `invert3x3Q16`. Division is exact on every
/// target and needs no builtin; `__builtin_mul_overflow` would be cheaper but
/// MSVC does not have it and the host suite builds there.
bool productFitsI64(i64 a, i64 b) FL_NO_EXCEPT {
    if (a == 0 || b == 0) {
        return true;
    }
    const i64 lhs = a < 0 ? -a : a;
    const i64 rhs = b < 0 ? -b : b;
    return rhs <= kI64Max / lhs;
}

/// Does `a + b` fit an i64, for operands already known to be in range?
///
/// The negative bound stops one short of i64's minimum, deliberately.
/// `divideCoefficientQ16` and `roundedDivI64` both negate the determinant,
/// and negating the minimum is undefined behaviour -- so this is what
/// guarantees they never see it.
///
/// It is reachable, not theoretical. A single term cannot be the minimum
/// because `productFitsI64` rejects any product that large, but a sum can:
/// 2^63 - 1 factors as 7^2 * 73 * 127 * 337 * 92737 * 649657, so one term can
/// be made exactly -(2^63 - 1) from i32 entries and a second can contribute
/// the remaining -1. `tests/fl/gfx/device_solve.cpp` carries the matrix.
bool sumFitsI64(i64 a, i64 b) FL_NO_EXCEPT {
    if (b > 0) {
        return a <= kI64Max - b;
    }
    if (b < 0) {
        return a >= -kI64Max - b;
    }
    return true;
}

/// `e * i - f * h`, or false if any part of it leaves i64.
bool cofactorQ32(i32 e, i32 i_, i32 f, i32 h, i64* out) FL_NO_EXCEPT {
    const i64 left = static_cast<i64>(e) * static_cast<i64>(i_);
    const i64 right = static_cast<i64>(f) * static_cast<i64>(h);
    // Two i32 products always fit; their difference need not.
    if (!sumFitsI64(left, -right)) {
        return false;
    }
    *out = left - right;
    return true;
}

/// Nearest integer, halves away from zero. Truncating would bias every
/// coefficient toward zero, and the bias does not cancel across a row --
/// a solve is a sum of three of them.
i64 roundedDivI64(i64 numerator, i64 denominator) FL_NO_EXCEPT {
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    const i64 half = denominator / 2;
    if (numerator >= 0) {
        return (numerator + half) / denominator;
    }
    return -((-numerator + half) / denominator);
}

/// `round(cofactor_q32 * 2^32 / determinant_q48)`, in s16.16, without a
/// 128-bit intermediate.
///
/// The obvious form is `(cofactor << 32) / determinant`, which is what the
/// host study computes with unbounded integers. A target without `__int128`
/// cannot form that numerator: the cofactor is already Q32, and 32 more bits
/// puts it past i64 for any matrix that matters.
///
/// So each of the 32 doublings is spent where there is room -- on the
/// numerator while it still fits, on the denominator otherwise. Both raise
/// the quotient by the same factor, so the only loss is what halving the
/// denominator rounds away, and the determinant carries far more bits than a
/// s16.16 answer needs.
///
/// Checked against the exact divide over `ci/color_fixed_inverse_study.py`'s
/// corpus, six degrees of primary collapse, and 20,000 random primary sets:
/// **zero** disagreement, and 72% of coefficients needed at least one
/// denominator halving, up to 11 -- so the staged path is the one being
/// exercised, not a branch nothing reaches.
i64 divideCoefficientQ16(i64 cofactor_q32, i64 determinant_q48) FL_NO_EXCEPT {
    i64 numerator = cofactor_q32;
    i64 denominator = determinant_q48;
    // Safe to negate: `sumFitsI64` refuses a determinant of i64's minimum, so
    // the caller cannot pass the one value this would be undefined for.
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    constexpr i64 kHalfMax = kI64Max / 2;
    for (int spent = 0; spent < 32; ++spent) {
        const i64 magnitude = numerator < 0 ? -numerator : numerator;
        if (magnitude <= kHalfMax) {
            numerator *= 2;
        } else {
            denominator = (denominator + 1) / 2;
        }
    }
    return roundedDivI64(numerator, denominator);
}

/// Nearest integer of `numerator / denominator`, halves away from zero.
///
/// Truncating would bias every column toward zero and the bias does not
/// cancel: the inverse is built from products of these.
i64 roundedDivideQ16(i64 numerator, i64 denominator) FL_NO_EXCEPT {
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    const i64 half = denominator / 2;
    if (numerator >= 0) {
        return (numerator + half) / denominator;
    }
    return -((-numerator + half) / denominator);
}

/// One emitter's XYZ column at its own luminance, all in s16.16.
///
/// `xyY_to_XYZ` is `x * Y * inv_y`, which C++ groups left to right and so
/// scales before it divides. This divides first, deliberately: in fixed point
/// `x * Y` discards low bits the divide would have used. Measured against the
/// float derivation over the study's corpus and four luminance sets, the
/// worst coefficient error is 2899 ULP dividing first against 7363 scaling
/// first, and the colour figure is the same either way. See
/// `ci/color_fixed_profile_study.py`.
bool emitterColumnQ16(const i32 (&xy)[2], i32 luminance,
                      i64 (&column)[3]) FL_NO_EXCEPT {
    const i32 x = xy[0];
    const i32 y = xy[1];
    // The float guard rejects a `y` at or below 1e-6; in Q16 anything under
    // half a step quantises to zero, and dividing by it is the failure the
    // float path cannot have.
    if (y <= 0 || luminance <= 0 || x <= 0) {
        return false;
    }
    if (static_cast<i64>(x) + static_cast<i64>(y) > kQ16One) {
        // Outside the CIE simplex, so z would be negative for a physical
        // emitter.
        return false;
    }
    const i64 z = kQ16One - static_cast<i64>(x) - static_cast<i64>(y);
    const i64 x_over_y = roundedDivideQ16(static_cast<i64>(x) * kQ16One, y);
    const i64 z_over_y = roundedDivideQ16(z * kQ16One, y);
    column[0] = roundedDivideQ16(x_over_y * luminance, kQ16One);
    column[1] = luminance;
    column[2] = roundedDivideQ16(z_over_y * luminance, kQ16One);
    for (int i = 0; i < 3; ++i) {
        // The inverse works in i32, so a column that does not fit is refused
        // here rather than wrapped on the way in.
        if (column[i] > 2147483647LL || column[i] < -2147483648LL) {
            return false;
        }
    }
    return true;
}

i32 dotSolveRowQ16(const i32 (&row)[3], const i32 (&v)[3]) FL_NO_EXCEPT {
    const i64 acc = static_cast<i64>(row[0]) * static_cast<i64>(v[0])
                  + static_cast<i64>(row[1]) * static_cast<i64>(v[1])
                  + static_cast<i64>(row[2]) * static_cast<i64>(v[2]);
    const i64 rounded = acc >= 0 ? (acc + 32768) >> 16
                                 : -((-acc + 32768) >> 16);
    // Saturate rather than wrap. An extreme target can still land outside
    // s16.16 after the shift, and wrapping would turn an out-of-gamut
    // overshoot into a wildly wrong colour of the opposite sign, which the
    // gamut mapper downstream would then treat as legitimate.
    constexpr i64 kMax = 2147483647;
    constexpr i64 kMin = -2147483647 - 1;
    if (rounded > kMax) {
        return static_cast<i32>(kMax);
    }
    if (rounded < kMin) {
        return static_cast<i32>(kMin);
    }
    return static_cast<i32>(rounded);
}

}  // namespace

bool invert3x3Q16(const i32 (&in)[3][3], i32 (&out)[3][3]) FL_NO_EXCEPT {
    const i32 a = in[0][0], b = in[0][1], c = in[0][2];
    const i32 d = in[1][0], e = in[1][1], f = in[1][2];
    const i32 g = in[2][0], h = in[2][1], i_ = in[2][2];

    // Cofactors are differences of two Q16 products, so Q32.
    i64 cof[3][3];
    if (!cofactorQ32(e, i_, f, h, &cof[0][0]) ||
        !cofactorQ32(c, h, b, i_, &cof[0][1]) ||
        !cofactorQ32(b, f, c, e, &cof[0][2]) ||
        !cofactorQ32(f, g, d, i_, &cof[1][0]) ||
        !cofactorQ32(a, i_, c, g, &cof[1][1]) ||
        !cofactorQ32(c, d, a, f, &cof[1][2]) ||
        !cofactorQ32(d, h, e, g, &cof[2][0]) ||
        !cofactorQ32(b, g, a, h, &cof[2][1]) ||
        !cofactorQ32(a, e, b, d, &cof[2][2])) {
        return false;
    }

    // Determinant is a Q16 times a Q32, so Q48.
    const i32 first_row[3] = {a, b, c};
    i64 determinant = 0;
    for (int col = 0; col < 3; ++col) {
        const i64 term_lhs = static_cast<i64>(first_row[col]);
        if (!productFitsI64(term_lhs, cof[col][0])) {
            return false;
        }
        const i64 term = term_lhs * cof[col][0];
        if (!sumFitsI64(determinant, term)) {
            return false;
        }
        determinant += term;
    }
    if (determinant == 0) {
        return false;
    }

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const i64 value = divideCoefficientQ16(cof[row][col], determinant);
            // The same bound the float path applies to its own result, and
            // for the same reason: past it the solve's accumulator can
            // overflow for an in-range XYZ input.
            const i64 limit = static_cast<i64>(kMaxCoefficient) << 16;
            if (value > limit || value < -limit) {
                return false;
            }
            out[row][col] = static_cast<i32>(value);
        }
    }
    return true;
}

bool buildRgbSolveMatrixFromQ16(const EmitterChromaticitiesQ16& profile,
                                EmitterSolveMatrixQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    i64 red[3];
    i64 green[3];
    i64 blue[3];
    if (!emitterColumnQ16(profile.xy_r, profile.lum_r, red) ||
        !emitterColumnQ16(profile.xy_g, profile.lum_g, green) ||
        !emitterColumnQ16(profile.xy_b, profile.lum_b, blue)) {
        return false;
    }

    // Columns are the emitters' XYZ contributions at full drive, the same
    // arrangement `buildRgbSolveMatrixQ16` builds in float.
    const i32 emitter[3][3] = {
        {static_cast<i32>(red[0]), static_cast<i32>(green[0]),
         static_cast<i32>(blue[0])},
        {static_cast<i32>(red[1]), static_cast<i32>(green[1]),
         static_cast<i32>(blue[1])},
        {static_cast<i32>(red[2]), static_cast<i32>(green[2]),
         static_cast<i32>(blue[2])},
    };
    return invert3x3Q16(emitter, out->m);
}

bool q16FromFloatBits(float value, i32* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    // Integer arithmetic on the IEEE-754 single-precision bits: sign, 8-bit
    // biased exponent, 23-bit mantissa. A float->int cast would be a call to
    // a soft-float helper (__aeabi_f2iz) on a part without an FPU, which is
    // exactly what the bind path must not reach (FastLED#4458).
    const u32 bits = fl::bit_cast<u32>(value);
    const bool negative = (bits >> 31) != 0;
    const i32 exponent = static_cast<i32>((bits >> 23) & 0xFFu);
    const u32 fraction = bits & 0x7FFFFFu;
    if (exponent == 0xFF) {
        return false;  // infinity or NaN
    }
    if (exponent == 0) {
        *out = 0;  // zero or subnormal: far below one s16.16 step
        return true;
    }
    // value = mantissa * 2^(exponent - 150); in s16.16 that is
    // mantissa * 2^(exponent - 134).
    const u64 mantissa = static_cast<u64>(fraction | 0x800000u);
    const i32 shift = exponent - 134;
    u64 magnitude = 0;
    if (shift >= 0) {
        if (shift > 7) {
            return false;  // |value| >= 32768: outside s16.16
        }
        magnitude = mantissa << shift;
    } else {
        const i32 right = -shift;
        if (right > 40) {
            magnitude = 0;
        } else {
            // Round to nearest, halves away from zero -- the same rounding
            // the float quantizers this replaces used.
            magnitude = (mantissa + (static_cast<u64>(1) << (right - 1))) >> right;
        }
    }
    if (magnitude > 0x7FFFFFFFull) {
        return false;
    }
    *out = negative ? -static_cast<i32>(magnitude) : static_cast<i32>(magnitude);
    return true;
}

namespace detail {

i64 roundedDivideQ16(i64 numerator, i64 denominator) FL_NO_EXCEPT {
    return ::fl::roundedDivideQ16(numerator, denominator);
}

bool xyzColumnQ16(const i32 (&xy)[2], i32 luminance,
                  i64 (&column)[3]) FL_NO_EXCEPT {
    return emitterColumnQ16(xy, luminance, column);
}

i32 dotRowQ16(const i32 (&row)[3], const i32 (&v)[3]) FL_NO_EXCEPT {
    return dotSolveRowQ16(row, v);
}

}  // namespace detail

bool buildRgbSolveMatrixQ16(const colorimetric_response::EmitterProfile& profile,
                            EmitterSolveMatrixQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    // The profile stores float; its fields come across by their bits, and
    // the whole derivation runs in s16.16 (FastLED#4458). The Q16 build
    // refuses what the float one did: a non-positive or out-of-simplex
    // chromaticity, a non-positive luminance, a singular matrix -- and NaN,
    // infinity or an out-of-range value never gets past the conversion.
    EmitterChromaticitiesQ16 q16;
    if (!q16FromFloatBits(profile.xy_r[0], &q16.xy_r[0]) ||
        !q16FromFloatBits(profile.xy_r[1], &q16.xy_r[1]) ||
        !q16FromFloatBits(profile.xy_g[0], &q16.xy_g[0]) ||
        !q16FromFloatBits(profile.xy_g[1], &q16.xy_g[1]) ||
        !q16FromFloatBits(profile.xy_b[0], &q16.xy_b[0]) ||
        !q16FromFloatBits(profile.xy_b[1], &q16.xy_b[1]) ||
        !q16FromFloatBits(profile.lum_r, &q16.lum_r) ||
        !q16FromFloatBits(profile.lum_g, &q16.lum_g) ||
        !q16FromFloatBits(profile.lum_b, &q16.lum_b)) {
        return false;
    }
    return buildRgbSolveMatrixFromQ16(q16, out);
}

void solveRgbDrivesQ16(const EmitterSolveMatrixQ16& matrix,
                       const i32 (&xyz)[3], i32 (&drives)[3]) FL_NO_EXCEPT {
    const i32 in[3] = {xyz[0], xyz[1], xyz[2]};
    drives[0] = dotSolveRowQ16(matrix.m[0], in);
    drives[1] = dotSolveRowQ16(matrix.m[1], in);
    drives[2] = dotSolveRowQ16(matrix.m[2], in);
}

}  // namespace fl

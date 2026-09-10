// ok no header - implementation for fl/gfx/device_solve.h

#include "fl/gfx/device_solve.h"

namespace fl {

namespace {

// Helpers carry a `solve` qualifier: .cpp.hpp files share one translation
// unit under the unity build, so an anonymous namespace does not isolate them
// from same-named helpers in sibling files.

bool isUsableSolveChromaticity(const float (&xy)[2]) FL_NO_EXCEPT {
    // Self-comparison rejects NaN, which every relational guard downstream
    // lets through because comparisons against NaN are false.
    if (!(xy[0] == xy[0]) || !(xy[1] == xy[1])) {
        return false;
    }
    if (xy[0] <= 0.0f || xy[0] >= 1.0f || xy[1] <= 1e-6f || xy[1] >= 1.0f) {
        return false;
    }
    // Inside the CIE xy simplex. Checking the coordinates independently is
    // not enough: {0.8, 0.8} passes that and gives z = 1 - x - y = -0.6, so
    // xyY_to_XYZ yields a negative Z for a physical emitter. The boundary
    // itself is legal, so the test is on the sum exceeding 1.
    return xy[0] + xy[1] <= 1.0f;
}

bool isUsableLuminance(float lum) FL_NO_EXCEPT {
    return lum == lum && lum > 0.0f && lum < 1e6f;
}

i32 quantizeSolveQ16(float v) FL_NO_EXCEPT {
    const float scaled = v * 65536.0f;
    return static_cast<i32>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

/// Largest coefficient magnitude the build accepts, in whole units.
///
/// Chosen so the i64 accumulator cannot overflow for *any* i32 input:
/// 3 * (kMaxCoefficient << 16) * 2^31 stays under 2^63. Real emitter
/// inverses sit near 1, and the largest column entry in a plausible profile
/// is the blue emitter's Z at about 13, so this is enormously permissive
/// while still being a proof rather than an assumption.
constexpr i32 kMaxCoefficient = 21845;  // floor(2^32 / 3) >> 16

constexpr i64 kI64Max = 9223372036854775807LL;

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

bool buildRgbSolveMatrixQ16(const colorimetric_response::EmitterProfile& profile,
                            EmitterSolveMatrixQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    if (!isUsableSolveChromaticity(profile.xy_r) ||
        !isUsableSolveChromaticity(profile.xy_g) ||
        !isUsableSolveChromaticity(profile.xy_b) ||
        !isUsableLuminance(profile.lum_r) || !isUsableLuminance(profile.lum_g) ||
        !isUsableLuminance(profile.lum_b)) {
        return false;
    }

    float xyz_r[3];
    float xyz_g[3];
    float xyz_b[3];
    colorimetric_response::xyY_to_XYZ(profile.xy_r[0], profile.xy_r[1],
                                      profile.lum_r, xyz_r);
    colorimetric_response::xyY_to_XYZ(profile.xy_g[0], profile.xy_g[1],
                                      profile.lum_g, xyz_g);
    colorimetric_response::xyY_to_XYZ(profile.xy_b[0], profile.xy_b[1],
                                      profile.lum_b, xyz_b);

    // Columns are the emitters' XYZ contributions at full drive.
    const float emitter[3][3] = {
        {xyz_r[0], xyz_g[0], xyz_b[0]},
        {xyz_r[1], xyz_g[1], xyz_b[1]},
        {xyz_r[2], xyz_g[2], xyz_b[2]},
    };

    float inverse[3][3];
    if (!colorimetric_response::invert3x3(emitter, inverse)) {
        return false;
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            // A non-finite entry would reach the float-to-i32 cast, which is
            // undefined behaviour. invert3x3's determinant guard does not
            // catch NaN, so check the result rather than trusting it.
            const float value = inverse[row][col];
            if (!(value == value) ||
                value > static_cast<float>(kMaxCoefficient) ||
                value < -static_cast<float>(kMaxCoefficient)) {
                return false;
            }
            out->m[row][col] = quantizeSolveQ16(value);
        }
    }
    return true;
}

void solveRgbDrivesQ16(const EmitterSolveMatrixQ16& matrix,
                       const i32 (&xyz)[3], i32 (&drives)[3]) FL_NO_EXCEPT {
    const i32 in[3] = {xyz[0], xyz[1], xyz[2]};
    drives[0] = dotSolveRowQ16(matrix.m[0], in);
    drives[1] = dotSolveRowQ16(matrix.m[1], in);
    drives[2] = dotSolveRowQ16(matrix.m[2], in);
}

}  // namespace fl

// ok no header - implementation for fl/gfx/chromatic_adaptation.h

#include "fl/gfx/chromatic_adaptation.h"

#include "fl/gfx/colorimetric_response.h"
#include "fl/gfx/device_solve.h"

namespace fl {

namespace detail {

/// ICC linear Bradford cone-response matrix.
const float kBradford[3][3] = {
    {0.8951f, 0.2664f, -0.1614f},
    {-0.7502f, 1.7135f, 0.0367f},
    {0.0389f, -0.0685f, 1.0296f},
};

/// Its inverse, precomputed.
///
/// This used to be `invert3x3(kBradford, ...)` on every profile bind -- a
/// float determinant, a reciprocal, three column-scale divisions and nine
/// multiplies, run to recompute a matrix that cannot change. The operand is a
/// compile-time constant, so the answer is one too.
///
/// The values are what `invert3x3` produces from `kBradford` in float32, to
/// the last bit, so the adaptation matrix this feeds is unchanged.
/// `test_bradford_inverse_matches_the_general_solver` pins that against the
/// solver rather than against a transcription, which is what catches a typo
/// here -- a wrong digit in a plausible-looking matrix is exactly the kind of
/// constant nothing else would notice (FastLED #4043).
const float kBradfordInverse[3][3] = {
    {0.986992955f, -0.14705427f, 0.159962654f},
    {0.432305276f, 0.518360257f, 0.049291227f},
    {-0.0085286675f, 0.0400428213f, 0.968486726f},
};

/// The two matrices above in s16.16, rounded from the float32 values, so the
/// bind-time build needs no float (FastLED#4458).
const i32 kBradfordQ16[3][3] = {
    {58661, 17459, -10578},
    {-49165, 112296, 2405},
    {2549, -4489, 67476},
};
const i32 kBradfordInverseQ16[3][3] = {
    {64684, -9637, 10483},
    {28332, 33971, 3230},
    {-559, 2624, 63471},
};

} // namespace detail

namespace {

// Helper names carry an `adaptation` qualifier because .cpp.hpp files are
// concatenated into one translation unit by the unity build, so an anonymous
// namespace does not isolate them from a same-named helper in a sibling file
// -- source_xyz.cpp.hpp defines its own quantizer.

constexpr i32 kAdaptationQ16One = 65536;

/// A white point, converted from float by its bits, inside the open unit
/// square with a non-zero y -- the float check (NaN, 0 < x < 1,
/// 1e-6 < y < 1) with y's floor at one s16.16 step.
bool adaptationWhiteQ16(Chromaticity c, i32 (&out)[2]) FL_NO_EXCEPT {
    if (!q16FromFloatBits(c.x, &out[0]) || !q16FromFloatBits(c.y, &out[1])) {
        return false;
    }
    return out[0] > 0 && out[0] < kAdaptationQ16One && out[1] > 0 &&
           out[1] < kAdaptationQ16One;
}

i32 dotAdaptationRowQ16(const i32 (&row)[3], const i32 (&v)[3]) FL_NO_EXCEPT {
    const i64 acc = static_cast<i64>(row[0]) * static_cast<i64>(v[0])
                  + static_cast<i64>(row[1]) * static_cast<i64>(v[1])
                  + static_cast<i64>(row[2]) * static_cast<i64>(v[2]);
    return static_cast<i32>(acc >= 0 ? (acc + 32768) >> 16
                                     : -((-acc + 32768) >> 16));
}

}  // namespace

bool buildBradfordMatrixQ16(Chromaticity source_white,
                            Chromaticity destination_white,
                            AdaptationMatrixQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    // In s16.16 throughout (FastLED#4458). The same construction as the
    // float build it replaces: both whites to XYZ at Y = 1, into cone space,
    // the per-cone ratio as a diagonal, and B^-1 * diag * B collapsed once.
    i32 source_xy[2];
    i32 destination_xy[2];
    if (!adaptationWhiteQ16(source_white, source_xy) ||
        !adaptationWhiteQ16(destination_white, destination_xy)) {
        return false;
    }
    i64 source_xyz[3];
    i64 destination_xyz[3];
    if (!detail::xyzColumnQ16(source_xy, kAdaptationQ16One, source_xyz) ||
        !detail::xyzColumnQ16(destination_xy, kAdaptationQ16One, destination_xyz)) {
        return false;
    }
    const i32 source_v[3] = {static_cast<i32>(source_xyz[0]), static_cast<i32>(source_xyz[1]),
                             static_cast<i32>(source_xyz[2])};
    const i32 destination_v[3] = {static_cast<i32>(destination_xyz[0]),
                                  static_cast<i32>(destination_xyz[1]),
                                  static_cast<i32>(destination_xyz[2])};
    i64 scale[3];
    for (int i = 0; i < 3; ++i) {
        const i32 source_cone = detail::dotRowQ16(detail::kBradfordQ16[i], source_v);
        const i32 destination_cone =
            detail::dotRowQ16(detail::kBradfordQ16[i], destination_v);
        // A zero cone response would divide by zero. No physical white does
        // this; a corrupt profile can.
        if (source_cone == 0) {
            return false;
        }
        scale[i] = detail::roundedDivideQ16(
            static_cast<i64>(destination_cone) * kAdaptationQ16One, source_cone);
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            // sum_k Binv[row][k] * scale[k] * B[k][col]: each term is Q48,
            // brought back to Q16 once at the end rather than per product.
            i64 sum_q32 = 0;
            for (int k = 0; k < 3; ++k) {
                const i64 inv_scale_q16 = detail::roundedDivideQ16(
                    static_cast<i64>(detail::kBradfordInverseQ16[row][k]) * scale[k],
                    kAdaptationQ16One);
                sum_q32 += inv_scale_q16 * detail::kBradfordQ16[k][col];
            }
            const i64 value = detail::roundedDivideQ16(sum_q32, kAdaptationQ16One);
            if (value > 2147483647LL || value < -2147483648LL) {
                return false;
            }
            out->m[row][col] = static_cast<i32>(value);
        }
    }
    return true;
}

void adaptXyzQ16(const AdaptationMatrixQ16& matrix, const i32 (&xyz)[3],
                 i32 (&out_xyz)[3]) FL_NO_EXCEPT {
    const i32 in[3] = {xyz[0], xyz[1], xyz[2]};
    out_xyz[0] = dotAdaptationRowQ16(matrix.m[0], in);
    out_xyz[1] = dotAdaptationRowQ16(matrix.m[1], in);
    out_xyz[2] = dotAdaptationRowQ16(matrix.m[2], in);
}

void foldAdaptationIntoSourceMatrix(const AdaptationMatrixQ16& adaptation,
                                    SourceMatrixQ16* source) FL_NO_EXCEPT {
    if (source == nullptr) {
        return;
    }
    SourceMatrixQ16 result;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const i64 acc =
                static_cast<i64>(adaptation.m[row][0]) * source->m[0][col]
              + static_cast<i64>(adaptation.m[row][1]) * source->m[1][col]
              + static_cast<i64>(adaptation.m[row][2]) * source->m[2][col];
            result.m[row][col] = static_cast<i32>(
                acc >= 0 ? (acc + 32768) >> 16 : -((-acc + 32768) >> 16));
        }
    }
    *source = result;
}

}  // namespace fl

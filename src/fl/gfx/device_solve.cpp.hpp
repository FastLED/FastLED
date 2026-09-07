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

bool buildRgbSolveMatrixQ16(const EmitterProfile& profile,
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

// ok no header - implementation for fl/gfx/device_solve.h

#include "fl/gfx/device_solve.h"

namespace fl {

namespace {

// Helpers carry a `solve` qualifier: .cpp.hpp files share one translation
// unit under the unity build, so an anonymous namespace does not isolate them
// from same-named helpers in sibling files.

bool isUsableSolveChromaticity(const float xy[2]) FL_NO_EXCEPT {  // ok array parameter
    // Self-comparison rejects NaN, which every relational guard downstream
    // lets through because comparisons against NaN are false.
    return xy[0] == xy[0] && xy[1] == xy[1] && xy[0] > 0.0f && xy[0] < 1.0f &&
           xy[1] > 1e-6f && xy[1] < 1.0f;
}

bool isUsableLuminance(float lum) FL_NO_EXCEPT {
    return lum == lum && lum > 0.0f && lum < 1e6f;
}

i32 quantizeSolveQ16(float v) FL_NO_EXCEPT {
    const float scaled = v * 65536.0f;
    return static_cast<i32>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

i32 dotSolveRowQ16(const i32 (&row)[3], const i32 (&v)[3]) FL_NO_EXCEPT {
    const i64 acc = static_cast<i64>(row[0]) * static_cast<i64>(v[0])
                  + static_cast<i64>(row[1]) * static_cast<i64>(v[1])
                  + static_cast<i64>(row[2]) * static_cast<i64>(v[2]);
    return static_cast<i32>(acc >= 0 ? (acc + 32768) >> 16
                                     : -((-acc + 32768) >> 16));
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
            if (!(value == value) || value > 32767.0f || value < -32767.0f) {
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

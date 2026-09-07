// ok no header - implementation for fl/gfx/chromatic_adaptation.h

#include "fl/gfx/chromatic_adaptation.h"

#include "fl/gfx/colorimetric_response.h"

namespace fl {

namespace {

// Helper names carry an `adaptation` qualifier because .cpp.hpp files are
// concatenated into one translation unit by the unity build, so an anonymous
// namespace does not isolate them from a same-named helper in a sibling file
// -- source_xyz.cpp.hpp defines its own quantizer.

/// ICC linear Bradford cone-response matrix.
const float kBradford[3][3] = {
    {0.8951f, 0.2664f, -0.1614f},
    {-0.7502f, 1.7135f, 0.0367f},
    {0.0389f, -0.0685f, 1.0296f},
};

/// True only for a real, usable chromaticity.
///
/// `xyY_to_XYZ` guards `y < 1e-12f`, which NaN slips past because every
/// comparison against NaN is false. It then propagates through the cone
/// solve -- the zero-cone check below is comparison-based and lets it
/// through for the same reason -- and reaches the float-to-i32 cast, where
/// converting a NaN is undefined behaviour rather than a wrong number.
bool isUsableChromaticity(Chromaticity c) FL_NO_EXCEPT {
    // Self-comparison rejects NaN; the bounds reject infinities and values
    // outside the chromaticity diagram.
    return c.x == c.x && c.y == c.y && c.x > 0.0f && c.x < 1.0f &&
           c.y > 1e-6f && c.y < 1.0f;
}

i32 quantizeAdaptationQ16(float v) FL_NO_EXCEPT {
    const float scaled = v * 65536.0f;
    return static_cast<i32>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
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
    if (!isUsableChromaticity(source_white) ||
        !isUsableChromaticity(destination_white)) {
        return false;
    }
    float source_xyz[3];
    float destination_xyz[3];
    colorimetric_response::xyY_to_XYZ(source_white.x, source_white.y, 1.0f,
                                      source_xyz);
    colorimetric_response::xyY_to_XYZ(destination_white.x, destination_white.y,
                                      1.0f, destination_xyz);

    float source_cones[3];
    float destination_cones[3];
    colorimetric_response::matvec3(kBradford, source_xyz, source_cones);
    colorimetric_response::matvec3(kBradford, destination_xyz,
                                   destination_cones);

    float scale[3];
    for (int i = 0; i < 3; ++i) {
        // A zero cone response would divide by zero. No physical white does
        // this; a corrupt profile can.
        if (source_cones[i] > -1e-9f && source_cones[i] < 1e-9f) {
            return false;
        }
        scale[i] = destination_cones[i] / source_cones[i];
    }

    float inverse[3][3];
    if (!colorimetric_response::invert3x3(kBradford, inverse)) {
        return false;
    }

    // Collapse B_inv * diag(scale) * B into one matrix. Done once here so the
    // per-pixel path never sees the cone space at all.
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k) {
                sum += inverse[row][k] * scale[k] * kBradford[k][col];
            }
            out->m[row][col] = quantizeAdaptationQ16(sum);
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

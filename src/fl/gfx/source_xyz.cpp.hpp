// ok no header - implementation for fl/gfx/source_xyz.h

#include "fl/gfx/source_xyz.h"

#include "fl/gfx/colorimetric_response.h"

namespace fl {

namespace {

/// True only for a real, usable chromaticity.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it from the
/// equivalent helper in chromatic_adaptation.cpp.hpp.
///
/// Neither `xyY_to_XYZ`'s `y < 1e-12f` guard nor `invert3x3`'s
/// `fabs(det) < 1e-20f` guard stops a NaN, because comparisons against NaN
/// are all false. It would reach the float-to-i32 cast below, where
/// converting a NaN is undefined behaviour.
bool isUsableSourceChromaticity(Chromaticity c) FL_NO_EXCEPT {
    return c.x == c.x && c.y == c.y && c.x > 0.0f && c.x < 1.0f &&
           c.y > 1e-6f && c.y < 1.0f;
}

/// Round-to-nearest quantization of a float into s16.16.
i32 quantizeQ16(float v) FL_NO_EXCEPT {
    const float scaled = v * 65536.0f;
    return static_cast<i32>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

/// One matrix row against the pixel. The accumulator is i64 because the
/// three Q16xQ16 products are Q32: at full scale each term is ~4.3e9, which
/// overflows i32 on its own.
i32 dotRowQ16(const i32 (&row)[3], u16 r, u16 g, u16 b) FL_NO_EXCEPT {
    const i64 acc = static_cast<i64>(row[0]) * static_cast<i64>(r)
                  + static_cast<i64>(row[1]) * static_cast<i64>(g)
                  + static_cast<i64>(row[2]) * static_cast<i64>(b);
    // Round to nearest rather than truncating: truncation biases every
    // pixel downward, and the bias accumulates across the later stages.
    return static_cast<i32>((acc + 32768) >> 16);
}

}  // namespace

bool buildSourceMatrixQ16(const RgbPrimaries& primaries,
                          SourceMatrixQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    if (!isUsableSourceChromaticity(primaries.red) ||
        !isUsableSourceChromaticity(primaries.green) ||
        !isUsableSourceChromaticity(primaries.blue) ||
        !isUsableSourceChromaticity(primaries.white)) {
        return false;
    }
    const float xy_r[2] = {primaries.red.x, primaries.red.y};
    const float xy_g[2] = {primaries.green.x, primaries.green.y};
    const float xy_b[2] = {primaries.blue.x, primaries.blue.y};
    const float xy_w[2] = {primaries.white.x, primaries.white.y};

    float matrix[3][3];
    if (!colorimetric_response::build_source_matrix(xy_r, xy_g, xy_b, xy_w, matrix)) {
        return false;
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out->m[row][col] = quantizeQ16(matrix[row][col]);
        }
    }
    return true;
}

void linearRgbToXyzQ16(const SourceMatrixQ16& matrix, u16 r, u16 g, u16 b,
                       i32 (&out_xyz)[3]) FL_NO_EXCEPT {
    out_xyz[0] = dotRowQ16(matrix.m[0], r, g, b);
    out_xyz[1] = dotRowQ16(matrix.m[1], r, g, b);
    out_xyz[2] = dotRowQ16(matrix.m[2], r, g, b);
}

}  // namespace fl

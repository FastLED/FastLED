// ok no header - implementation for fl/gfx/source_xyz.h

#include "fl/gfx/source_xyz.h"

#include "fl/gfx/device_solve.h"

namespace fl {

namespace {

constexpr i32 kSourceQ16One = 65536;

/// A chromaticity, converted from float by its bits, that is real and usable:
/// inside the open unit square with a non-zero y. The float check it replaces
/// (NaN, 0 < x < 1, 1e-6 < y < 1) is kept, with y's floor at one s16.16
/// step -- a smaller y would quantise to zero and be divided by.
bool sourceChromaticityQ16(Chromaticity c, i32 (&out)[2]) FL_NO_EXCEPT {
    if (!q16FromFloatBits(c.x, &out[0]) || !q16FromFloatBits(c.y, &out[1])) {
        return false;
    }
    return out[0] > 0 && out[0] < kSourceQ16One && out[1] > 0 &&
           out[1] < kSourceQ16One;
}

/// True when the three primaries enclose an actual area of chromaticity.
///
/// A near-singular primary set passes any determinant guard as rounding
/// noise, so the degeneracy is caught geometrically: twice the triangle's
/// area, in Q32, against 1e-4 (sRGB's is 0.224). Measured on the float path,
/// red and green identical gave a determinant of -1.09e-7 and an inverse of
/// pure noise.
bool sourcePrimariesEncloseAreaQ16(const i32 (&r)[2], const i32 (&g)[2],
                                   const i32 (&b)[2]) FL_NO_EXCEPT {
    const i64 ux = static_cast<i64>(g[0]) - r[0];
    const i64 uy = static_cast<i64>(g[1]) - r[1];
    const i64 vx = static_cast<i64>(b[0]) - r[0];
    const i64 vy = static_cast<i64>(b[1]) - r[1];
    const i64 twice_area_q32 = ux * vy - uy * vx;
    constexpr i64 kMinTwiceAreaQ32 = 429497;  // 1e-4 * 2^32
    return twice_area_q32 > kMinTwiceAreaQ32 || twice_area_q32 < -kMinTwiceAreaQ32;
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
    // In s16.16 throughout (FastLED#4458): the same construction as
    // `colorimetric_response::build_source_matrix` -- primaries' XYZ at unit
    // luminance as columns P, scaled per column by k = P^-1 W so that full
    // RGB lands on the white -- with no float operation on the way.
    i32 xy_r[2];
    i32 xy_g[2];
    i32 xy_b[2];
    i32 xy_w[2];
    if (!sourceChromaticityQ16(primaries.red, xy_r) ||
        !sourceChromaticityQ16(primaries.green, xy_g) ||
        !sourceChromaticityQ16(primaries.blue, xy_b) ||
        !sourceChromaticityQ16(primaries.white, xy_w)) {
        return false;
    }
    if (!sourcePrimariesEncloseAreaQ16(xy_r, xy_g, xy_b)) {
        return false;
    }
    i64 red[3];
    i64 green[3];
    i64 blue[3];
    i64 white[3];
    if (!detail::xyzColumnQ16(xy_r, kSourceQ16One, red) ||
        !detail::xyzColumnQ16(xy_g, kSourceQ16One, green) ||
        !detail::xyzColumnQ16(xy_b, kSourceQ16One, blue) ||
        !detail::xyzColumnQ16(xy_w, kSourceQ16One, white)) {
        return false;
    }
    const i32 primaries_xyz[3][3] = {
        {static_cast<i32>(red[0]), static_cast<i32>(green[0]), static_cast<i32>(blue[0])},
        {static_cast<i32>(red[1]), static_cast<i32>(green[1]), static_cast<i32>(blue[1])},
        {static_cast<i32>(red[2]), static_cast<i32>(green[2]), static_cast<i32>(blue[2])},
    };
    i32 inverse[3][3];
    if (!invert3x3Q16(primaries_xyz, inverse)) {
        return false;
    }
    const i32 white_xyz[3] = {static_cast<i32>(white[0]), static_cast<i32>(white[1]),
                              static_cast<i32>(white[2])};
    i32 k[3];
    for (int i = 0; i < 3; ++i) {
        k[i] = detail::dotRowQ16(inverse[i], white_xyz);
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const i64 product = static_cast<i64>(primaries_xyz[row][col]) * k[col];
            out->m[row][col] = static_cast<i32>(
                detail::roundedDivideQ16(product, kSourceQ16One));
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

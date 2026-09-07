// Device-solve coverage for color pipeline P7 (#4041).

#include "fl/gfx/device_solve.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/stl/int.h"
#include "fl/stl/limits.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// The corpus's `rgb` device: sRGB primaries, unit luminance each.
EmitterProfile rgbDevice() {
    EmitterProfile p = {};
    p.xy_r[0] = 0.6400f; p.xy_r[1] = 0.3300f;
    p.xy_g[0] = 0.3000f; p.xy_g[1] = 0.6000f;
    p.xy_b[0] = 0.1500f; p.xy_b[1] = 0.0600f;
    p.lum_r = 1.0f; p.lum_g = 1.0f; p.lum_b = 1.0f;
    p.native_code_depth = 8;
    return p;
}

i32 q16(float v) { return static_cast<i32>(v * 65536.0f + (v >= 0 ? 0.5f : -0.5f)); }
float toFloat(i32 v) { return static_cast<float>(v) / 65536.0f; }

}  // namespace

FL_TEST_CASE("Solving an emitter's own light drives only that emitter") {
    // Feeding back exactly one emitter's XYZ must recover a unit drive on
    // that emitter and nothing on the others. This is the inverse's defining
    // property and catches a transposed or mis-scaled matrix immediately.
    EmitterSolveMatrixQ16 matrix;
    FL_REQUIRE(buildRgbSolveMatrixQ16(rgbDevice(), &matrix));

    const float chroma[3][2] = {{0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
    for (int e = 0; e < 3; ++e) {
        float xyz_f[3];
        colorimetric_response::xyY_to_XYZ(chroma[e][0], chroma[e][1], 1.0f, xyz_f);
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};
        i32 drives[3];
        solveRgbDrivesQ16(matrix, xyz, drives);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_CLOSE(toFloat(drives[i]), i == e ? 1.0f : 0.0f, 0.002f);
        }
    }
}

FL_TEST_CASE("Black solves to no drive at all") {
    EmitterSolveMatrixQ16 matrix;
    FL_REQUIRE(buildRgbSolveMatrixQ16(rgbDevice(), &matrix));
    const i32 xyz[3] = {0, 0, 0};
    i32 drives[3];
    solveRgbDrivesQ16(matrix, xyz, drives);
    FL_CHECK_EQ(drives[0], 0);
    FL_CHECK_EQ(drives[1], 0);
    FL_CHECK_EQ(drives[2], 0);
}

FL_TEST_CASE("The solve is linear, so scaling the target scales the drives") {
    // Linearity is what lets the brightness stage sit after the solve and
    // still preserve chromaticity.
    EmitterSolveMatrixQ16 matrix;
    FL_REQUIRE(buildRgbSolveMatrixQ16(rgbDevice(), &matrix));

    const i32 xyz[3] = {q16(0.4f), q16(0.5f), q16(0.6f)};
    const i32 half[3] = {q16(0.2f), q16(0.25f), q16(0.3f)};
    i32 full_drives[3];
    i32 half_drives[3];
    solveRgbDrivesQ16(matrix, xyz, full_drives);
    solveRgbDrivesQ16(matrix, half, half_drives);
    for (int i = 0; i < 3; ++i) {
        FL_CHECK(half_drives[i] >= full_drives[i] / 2 - 2 &&
                 half_drives[i] <= full_drives[i] / 2 + 2);
    }
}

FL_TEST_CASE("Out-of-gamut targets yield negative drives rather than silent clipping") {
    // Clamping belongs to the gamut mapper. If the solve clipped here the
    // mapper could never see that the target was outside the hull.
    EmitterSolveMatrixQ16 matrix;
    FL_REQUIRE(buildRgbSolveMatrixQ16(rgbDevice(), &matrix));
    // A highly saturated target beyond the sRGB hull.
    const i32 xyz[3] = {q16(0.05f), q16(0.9f), q16(0.05f)};
    i32 drives[3];
    solveRgbDrivesQ16(matrix, xyz, drives);
    bool any_negative = false;
    for (int i = 0; i < 3; ++i) {
        if (drives[i] < 0) any_negative = true;
    }
    FL_CHECK(any_negative);
}

FL_TEST_CASE("Fixed-point solve tracks the P5 float64 reference") {
    // mapped_xyz -> emitter_light for in-gamut vectors of the corpus's `rgb`
    // device, taken from ci/golden/color-reference-v1.json.
    EmitterSolveMatrixQ16 matrix;
    FL_REQUIRE(buildRgbSolveMatrixQ16(rgbDevice(), &matrix));

    struct Case { float xyz[3]; float drives[3]; };
    const Case cases[] = {
        {{0.000828284f, 0.000871460f, 0.000949070f}, {0.000185306f, 0.000623241f, 0.000062913f}},
        {{0.002791661f, 0.003234690f, 0.007494703f}, {0.000071828f, 0.002627398f, 0.000535464f}},
        {{0.248526648f, 0.261481507f, 0.284768462f}, {0.055601168f, 0.187003384f, 0.018876955f}},
        {{0.013252545f, 0.013943355f, 0.015185119f}, {0.002964901f, 0.009971851f, 0.001006603f}},
        {{0.841088340f, 0.884931448f, 0.963741453f}, {0.188170943f, 0.632875255f, 0.063885250f}},
        {{0.001099238f, 0.001171371f, 0.002589695f}, {0.000100047f, 0.000886551f, 0.000184773f}},
    };
    for (const Case& c : cases) {
        const i32 xyz[3] = {q16(c.xyz[0]), q16(c.xyz[1]), q16(c.xyz[2])};
        i32 drives[3];
        solveRgbDrivesQ16(matrix, xyz, drives);
        for (int i = 0; i < 3; ++i) {
            // Two Q16 steps: the measured worst case over all 28 in-gamut
            // rgb vectors is 1.3 steps.
            FL_CHECK_CLOSE(toFloat(drives[i]), c.drives[i], 3.1e-5f);
        }
    }
}

FL_TEST_CASE("Degenerate and non-finite emitter profiles are rejected") {
    EmitterSolveMatrixQ16 matrix;
    FL_CHECK_FALSE(buildRgbSolveMatrixQ16(rgbDevice(), nullptr));

    EmitterProfile collinear = rgbDevice();
    collinear.xy_g[0] = 0.6400f; collinear.xy_g[1] = 0.3300f;
    collinear.xy_b[0] = 0.6400f; collinear.xy_b[1] = 0.3300f;
    FL_CHECK_FALSE(buildRgbSolveMatrixQ16(collinear, &matrix));

    EmitterProfile nan_chroma = rgbDevice();
    nan_chroma.xy_r[0] = numeric_limits<float>::quiet_NaN();
    FL_CHECK_FALSE(buildRgbSolveMatrixQ16(nan_chroma, &matrix));

    EmitterProfile zero_lum = rgbDevice();
    zero_lum.lum_g = 0.0f;
    FL_CHECK_FALSE(buildRgbSolveMatrixQ16(zero_lum, &matrix));

    FL_CHECK(buildRgbSolveMatrixQ16(rgbDevice(), &matrix));
}

}  // FL_TEST_FILE

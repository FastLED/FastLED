// Bradford chromatic-adaptation coverage for color pipeline P6 (#4040).
//
// The P5 golden corpus cannot exercise this stage: all three of its source
// profiles use a D65 white, so source_xyz equals d65_xyz identically for all
// 192 vectors. Expectations here are computed from the same authority the
// corpus is generated from, ci/color_reference.py::bradford_adaptation.

#include "fl/gfx/chromatic_adaptation.h"
#include "fl/gfx/color_profile.h"
#include "fl/gfx/source_xyz.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

constexpr Chromaticity kD50(0.34567f, 0.35850f);
constexpr Chromaticity kD65(0.3127f, 0.3290f);

i32 q16(float v) { return static_cast<i32>(v * 65536.0f + (v >= 0 ? 0.5f : -0.5f)); }
float toFloat(i32 v) { return static_cast<float>(v) / 65536.0f; }

}  // namespace

FL_TEST_CASE("Adapting a white to itself is the identity") {
    AdaptationMatrixQ16 matrix;
    FL_REQUIRE(buildBradfordMatrixQ16(kD65, kD65, &matrix));

    const i32 in[3] = {q16(0.3f), q16(0.6f), q16(0.9f)};
    i32 out[3];
    adaptXyzQ16(matrix, in, out);
    for (int i = 0; i < 3; ++i) {
        // Within one Q16 step; the collapsed matrix is B_inv * I * B.
        FL_CHECK(out[i] >= in[i] - 1 && out[i] <= in[i] + 1);
    }
}

FL_TEST_CASE("Adapting the source white lands on the destination white") {
    // The defining property of a chromatic adaptation transform.
    AdaptationMatrixQ16 matrix;
    FL_REQUIRE(buildBradfordMatrixQ16(kD50, kD65, &matrix));

    // D50 white at Y=1, and D65 white at Y=1.
    const i32 in[3] = {q16(0.96420f), q16(1.0f), q16(0.82491f)};
    i32 out[3];
    adaptXyzQ16(matrix, in, out);

    FL_CHECK_CLOSE(toFloat(out[0]), 0.95043f, 0.001f);
    FL_CHECK_CLOSE(toFloat(out[1]), 1.00000f, 0.001f);
    FL_CHECK_CLOSE(toFloat(out[2]), 1.08867f, 0.001f);
}

FL_TEST_CASE("Fixed-point Bradford tracks the float64 reference") {
    AdaptationMatrixQ16 matrix;
    FL_REQUIRE(buildBradfordMatrixQ16(kD50, kD65, &matrix));

    struct Case { float in[3]; float out[3]; };
    const Case cases[] = {
        {{1.0f, 1.0f, 1.0f},       {0.995705078f, 1.002662625f, 1.322038361f}},
        {{0.5f, 0.25f, 0.125f},    {0.479908114f, 0.240959235f, 0.167308221f}},
    };
    for (const Case& c : cases) {
        const i32 in[3] = {q16(c.in[0]), q16(c.in[1]), q16(c.in[2])};
        i32 out[3];
        adaptXyzQ16(matrix, in, out);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_CLOSE(toFloat(out[i]), c.out[i], 0.0002f);
        }
    }
}

FL_TEST_CASE("Black is unchanged by adaptation") {
    AdaptationMatrixQ16 matrix;
    FL_REQUIRE(buildBradfordMatrixQ16(kD50, kD65, &matrix));
    const i32 in[3] = {0, 0, 0};
    i32 out[3];
    adaptXyzQ16(matrix, in, out);
    FL_CHECK_EQ(out[0], 0);
    FL_CHECK_EQ(out[1], 0);
    FL_CHECK_EQ(out[2], 0);
}

FL_TEST_CASE("Folding into the source matrix equals applying both in sequence") {
    // This is what makes adaptation free per pixel: one matrix instead of two.
    SourceMatrixQ16 sequential;
    FL_REQUIRE(buildSourceMatrixQ16(SourceProfile::bt2020().primaries, &sequential));
    SourceMatrixQ16 folded = sequential;

    AdaptationMatrixQ16 adaptation;
    FL_REQUIRE(buildBradfordMatrixQ16(kD50, kD65, &adaptation));
    foldAdaptationIntoSourceMatrix(adaptation, &folded);

    const u16 rgb[3] = {50000, 30000, 12000};
    i32 staged[3];
    linearRgbToXyzQ16(sequential, rgb[0], rgb[1], rgb[2], staged);
    i32 adapted[3];
    adaptXyzQ16(adaptation, staged, adapted);

    i32 once[3];
    linearRgbToXyzQ16(folded, rgb[0], rgb[1], rgb[2], once);

    for (int i = 0; i < 3; ++i) {
        // Folding rounds once instead of twice, so it can differ by a step
        // or two -- it should never diverge further than that.
        FL_CHECK(once[i] >= adapted[i] - 4 && once[i] <= adapted[i] + 4);
    }
}

FL_TEST_CASE("Non-finite chromaticities are rejected, not converted") {
    // xyY_to_XYZ guards `y < 1e-12f`, which NaN slips past because every
    // comparison against NaN is false. Without an explicit check it reaches
    // the float-to-i32 cast, and converting a NaN there is undefined
    // behaviour rather than merely a wrong colour.
    const float nan_value = 0.0f / (sizeof(int) > 100 ? 1.0f : 0.0f);
    AdaptationMatrixQ16 matrix;
    FL_CHECK_FALSE(buildBradfordMatrixQ16(Chromaticity(nan_value, 0.3290f), kD65, &matrix));
    FL_CHECK_FALSE(buildBradfordMatrixQ16(kD50, Chromaticity(0.3127f, nan_value), &matrix));
    // y == 0 would divide by zero inside xyY_to_XYZ's guarded path.
    FL_CHECK_FALSE(buildBradfordMatrixQ16(Chromaticity(0.3f, 0.0f), kD65, &matrix));
    // Out of the chromaticity diagram entirely.
    FL_CHECK_FALSE(buildBradfordMatrixQ16(Chromaticity(1.5f, 0.3f), kD65, &matrix));
    // The valid pair still builds.
    FL_CHECK(buildBradfordMatrixQ16(kD50, kD65, &matrix));
}

FL_TEST_CASE("A null out-pointer is rejected rather than dereferenced") {
    FL_CHECK_FALSE(buildBradfordMatrixQ16(kD50, kD65, nullptr));
    AdaptationMatrixQ16 matrix;
    FL_REQUIRE(buildBradfordMatrixQ16(kD50, kD65, &matrix));
    foldAdaptationIntoSourceMatrix(matrix, nullptr);  // must not crash
}

}  // FL_TEST_FILE

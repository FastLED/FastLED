// Source RGB -> XYZ working-domain coverage for color pipeline P6 (#4040).

#include "fl/gfx/source_xyz.h"
#include "fl/gfx/color_profile.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

enum class SourceKind { SrgbBt709, DisplayP3, Bt2020 };

RgbPrimaries primariesFor(SourceKind kind) {
    // Taken from the public factories rather than hand-written, so the test
    // cannot drift from the primaries the library actually ships.
    switch (kind) {
    case SourceKind::DisplayP3: return SourceProfile::displayP3().primaries;
    case SourceKind::Bt2020:    return SourceProfile::bt2020().primaries;
    case SourceKind::SrgbBt709: break;
    }
    return SourceProfile::srgbBt709().primaries;
}

float toFloat(i32 q16) { return static_cast<float>(q16) / 65536.0f; }

}  // namespace

FL_TEST_CASE("Source matrix maps full-scale white onto the profile white point") {
    // M * (1,1,1) must land on the source white at Y=1. This is the property
    // the k-scaling in the derivation exists to guarantee, and it is what
    // makes a neutral input stay neutral.
    for (SourceKind kind : {SourceKind::SrgbBt709, SourceKind::DisplayP3,
                            SourceKind::Bt2020}) {
        SourceMatrixQ16 matrix;
        FL_REQUIRE(buildSourceMatrixQ16(primariesFor(kind), &matrix));

        i32 xyz[3];
        linearRgbToXyzQ16(matrix, 65535, 65535, 65535, xyz);
        // Y = 1 within the 65535/65536 full-scale bias plus rounding.
        FL_CHECK_CLOSE(toFloat(xyz[1]), 1.0f, 0.0005f);

        const RgbPrimaries p = primariesFor(kind);
        const float sum = static_cast<float>(xyz[0] + xyz[1] + xyz[2]);
        FL_CHECK_CLOSE(static_cast<float>(xyz[0]) / sum, p.white.x, 0.0005f);
        FL_CHECK_CLOSE(static_cast<float>(xyz[1]) / sum, p.white.y, 0.0005f);
    }
}

FL_TEST_CASE("Black maps to exactly zero XYZ") {
    SourceMatrixQ16 matrix;
    FL_REQUIRE(buildSourceMatrixQ16(SourceProfile::srgbBt709().primaries, &matrix));
    i32 xyz[3];
    linearRgbToXyzQ16(matrix, 0, 0, 0, xyz);
    FL_CHECK_EQ(xyz[0], 0);
    FL_CHECK_EQ(xyz[1], 0);
    FL_CHECK_EQ(xyz[2], 0);
}

FL_TEST_CASE("Collinear primaries are rejected rather than producing garbage") {
    SourceMatrixQ16 matrix;
    const RgbPrimaries degenerate(Chromaticity(0.3f, 0.3f), Chromaticity(0.3f, 0.3f),
                                  Chromaticity(0.3f, 0.3f), Chromaticity(0.3127f, 0.3290f));
    FL_CHECK_FALSE(buildSourceMatrixQ16(degenerate, &matrix));
    FL_CHECK_FALSE(buildSourceMatrixQ16(SourceProfile::srgbBt709().primaries, nullptr));
}

FL_TEST_CASE("Fixed-point XYZ tracks the P5 float64 reference") {
    // Inputs are the golden corpus's linear_rgb quantized to u16; expectations
    // are its source_xyz. Tolerance is absolute because these span four orders
    // of magnitude and the s16.16 step is 1.5e-5 regardless of magnitude.
    struct Case { SourceKind kind; u16 rgb[3]; float xyz[3]; };
    const Case cases[] = {
    {SourceKind::Bt2020, {57, 57, 57}, {0.000828284f, 0.000871460f, 0.000949070f}},
    {SourceKind::Bt2020, {57, 0, 0}, {0.000555083f, 0.000228933f, 0.000000000f}},
    {SourceKind::Bt2020, {0, 57, 0}, {0.000126028f, 0.000590848f, 0.000024464f}},
    {SourceKind::Bt2020, {0, 0, 57}, {0.000147173f, 0.000051679f, 0.000924606f}},
    {SourceKind::Bt2020, {114, 228, 457}, {0.002791661f, 0.003234690f, 0.007494703f}},
    {SourceKind::Bt2020, {457, 1921, 10183}, {0.034920790f, 0.030922128f, 0.165677298f}},
    {SourceKind::Bt2020, {17136, 17136, 17136}, {0.248526648f, 0.261481507f, 0.284768462f}},
    {SourceKind::Bt2020, {914, 914, 914}, {0.013252545f, 0.013943355f, 0.015185119f}},
    {SourceKind::Bt2020, {57994, 57994, 57994}, {0.841088340f, 0.884931448f, 0.963741453f}},
    {SourceKind::Bt2020, {65535, 0, 0}, {0.636958048f, 0.262700212f, 0.000000000f}},
    {SourceKind::Bt2020, {0, 65535, 0}, {0.144616904f, 0.677998072f, 0.028072693f}},
    {SourceKind::Bt2020, {0, 0, 65535}, {0.168880975f, 0.059301716f, 1.060985058f}},
    };
    for (const Case& c : cases) {
        SourceMatrixQ16 matrix;
        FL_REQUIRE(buildSourceMatrixQ16(primariesFor(c.kind), &matrix));
        i32 xyz[3];
        linearRgbToXyzQ16(matrix, c.rgb[0], c.rgb[1], c.rgb[2], xyz);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_CLOSE(toFloat(xyz[i]), c.xyz[i], 0.0001f);
        }
    }
}

}  // FL_TEST_FILE

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

FL_TEST_CASE("Chromaticities outside the CIE simplex are rejected") {
    // Checking x and y independently is not enough: {0.8, 0.8} passes that
    // and gives z = 1 - x - y = -0.6, so xyY_to_XYZ produces a negative Z
    // for something claiming to be a physical emitter, and invert3x3 will
    // happily invert the resulting nonsingular matrix.
    EmitterSolveMatrixQ16 matrix;
    EmitterProfile outside = rgbDevice();
    outside.xy_r[0] = 0.8f;
    outside.xy_r[1] = 0.8f;
    FL_CHECK_FALSE(buildRgbSolveMatrixQ16(outside, &matrix));

    // The simplex boundary itself is legal: x + y == 1 means z == 0, which
    // is a real monochromatic-locus emitter.
    EmitterProfile boundary = rgbDevice();
    boundary.xy_r[0] = 0.7f;
    boundary.xy_r[1] = 0.3f;
    FL_CHECK(buildRgbSolveMatrixQ16(boundary, &matrix));
}

FL_TEST_CASE("An extreme target saturates instead of wrapping") {
    // Wrapping would turn an out-of-gamut overshoot into a wildly wrong
    // colour of the opposite sign, which the gamut mapper downstream would
    // then treat as a legitimate drive.
    EmitterSolveMatrixQ16 matrix;
    FL_REQUIRE(buildRgbSolveMatrixQ16(rgbDevice(), &matrix));

    // Aligned with the green row, whose Y coefficient is about 1.34: the
    // product exceeds s16.16 where an all-equal input would not, because
    // this inverse's row sums are all below 1.
    const i32 xyz[3] = {0, 2147483647, 0};
    i32 drives[3];
    solveRgbDrivesQ16(matrix, xyz, drives);
    FL_CHECK_EQ(drives[1], 2147483647);
    // The other two stay in range and keep their signs, so saturation is
    // per-component rather than poisoning the whole solve.
    FL_CHECK(drives[0] < 0);
    FL_CHECK(drives[2] < 0);
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

namespace {

/// The four profiles `ci/color_fixed_inverse_study.py` prices P9 item 2
/// against, quantised to s16.16 exactly as its `quantize_rows` does, with the
/// inverse its `invert3x3_q16` produces using Python's unbounded integers.
///
/// These are a reference, not a recording of this implementation's output.
/// The study computes `(cofactor << 32) / determinant` exactly; that is the
/// thing `invert3x3Q16` claims to reproduce without a 128-bit intermediate,
/// and a table written from C++ output could not tell the two apart.
struct StudyInverse {
    i32 forward[3][3];
    i32 inverse[3][3];
};

const StudyInverse kStudyInverses[] = {
    // srgb
    {{{127100, 32768, 163840}, {65536, 65536, 65536}, {5958, 10923, 862891}},
     {{45165, -21424, -6948}, {-45428, 87925, 1948}, {263, -965, 5001}}},
    // bt2020
    {{{158902, 13979, 186635}, {65536, 65536, 65536}, {0, 2714, 1172525}},
     {{29555, -6123, -4362}, {-29623, 71826, 701}, {69, -166, 3661}}},
    // display_p3
    {{{139264, 25170, 163840}, {65536, 65536, 65536}, {0, 4274, 862891}},
     {{37418, -13977, -6043}, {-37604, 79908, 1071}, {186, -396, 4972}}},
    // narrow -- the near-degenerate set, where the determinant is smallest
    {{{127100, 78019, 72090}, {65536, 65536, 65536}, {5958, 12483, 26214}},
     {{92837, -118156, 40087}, {-136953, 299419, -371929}, {44116, -115727, 331843}}},
};

/// The (0,0) cofactor, which is what `cofactor << 32` would have to hold.
i64 leadCofactor(const i32 (&m)[3][3]) {
    return static_cast<i64>(m[1][1]) * static_cast<i64>(m[2][2]) -
           static_cast<i64>(m[1][2]) * static_cast<i64>(m[2][1]);
}

}  // namespace

FL_TEST_CASE("Q16 inverse reproduces the host study exactly") {
    for (const auto& item : kStudyInverses) {
        i32 out[3][3];
        FL_REQUIRE(invert3x3Q16(item.forward, out));
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                FL_CHECK_EQ(out[row][col], item.inverse[row][col]);
            }
        }
    }
}

FL_TEST_CASE("the wide numerator the staged divide avoids really is needed") {
    // Vacuity guard for the case above. If every cofactor fitted in the 31
    // bits that `cofactor << 32` leaves, the staged divide would be an
    // elaborate way to write one division and the agreement above would say
    // nothing about it.
    //
    // Lead cofactors, in bits: srgb 2^35.7, bt2020 2^36.2, display_p3 2^35.7,
    // narrow 2^29.7. Three of the four need more than the 31 bits available,
    // and narrow -- the near-degenerate set -- is the one that does not.
    int needing_wide = 0;
    for (const auto& item : kStudyInverses) {
        const i64 cofactor = leadCofactor(item.forward);
        const i64 magnitude = cofactor < 0 ? -cofactor : cofactor;
        if (magnitude >= (static_cast<i64>(1) << 31)) {
            ++needing_wide;
        }
    }
    FL_CHECK_EQ(needing_wide, 3);
}

FL_TEST_CASE("Q16 inverse round-trips to the identity") {
    // Independent of the study: M . M^-1 must be I, which no transcription
    // error in the table above could satisfy by accident.
    for (const auto& item : kStudyInverses) {
        i32 inverse[3][3];
        FL_REQUIRE(invert3x3Q16(item.forward, inverse));
        i32 worst_diagonal = 0;
        i32 worst_off_diagonal = 0;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                i64 acc = 0;
                for (int k = 0; k < 3; ++k) {
                    acc += static_cast<i64>(item.forward[row][k]) *
                           static_cast<i64>(inverse[k][col]);
                }
                const i32 value = static_cast<i32>((acc + 32768) >> 16);
                const i32 expected = row == col ? 65536 : 0;
                i32 error = value - expected;
                if (error < 0) {
                    error = -error;
                }
                if (row == col) {
                    if (error > worst_diagonal) {
                        worst_diagonal = error;
                    }
                } else if (error > worst_off_diagonal) {
                    worst_off_diagonal = error;
                }
            }
        }
        // Measured worst across the four: 7 raw units on the diagonal and 8
        // off it, both on bt2020 -- about 1e-4 of a unit, which is the
        // inputs' own quantisation rather than anything the inversion adds.
        // display_p3 and narrow are inside 3.
        FL_CHECK_LT(worst_diagonal, 16);
        FL_CHECK_LT(worst_off_diagonal, 16);
    }
}

FL_TEST_CASE("Q16 inverse refuses a singular matrix") {
    i32 out[3][3];
    // Two identical columns: determinant exactly zero.
    const i32 repeated_column[3][3] = {
        {65536, 65536, 32768}, {65536, 65536, 16384}, {13107, 13107, 65536}};
    FL_CHECK(!invert3x3Q16(repeated_column, out));

    // Singular by a different route, and the one a zero-initialised profile
    // would produce.
    const i32 zero[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    FL_CHECK(!invert3x3Q16(zero, out));
}

FL_TEST_CASE("Q16 inverse refuses input whose determinant leaves i64") {
    // Why the guard is a check and not a bound on the input: this is refused
    // for arithmetic reasons, not because the entries are implausible. A
    // fixed input limit safe enough to make the determinant fit would have to
    // sit near 16.0, and a deep-blue emitter at xy = (0.14, 0.03) already has
    // a Z column near 28.
    i32 out[3][3];
    const i32 huge[3][3] = {
        {2000000000, 3, 5}, {7, 2000000000, 11}, {13, 17, 2000000000}};
    FL_CHECK(!invert3x3Q16(huge, out));

    // And it refuses rather than wrapping. A wrapped determinant would hand
    // back a plausible-looking inverse for a matrix that has none in s16.16,
    // so the identity is checked alongside to show refusal is not the only
    // thing this function does.
    const i32 identity[3][3] = {{65536, 0, 0}, {0, 65536, 0}, {0, 0, 65536}};
    FL_REQUIRE(invert3x3Q16(identity, out));
    FL_CHECK_EQ(out[0][0], 65536);
    FL_CHECK_EQ(out[1][1], 65536);
    FL_CHECK_EQ(out[2][2], 65536);
    FL_CHECK_EQ(out[0][1], 0);
}

FL_TEST_CASE("Q16 inverse refuses a determinant of exactly i64's minimum") {
    // Every product below fits an i64 on its own and every cofactor is a
    // valid difference of two i32 products, but the three terms sum to
    // exactly -2^63 -- and negating that, which both the staged divide and
    // its rounding helper do, is undefined behaviour.
    //
    // Constructed rather than found: 2^63 - 1 factors as
    // 7 * 73 * 127 * 337 * 92737 * 649657, so one term can be made exactly
    // -(2^63 - 1) with i32 entries, and a second contributes the remaining
    // -1. Reported by review on FastLED#4305.
    i32 out[3][3];
    const i32 int64_min_determinant[3][3] = {
        {-92737, 64896, 1}, {0, 64897, 1}, {1, 0, 1532540863}};
    FL_CHECK(!invert3x3Q16(int64_min_determinant, out));
}

FL_TEST_CASE("Q16 inverse rounds rather than truncates") {
    // Truncation biases every coefficient toward zero, and a solve sums three
    // of them, so the bias does not cancel across a row. 3/2 inverts to 2/3,
    // which is 43690.667 in s16.16 -- rounding gives 43691 and truncation
    // 43690, so the two are distinguishable. `diag(3)` would not do: 1/3 is
    // 21845.33 and both give 21845.
    i32 out[3][3];
    const i32 three_halves[3][3] = {
        {98304, 0, 0}, {0, 98304, 0}, {0, 0, 98304}};
    FL_REQUIRE(invert3x3Q16(three_halves, out));
    FL_CHECK_EQ(out[0][0], 43691);
}

}  // FL_TEST_FILE

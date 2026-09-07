// White-preferred allocation coverage for color pipeline P7 / C3 (#4041).

#include "fl/gfx/white_allocation.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/math/math.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// The corpus's RGB primaries: sRGB chromaticities, unit luminance each.
EmitterProfile rgbDevice() {
    EmitterProfile p = {};
    p.xy_r[0] = 0.6400f; p.xy_r[1] = 0.3300f;
    p.xy_g[0] = 0.3000f; p.xy_g[1] = 0.6000f;
    p.xy_b[0] = 0.1500f; p.xy_b[1] = 0.0600f;
    p.lum_r = 1.0f; p.lum_g = 1.0f; p.lum_b = 1.0f;
    p.native_code_depth = 8;
    return p;
}

/// The two white emitters the corpus uses, at unit luminance, in s16.16.
constexpr i32 kWhiteD65[3] = {62289, 65536, 71372};
constexpr i32 kWhiteD50[3] = {63196, 65536, 54074};

constexpr i32 kFullDrive = 65536;

i32 q16(float v) { return static_cast<i32>(v * 65536.0f + (v >= 0 ? 0.5f : -0.5f)); }
float toFloat(i32 v) { return static_cast<float>(v) / 65536.0f; }

struct Vector {
    i32 xyz[3];
    i32 drives[4];
};

/// Sampled from `ci/golden/color-reference-v1.json` -- the brightest six and
/// the two dimmest of each device, so the set spans full-drive white, a
/// saturated primary mix, and the dark floor.
const Vector kRgbwVectors[] = {
    {{  51661,  61565,   9162}, { 14505, 47060,     0,     0}},
    {{  50461,  60805,   9078}, { 13936, 46869,     0,     0}},
    {{  49779,  60243,   9006}, { 13657, 46586,     0,     0}},
    {{  55122,  57995,  63160}, {     0,     0,     0, 57995}},
    {{  54277,  57106,  62192}, {     0,     0,     0, 57106}},
    {{      4,      2,     21}, {     0,     0,     2,     0}},
    {{      4,      1,     19}, {     0,     0,     1,     0}},
};

const Vector kNonD65Vectors[] = {
    {{  55145,  62281,   9117}, { 16677, 45605,     0,     0}},
    {{  53903,  61489,   9029}, { 16089, 45401,     0,     0}},
    {{  53189,  60913,   8956}, { 15794, 45119,     0,     0}},
    {{  55924,  57995,  47852}, {     0,     0,     0, 57995}},
    {{  55067,  57106,  47118}, {     0,     0,     0, 57106}},
    {{      3,      1,     16}, {     0,     0,     1,     0}},
    {{      3,      1,     14}, {     0,     0,     1,     0}},
};

void checkAgainstReference(const i32 (&white)[3], const Vector* vectors,
                           int count) {
    WhiteAllocationQ16 allocation;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), white, &allocation));
    for (int v = 0; v < count; ++v) {
        i32 drives[4];
        FL_REQUIRE(allocateWhitePreferredQ16(allocation, vectors[v].xyz, drives));
        for (int i = 0; i < 4; ++i) {
            // 256 raw units is one code at 8-bit output. The reference
            // inverts in float64; this quantizes the matrix first, so
            // bit-exactness is not the claim.
            FL_CHECK_LT(fl::fabsf(toFloat(drives[i] - vectors[v].drives[i])),
                        256.0f / 65536.0f);
        }
    }
}

}  // namespace

FL_TEST_CASE("White allocation reproduces the reference on rgbw") {
    // The claim the closed form rests on. The reference reaches these drives
    // by enumerating vertices; if this agrees, the per-pixel path does not
    // need the enumeration. See docs/color-gamut-algorithm-selection.md.
    checkAgainstReference(kWhiteD65,
                          kRgbwVectors,
                          static_cast<int>(sizeof(kRgbwVectors) / sizeof(Vector)));
}

FL_TEST_CASE("White allocation reproduces the reference with an off-axis white") {
    // Nothing in the derivation assumed the white sat on the neutral axis.
    // This device puts it at D50, and is what says so.
    checkAgainstReference(kWhiteD50,
                          kNonD65Vectors,
                          static_cast<int>(sizeof(kNonD65Vectors) / sizeof(Vector)));
}

FL_TEST_CASE("White allocation prefers white as far as the target allows") {
    // White-*preferred* is the policy, so the returned level must be the
    // largest one the RGB bounds permit -- compared against that bound
    // computed independently in float here, rather than against a nudge.
    //
    // A nudge cannot do this job. The earlier version pushed the level up by
    // 1024 units and asked whether an RGB drive left a 2-unit window; with
    // the smallest slope here at 0.072, that accepts about 1022 units of
    // under-allocation before it notices, and the reconstruction test does
    // not catch the difference because the RGB drives absorb it.
    WhiteAllocationQ16 allocation;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65, &allocation));

    int exercised = 0;
    for (int step = 1; step <= 12; ++step) {
        const float luminance = static_cast<float>(step) / 12.0f;
        float xyz_f[3];
        colorimetric_response::xyY_to_XYZ(0.3127f, 0.3290f, luminance, xyz_f);
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};
        i32 drives[4];
        FL_REQUIRE(allocateWhitePreferredQ16(allocation, xyz, drives));

        i32 at_zero[3];
        solveRgbDrivesQ16(allocation.rgb_solve, xyz, at_zero);
        float largest_allowed = 1.0f;
        for (int i = 0; i < 3; ++i) {
            const float slope = toFloat(allocation.per_white[i]);
            const float start_drive = toFloat(at_zero[i]);
            if (slope > 0.0f) {
                if (start_drive / slope < largest_allowed) {
                    largest_allowed = start_drive / slope;
                }
            } else if (slope < 0.0f) {
                if ((start_drive - 1.0f) / slope < largest_allowed) {
                    largest_allowed = (start_drive - 1.0f) / slope;
                }
            }
        }
        if (largest_allowed >= 1.0f) {
            continue;  // capped by the emitter, not by the RGB bounds
        }
        ++exercised;
        // 16 raw units covers the division's truncation on three bounds.
        FL_CHECK_LT(fl::fabsf(toFloat(drives[3]) - largest_allowed),
                    16.0f / 65536.0f);
    }
    // Guard against the sweep finding only capped cases, which would make
    // every check above vacuous.
    FL_CHECK_GT(exercised, 4);
}

FL_TEST_CASE("White allocation survives a white emitter dim enough to overflow") {
    // Regression. The bounds are (drive << 16) / slope, and `slope` is the
    // RGB drive one unit of white replaces -- small for a dim white emitter.
    // At a slope of one raw unit and a full-scale numerator the quotient is
    // 2^32, past i32, where narrowing is implementation-defined and could
    // turn a valid upper bound into zero and suppress the white entirely.
    //
    // A white emitter at a ten-thousandth of the primaries' luminance puts
    // the slopes down in the single raw units.
    const i32 dim_white[3] = {6, 7, 7};

    WhiteAllocationQ16 allocation;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), dim_white, &allocation));
    // Pin that the fixture really does reach the small-slope case, so this
    // cannot go quiet if the solve or the constants move.
    i32 largest_slope = 0;
    for (int i = 0; i < 3; ++i) {
        if (allocation.per_white[i] > largest_slope) {
            largest_slope = allocation.per_white[i];
        }
    }
    FL_REQUIRE_GT(largest_slope, 0);
    FL_REQUIRE_LE(largest_slope, 16);

    float xyz_f[3];
    colorimetric_response::xyY_to_XYZ(0.3127f, 0.3290f, 0.5f, xyz_f);
    const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};
    i32 drives[4];
    FL_REQUIRE(allocateWhitePreferredQ16(allocation, xyz, drives));
    for (int i = 0; i < 4; ++i) {
        FL_CHECK_GE(drives[i], 0);
        FL_CHECK_LE(drives[i], kFullDrive);
    }
    // A white this dim cannot displace much, so the RGB drives must still
    // carry the colour -- the failure mode here is not a wrong white level
    // but a wrapped bound that suppresses or saturates it.
    FL_CHECK_GT(drives[0] + drives[1] + drives[2], kFullDrive / 4);
}

FL_TEST_CASE("White allocation reproduces the target it was given") {
    // The allocation must be a *solve*, not an approximation: pushing the
    // four drives back through the emitter columns has to land on the
    // target. This is what would break if the white column were subtracted
    // with the wrong sign or scale.
    WhiteAllocationQ16 allocation;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65, &allocation));

    const float emitters[3][2] = {
        {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
    for (const auto& vector : kRgbwVectors) {
        i32 drives[4];
        FL_REQUIRE(allocateWhitePreferredQ16(allocation, vector.xyz, drives));
        float reproduced[3] = {0.0f, 0.0f, 0.0f};
        for (int e = 0; e < 3; ++e) {
            float column[3];
            colorimetric_response::xyY_to_XYZ(emitters[e][0], emitters[e][1], 1.0f,
                                              column);
            for (int i = 0; i < 3; ++i) {
                reproduced[i] += toFloat(drives[e]) * column[i];
            }
        }
        for (int i = 0; i < 3; ++i) {
            reproduced[i] += toFloat(drives[3]) * toFloat(kWhiteD65[i]);
        }
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_LT(fl::fabsf(reproduced[i] - toFloat(vector.xyz[i])), 0.01f);
        }
    }
}

FL_TEST_CASE("White allocation refuses a target outside the hull") {
    WhiteAllocationQ16 allocation;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65, &allocation));

    // A saturated chromaticity well outside the sRGB triangle, and an
    // over-bright neutral. Neither has a white level that rescues it, and
    // saying so is this function's job -- clamping belongs to the mapper.
    int refused = 0;
    const float outside[][3] = {
        {0.7500f, 0.2400f, 0.5f},
        {0.0800f, 0.8000f, 0.5f},
        {0.3127f, 0.3290f, 40.0f},
    };
    for (const auto& sample : outside) {
        float xyz_f[3];
        colorimetric_response::xyY_to_XYZ(sample[0], sample[1], sample[2], xyz_f);
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};
        i32 drives[4];
        if (!allocateWhitePreferredQ16(allocation, xyz, drives)) {
            ++refused;
        }
    }
    FL_CHECK_EQ(refused, 3);
}

FL_TEST_CASE("White allocation rejects profiles it cannot work with") {
    WhiteAllocationQ16 allocation;
    FL_CHECK_FALSE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65, nullptr));

    EmitterProfile collinear = rgbDevice();
    collinear.xy_g[0] = 0.6400f;
    collinear.xy_g[1] = 0.3300f;
    FL_CHECK_FALSE(buildWhiteAllocationQ16(collinear, kWhiteD65, &allocation));

    // A white emitter with no light in it leaves nothing to trade against.
    const i32 dark[3] = {0, 0, 0};
    FL_CHECK_FALSE(buildWhiteAllocationQ16(rgbDevice(), dark, &allocation));
}

}  // FL_TEST_FILE

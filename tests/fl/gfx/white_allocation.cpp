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
    // White-*preferred* is the policy, so nudging the white level up has to
    // push an RGB drive out of range. Checked directly rather than trusted.
    WhiteAllocationQ16 allocation;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65, &allocation));

    int exercised = 0;
    for (int step = 1; step <= 12; ++step) {
        // Neutral-ish targets at a range of luminances, where white has the
        // most to offer.
        const float luminance = static_cast<float>(step) / 12.0f;
        float xyz_f[3];
        colorimetric_response::xyY_to_XYZ(0.3127f, 0.3290f, luminance, xyz_f);
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};
        i32 drives[4];
        FL_REQUIRE(allocateWhitePreferredQ16(allocation, xyz, drives));
        if (drives[3] >= kFullDrive) {
            continue;  // capped by the emitter, not by the RGB bounds
        }
        ++exercised;
        i32 at_zero[3];
        solveRgbDrivesQ16(allocation.rgb_solve, xyz, at_zero);
        // The nudge has to be big enough that the *binding* drive clears the
        // escape window, and the binding one may have a small slope: blue
        // carries only 0.072 of a unit of white here, so a 256-unit nudge
        // moves it 18 and would look like no escape at all. 1024 moves it 74,
        // against a window of 2 units for the division's truncation.
        const i32 nudged = drives[3] + 1024;
        bool escaped = false;
        for (int i = 0; i < 3; ++i) {
            const i64 product =
                static_cast<i64>(allocation.per_white[i]) * static_cast<i64>(nudged);
            const i32 drive = at_zero[i] - static_cast<i32>((product + 32768) >> 16);
            if (drive < -2 || drive > kFullDrive + 2) {
                escaped = true;
            }
        }
        FL_CHECK(escaped);
    }
    // Guard against the sweep finding only capped cases, which would make
    // every check above vacuous.
    FL_CHECK_GT(exercised, 4);
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

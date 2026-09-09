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
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), white,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &allocation));
    for (int v = 0; v < count; ++v) {
        i32 drives[4];
        FL_REQUIRE(allocateEmitterDrivesQ16(allocation, vectors[v].xyz, drives));
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
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &allocation));

    int exercised = 0;
    for (int step = 1; step <= 12; ++step) {
        const float luminance = static_cast<float>(step) / 12.0f;
        float xyz_f[3];
        colorimetric_response::xyY_to_XYZ(0.3127f, 0.3290f, luminance, xyz_f);
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};
        i32 drives[4];
        FL_REQUIRE(allocateEmitterDrivesQ16(allocation, xyz, drives));

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
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), dim_white,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &allocation));
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
    FL_REQUIRE(allocateEmitterDrivesQ16(allocation, xyz, drives));
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
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &allocation));

    const float emitters[3][2] = {
        {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
    for (const auto& vector : kRgbwVectors) {
        i32 drives[4];
        FL_REQUIRE(allocateEmitterDrivesQ16(allocation, vector.xyz, drives));
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
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &allocation));

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
        if (!allocateEmitterDrivesQ16(allocation, xyz, drives)) {
            ++refused;
        }
    }
    FL_CHECK_EQ(refused, 3);
}

FL_TEST_CASE("RGB-preferred takes the other end of the same interval") {
    // C3's per-profile override. Both policies reproduce the target exactly
    // -- they pick different points on the same feasible interval -- so the
    // test is that they differ in white and agree in the light they make.
    WhiteAllocationQ16 white_first;
    WhiteAllocationQ16 rgb_first;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &white_first));
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                       WhiteAllocationPolicy::RgbPreferred,
                                       &rgb_first));

    const float emitters[3][2] = {
        {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
    int differed = 0;
    for (int step = 1; step <= 12; ++step) {
        const float luminance = static_cast<float>(step) / 12.0f;
        float xyz_f[3];
        colorimetric_response::xyY_to_XYZ(0.3127f, 0.3290f, luminance, xyz_f);
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};

        i32 with_white[4];
        i32 with_rgb[4];
        FL_REQUIRE(allocateEmitterDrivesQ16(white_first, xyz, with_white));
        FL_REQUIRE(allocateEmitterDrivesQ16(rgb_first, xyz, with_rgb));

        // RGB-preferred never uses more white than white-preferred.
        FL_CHECK_LE(with_rgb[3], with_white[3]);
        if (with_rgb[3] < with_white[3]) {
            ++differed;
        }

        // Both must land on the same colour. Reconstructed through the
        // emitter columns rather than compared drive by drive, since the
        // whole point is that the drives differ.
        for (int which = 0; which < 2; ++which) {
            const i32* drives = which == 0 ? with_white : with_rgb;
            float made[3] = {0.0f, 0.0f, 0.0f};
            for (int e = 0; e < 3; ++e) {
                float column[3];
                colorimetric_response::xyY_to_XYZ(emitters[e][0], emitters[e][1],
                                                  1.0f, column);
                for (int i = 0; i < 3; ++i) {
                    made[i] += toFloat(drives[e]) * column[i];
                }
            }
            for (int i = 0; i < 3; ++i) {
                made[i] += toFloat(drives[3]) * toFloat(kWhiteD65[i]);
            }
            for (int i = 0; i < 3; ++i) {
                FL_CHECK_LT(fl::fabsf(made[i] - toFloat(xyz[i])), 0.01f);
            }
        }
    }
    // Guard against the two policies never actually diverging, which would
    // make every check above vacuous. A D65 neutral is exactly the case
    // where white can do all the work or none of it.
    FL_CHECK_GT(differed, 8);
}

FL_TEST_CASE("RGB-preferred still uses white when RGB alone cannot reach") {
    // The override is not "ignore the white emitter". Where the primaries
    // cannot reach the target on their own, this must return the smallest
    // white level that makes it reachable rather than failing.
    WhiteAllocationQ16 rgb_first;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                       WhiteAllocationPolicy::RgbPreferred,
                                       &rgb_first));

    // Brighter than the RGB primaries can manage alone, but inside the
    // four-emitter hull.
    float xyz_f[3];
    colorimetric_response::xyY_to_XYZ(0.3127f, 0.3290f, 2.0f, xyz_f);
    const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};

    // Pin the premise: RGB alone really cannot do this one.
    i32 rgb_only[3];
    solveRgbDrivesQ16(rgb_first.rgb_solve, xyz, rgb_only);
    bool rgb_alone_fails = false;
    for (int i = 0; i < 3; ++i) {
        if (rgb_only[i] > kFullDrive) {
            rgb_alone_fails = true;
        }
    }
    FL_REQUIRE(rgb_alone_fails);

    i32 drives[4];
    FL_REQUIRE(allocateEmitterDrivesQ16(rgb_first, xyz, drives));
    for (int i = 0; i < 4; ++i) {
        FL_CHECK_GE(drives[i], 0);
        FL_CHECK_LE(drives[i], kFullDrive);
    }

    // The *smallest* white that works, not merely some white. Checking only
    // that the level is above zero would pass for any feasible answer,
    // including the white-preferred one, which is the whole thing this test
    // is supposed to distinguish.
    //
    // Derived here from the drives RGB alone would need: every channel above
    // full scale has to be brought down, and the one needing the most white
    // to get there sets the floor.
    float smallest_that_works = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float slope = toFloat(rgb_first.per_white[i]);
        const float start = toFloat(rgb_only[i]);
        float needed = 0.0f;
        if (slope > 0.0f) {
            needed = (start - 1.0f) / slope;
        } else if (slope < 0.0f) {
            needed = start / slope;
        }
        if (needed > smallest_that_works) {
            smallest_that_works = needed;
        }
    }
    FL_REQUIRE_GT(smallest_that_works, 0.0f);
    FL_CHECK_LT(fl::fabsf(toFloat(drives[3]) - smallest_that_works),
                16.0f / 65536.0f);

    // And it really is less than what white-preferred would have chosen, so
    // the two policies are distinguishable on this target.
    WhiteAllocationQ16 white_first;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &white_first));
    i32 white_drives[4];
    FL_REQUIRE(allocateEmitterDrivesQ16(white_first, xyz, white_drives));
    FL_CHECK_LT(drives[3], white_drives[3]);
}

FL_TEST_CASE("White allocation rejects profiles it cannot work with") {
    WhiteAllocationQ16 allocation;
    FL_CHECK_FALSE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                        WhiteAllocationPolicy::WhitePreferred,
                                        nullptr));

    EmitterProfile collinear = rgbDevice();
    collinear.xy_g[0] = 0.6400f;
    collinear.xy_g[1] = 0.3300f;
    FL_CHECK_FALSE(buildWhiteAllocationQ16(collinear, kWhiteD65,
                                        WhiteAllocationPolicy::WhitePreferred,
                                        &allocation));

    // A white emitter with no light in it leaves nothing to trade against.
    const i32 dark[3] = {0, 0, 0};
    FL_CHECK_FALSE(buildWhiteAllocationQ16(rgbDevice(), dark,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &allocation));
}


// ---------------------------------------------------------------------------
// Two whites (#4198)
// ---------------------------------------------------------------------------

namespace {

/// The corpus's two-white device: a cool white at D65 and a warm one at D50,
/// each at unit luminance, in s16.16.
const i32 (&kCoolWhite)[3] = kWhiteD65;
const i32 (&kWarmWhite)[3] = kWhiteD50;

/// A float model of the same device, built from the chromaticities rather
/// than from anything the code under test computes.
///
/// This is the oracle, so it must not share a line of arithmetic with the
/// implementation: it inverts in float, enumerates vertices, and knows
/// nothing about s16.16.
struct FloatDevice {
    float inverse[3][3];  // M^-1, columns R G B
    float per_white1[3];  // M^-1 . w1
    float per_white2[3];  // M^-1 . w2
    float column1[3];     // w1 XYZ
    float column2[3];     // w2 XYZ
    float primaries[3][3];
};

void primaryXyz(const float (&xy)[2], float lum, float (&out)[3]) {
    colorimetric_response::xyY_to_XYZ(xy[0], xy[1], lum, out);
}

bool invertFloat3(const float (&m)[3][3], float (&out)[3][3]) {
    const float det =
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (fl::fabsf(det) < 1e-9f) {
        return false;
    }
    const float inv = 1.0f / det;
    out[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inv;
    out[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inv;
    out[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv;
    out[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * inv;
    out[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv;
    out[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inv;
    out[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inv;
    out[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inv;
    out[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv;
    return true;
}

void applyFloat3(const float (&m)[3][3], const float (&v)[3], float (&out)[3]) {
    for (int r = 0; r < 3; ++r) {
        out[r] = m[r][0] * v[0] + m[r][1] * v[1] + m[r][2] * v[2];
    }
}

FloatDevice makeFloatDevice(const i32 (&white1)[3], const i32 (&white2)[3]) {
    const EmitterProfile profile = rgbDevice();
    FloatDevice device = {};
    float r[3];
    float g[3];
    float b[3];
    primaryXyz(profile.xy_r, profile.lum_r, r);
    primaryXyz(profile.xy_g, profile.lum_g, g);
    primaryXyz(profile.xy_b, profile.lum_b, b);
    float matrix[3][3];
    for (int i = 0; i < 3; ++i) {
        matrix[i][0] = r[i];
        matrix[i][1] = g[i];
        matrix[i][2] = b[i];
        device.primaries[i][0] = r[i];
        device.primaries[i][1] = g[i];
        device.primaries[i][2] = b[i];
        device.column1[i] = toFloat(white1[i]);
        device.column2[i] = toFloat(white2[i]);
    }
    FL_REQUIRE(invertFloat3(matrix, device.inverse));
    applyFloat3(device.inverse, device.column1, device.per_white1);
    applyFloat3(device.inverse, device.column2, device.per_white2);
    return device;
}

/// The largest reachable `w1 + w2` for a target, by vertex enumeration.
///
/// The feasible set in (w1, w2) is a polygon cut by ten half-planes -- the
/// two white drives and the three RGB drives, each bounded twice. A linear
/// objective over a polygon is maximized at a vertex, so intersecting every
/// pair of boundary lines and keeping the feasible intersections finds the
/// optimum exactly. This is the enumeration A3/B11 forbids per pixel, which
/// is precisely why it belongs in the test and not in the shipped path.
struct Optimum {
    bool feasible;
    float total;
};

Optimum bruteForceMaxWhite(const FloatDevice& device, const float (&target)[3]) {
    float at_zero[3];
    applyFloat3(device.inverse, target, at_zero);

    // Each row is `a*w1 + b*w2 <= c`.
    float rows[10][3];
    int count = 0;
    rows[count][0] = -1.0f; rows[count][1] = 0.0f; rows[count][2] = 0.0f; ++count;
    rows[count][0] = 1.0f;  rows[count][1] = 0.0f; rows[count][2] = 1.0f; ++count;
    rows[count][0] = 0.0f;  rows[count][1] = -1.0f; rows[count][2] = 0.0f; ++count;
    rows[count][0] = 0.0f;  rows[count][1] = 1.0f;  rows[count][2] = 1.0f; ++count;
    for (int i = 0; i < 3; ++i) {
        // d_i = at_zero_i - w1*p1_i - w2*p2_i, needing 0 <= d_i <= 1.
        rows[count][0] = device.per_white1[i];
        rows[count][1] = device.per_white2[i];
        rows[count][2] = at_zero[i];
        ++count;
        rows[count][0] = -device.per_white1[i];
        rows[count][1] = -device.per_white2[i];
        rows[count][2] = 1.0f - at_zero[i];
        ++count;
    }

    Optimum best = {false, 0.0f};
    const float tolerance = 1e-4f;
    for (int a = 0; a < count; ++a) {
        for (int b = a + 1; b < count; ++b) {
            const float det = rows[a][0] * rows[b][1] - rows[a][1] * rows[b][0];
            if (fl::fabsf(det) < 1e-7f) {
                continue;
            }
            const float w1 = (rows[a][2] * rows[b][1] - rows[a][1] * rows[b][2]) / det;
            const float w2 = (rows[a][0] * rows[b][2] - rows[a][2] * rows[b][0]) / det;
            bool inside = true;
            for (int k = 0; k < count && inside; ++k) {
                inside = rows[k][0] * w1 + rows[k][1] * w2 <= rows[k][2] + tolerance;
            }
            if (!inside) {
                continue;
            }
            if (!best.feasible || w1 + w2 > best.total) {
                best.feasible = true;
                best.total = w1 + w2;
            }
        }
    }
    return best;
}

/// The XYZ five drives produce on the float model.
void reproduce(const FloatDevice& device, const float (&drives)[5],
               float (&out)[3]) {
    for (int i = 0; i < 3; ++i) {
        out[i] = device.primaries[i][0] * drives[0] +
                 device.primaries[i][1] * drives[1] +
                 device.primaries[i][2] * drives[2] +
                 device.column1[i] * drives[3] + device.column2[i] * drives[4];
    }
}

/// A target built from drives, so it is reachable by construction.
void targetFromDrives(const FloatDevice& device, const float (&drives)[5],
                      i32 (&out)[3]) {
    float xyz[3];
    reproduce(device, drives, xyz);
    for (int i = 0; i < 3; ++i) {
        out[i] = q16(xyz[i]);
    }
}

TwoWhiteAllocationQ16 buildTwoWhite(WhiteAllocationPolicy policy) {
    TwoWhiteAllocationQ16 allocation;
    FL_REQUIRE(buildTwoWhiteAllocationQ16(rgbDevice(), kCoolWhite, kWarmWhite,
                                          policy, &allocation));
    return allocation;
}

}  // namespace

FL_TEST_CASE("Two-white allocation reproduces the target it was given") {
    // The identity the whole method rests on: whichever total and split come
    // back, the five drives must still make the requested colour.
    const FloatDevice device = makeFloatDevice(kCoolWhite, kWarmWhite);
    const TwoWhiteAllocationQ16 allocation =
        buildTwoWhite(WhiteAllocationPolicy::WhitePreferred);

    const float samples[][5] = {
        {0.20f, 0.40f, 0.60f, 0.30f, 0.10f},
        {0.90f, 0.10f, 0.05f, 0.00f, 0.00f},
        {0.05f, 0.05f, 0.05f, 0.90f, 0.90f},
        {0.00f, 0.00f, 0.00f, 1.00f, 1.00f},
        {1.00f, 1.00f, 1.00f, 1.00f, 1.00f},
        {0.33f, 0.66f, 0.99f, 0.50f, 0.50f},
        {0.01f, 0.02f, 0.03f, 0.04f, 0.05f},
    };
    const int count = static_cast<int>(sizeof(samples) / sizeof(samples[0]));
    for (int v = 0; v < count; ++v) {
        i32 xyz[3];
        targetFromDrives(device, samples[v], xyz);
        i32 drives[5];
        FL_REQUIRE(allocateTwoWhiteDrivesQ16(allocation, xyz, drives));
        float as_float[5];
        for (int i = 0; i < 5; ++i) {
            as_float[i] = toFloat(drives[i]);
        }
        float reproduced[3];
        reproduce(device, as_float, reproduced);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_LT(fl::fabsf(reproduced[i] - toFloat(xyz[i])), 0.01f);
        }
    }
}

FL_TEST_CASE("Two-white allocation takes the largest reachable total") {
    // White-preferred over two whites is a linear program, and this asks
    // whether the closed form finds its optimum -- against an independent
    // vertex enumeration, not against a nudge.
    const FloatDevice device = makeFloatDevice(kCoolWhite, kWarmWhite);
    const TwoWhiteAllocationQ16 allocation =
        buildTwoWhite(WhiteAllocationPolicy::WhitePreferred);

    int exercised = 0;
    for (int r = 0; r <= 4; ++r) {
        for (int g = 0; g <= 4; ++g) {
            for (int b = 0; b <= 4; ++b) {
                for (int w = 0; w <= 4; ++w) {
                    const float sample[5] = {
                        static_cast<float>(r) / 4.0f, static_cast<float>(g) / 4.0f,
                        static_cast<float>(b) / 4.0f, static_cast<float>(w) / 4.0f,
                        static_cast<float>(4 - w) / 4.0f};
                    i32 xyz[3];
                    targetFromDrives(device, sample, xyz);
                    float as_target[3];
                    for (int i = 0; i < 3; ++i) {
                        as_target[i] = toFloat(xyz[i]);
                    }
                    const Optimum best = bruteForceMaxWhite(device, as_target);
                    i32 drives[5];
                    const bool solved =
                        allocateTwoWhiteDrivesQ16(allocation, xyz, drives);
                    FL_REQUIRE(best.feasible);
                    FL_REQUIRE(solved);
                    const float total = toFloat(drives[3]) + toFloat(drives[4]);
                    // One 8-bit code, the same tolerance the one-white
                    // vectors carry: the oracle inverts in float, this
                    // quantizes the matrix first.
                    FL_CHECK_LT(fl::fabsf(total - best.total), 2.0f / 255.0f);
                    ++exercised;
                }
            }
        }
    }
    FL_CHECK_EQ(exercised, 625);
}

FL_TEST_CASE("Two-white allocation solves targets one white cannot reach") {
    // The case the first attempt got wrong (#4198). The achievable totals
    // form an interval that need not start at zero: these targets are
    // unreachable with the primaries alone *and* unreachable with either
    // white on its own, so anything that searches upward from zero, or that
    // runs the one-white form twice and keeps the better answer, reports
    // them as outside the gamut.
    const FloatDevice device = makeFloatDevice(kCoolWhite, kWarmWhite);
    const TwoWhiteAllocationQ16 allocation =
        buildTwoWhite(WhiteAllocationPolicy::WhitePreferred);

    int exercised = 0;
    for (int step = 0; step <= 8; ++step) {
        const float lean = static_cast<float>(step) / 8.0f;
        // Green at full with both whites near full. Green is the channel
        // this device's whites load most heavily, so the target needs more
        // green than one white plus a full green primary can supply -- which
        // is what makes it a two-white target rather than merely a bright
        // one. The `lean` slides the split without moving the total, so the
        // family also says the answer does not depend on which white is
        // asked first.
        const float sample[5] = {0.10f, 1.00f, 0.10f, 0.80f + 0.2f * lean,
                                 1.00f - 0.2f * lean};
        i32 xyz[3];
        targetFromDrives(device, sample, xyz);

        // Confirm the premise rather than asserting it: one white capped at
        // full drive cannot reach this target.
        WhiteAllocationQ16 single;
        FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kCoolWhite,
                                           WhiteAllocationPolicy::WhitePreferred,
                                           &single));
        i32 single_drives[4];
        FL_CHECK_FALSE(allocateEmitterDrivesQ16(single, xyz, single_drives));

        i32 drives[5];
        FL_REQUIRE(allocateTwoWhiteDrivesQ16(allocation, xyz, drives));
        FL_CHECK_GT(toFloat(drives[3]) + toFloat(drives[4]), 1.0f);
        ++exercised;
    }
    FL_CHECK_EQ(exercised, 9);
}

FL_TEST_CASE("Two-white allocation rejects targets outside the hull") {
    const FloatDevice device = makeFloatDevice(kCoolWhite, kWarmWhite);
    const TwoWhiteAllocationQ16 allocation =
        buildTwoWhite(WhiteAllocationPolicy::WhitePreferred);

    // Every emitter at full drive, then half as much again. No combination
    // of drives in [0, 1] reaches it.
    const float full[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float xyz_f[3];
    reproduce(device, full, xyz_f);
    const i32 beyond[3] = {q16(xyz_f[0] * 1.5f), q16(xyz_f[1] * 1.5f),
                           q16(xyz_f[2] * 1.5f)};
    i32 drives[5];
    FL_CHECK_FALSE(allocateTwoWhiteDrivesQ16(allocation, beyond, drives));
}

FL_TEST_CASE("Two-white RGB-preferred takes the other end of the total") {
    // The per-profile override C3 asks for. Same targets, same feasible
    // interval; this end is its minimum, and for a target the primaries can
    // reach on their own that minimum is no white at all.
    const FloatDevice device = makeFloatDevice(kCoolWhite, kWarmWhite);
    const TwoWhiteAllocationQ16 white_first =
        buildTwoWhite(WhiteAllocationPolicy::WhitePreferred);
    const TwoWhiteAllocationQ16 rgb_first =
        buildTwoWhite(WhiteAllocationPolicy::RgbPreferred);

    const float sample[5] = {0.30f, 0.20f, 0.10f, 0.00f, 0.00f};
    i32 xyz[3];
    targetFromDrives(device, sample, xyz);

    i32 white_drives[5];
    i32 rgb_drives[5];
    FL_REQUIRE(allocateTwoWhiteDrivesQ16(white_first, xyz, white_drives));
    FL_REQUIRE(allocateTwoWhiteDrivesQ16(rgb_first, xyz, rgb_drives));

    const float white_total = toFloat(white_drives[3]) + toFloat(white_drives[4]);
    const float rgb_total = toFloat(rgb_drives[3]) + toFloat(rgb_drives[4]);
    FL_CHECK_LT(rgb_total, white_total);
    FL_CHECK_LT(rgb_total, 1.0f / 255.0f);

    // And it still reproduces the target, so this is a different point on
    // the same feasible set rather than a different answer.
    float as_float[5];
    for (int i = 0; i < 5; ++i) {
        as_float[i] = toFloat(rgb_drives[i]);
    }
    float reproduced[3];
    reproduce(device, as_float, reproduced);
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_LT(fl::fabsf(reproduced[i] - toFloat(xyz[i])), 0.01f);
    }
}


FL_TEST_CASE("Two-white allocation leaves no freedom in the split") {
    // Why the shipped path takes an end of the split interval rather than
    // optimizing over it: at the extreme total there is nothing to optimize.
    // Scanned exactly -- drives in [0, 1] with only enough room for the
    // quantization of the target -- the feasible splits are a point.
    //
    // The implementation's slack widens that to about 0.05 here, because a
    // drive may sit 64 raw units outside range and the difference column's
    // smallest entry is 0.017; the second check is that the split it picks
    // stays inside the widened set rather than wandering off it.
    const FloatDevice device = makeFloatDevice(kCoolWhite, kWarmWhite);
    const TwoWhiteAllocationQ16 allocation =
        buildTwoWhite(WhiteAllocationPolicy::WhitePreferred);

    const float samples[][5] = {
        {0.20f, 0.40f, 0.60f, 0.30f, 0.10f},
        {0.10f, 0.90f, 0.20f, 0.70f, 0.60f},
        {0.05f, 0.05f, 0.05f, 0.90f, 0.90f},
        {0.33f, 0.66f, 0.99f, 0.50f, 0.50f},
        {0.40f, 0.30f, 0.20f, 0.10f, 0.80f},
    };
    const int count = static_cast<int>(sizeof(samples) / sizeof(samples[0]));
    const float kSlack = 64.0f / 65536.0f;
    for (int v = 0; v < count; ++v) {
        i32 xyz[3];
        targetFromDrives(device, samples[v], xyz);
        i32 drives[5];
        FL_REQUIRE(allocateTwoWhiteDrivesQ16(allocation, xyz, drives));

        const float total = toFloat(drives[3]) + toFloat(drives[4]);
        const float chosen = toFloat(drives[3]);
        float target_f[3];
        for (int i = 0; i < 3; ++i) {
            target_f[i] = toFloat(xyz[i]);
        }
        float at_zero[3];
        applyFloat3(device.inverse, target_f, at_zero);

        float exact_low = 2.0f;
        float exact_high = -1.0f;
        float slack_low = 2.0f;
        float slack_high = -1.0f;
        const int kSteps = 8192;
        for (int step = 0; step <= kSteps; ++step) {
            const float split =
                total * static_cast<float>(step) / static_cast<float>(kSteps);
            if (split > 1.0f + kSlack || total - split > 1.0f + kSlack) {
                continue;
            }
            float worst = 0.0f;
            for (int i = 0; i < 3; ++i) {
                const float drive = at_zero[i] - split * device.per_white1[i] -
                                    (total - split) * device.per_white2[i];
                const float outside = drive < 0.0f ? -drive : drive - 1.0f;
                if (outside > worst) {
                    worst = outside;
                }
            }
            // 1e-4 is the target's own quantization, not a policy: the XYZ
            // went through s16.16 before it got here.
            if (worst <= 1e-4f) {
                if (split < exact_low) { exact_low = split; }
                if (split > exact_high) { exact_high = split; }
            }
            if (worst <= kSlack) {
                if (split < slack_low) { slack_low = split; }
                if (split > slack_high) { slack_high = split; }
            }
        }
        FL_REQUIRE(exact_high >= 0.0f);
        FL_CHECK_LT(exact_high - exact_low, 0.01f);
        FL_CHECK_GE(chosen, slack_low - 0.01f);
        FL_CHECK_LE(chosen, slack_high + 0.01f);
    }
}

FL_TEST_CASE("Two-white allocation handles two whites of the same colour") {
    // The degenerate device: both whites in the same place, so the
    // difference column is zero and the split cannot move any RGB drive.
    // Every bound then falls on the total alone, which is a branch nothing
    // else in this file reaches.
    const FloatDevice device = makeFloatDevice(kCoolWhite, kCoolWhite);
    TwoWhiteAllocationQ16 allocation;
    FL_REQUIRE(buildTwoWhiteAllocationQ16(rgbDevice(), kCoolWhite, kCoolWhite,
                                          WhiteAllocationPolicy::WhitePreferred,
                                          &allocation));
    for (int i = 0; i < 3; ++i) {
        FL_REQUIRE(allocation.difference[i] == 0);
    }

    const float samples[][5] = {
        {0.20f, 0.40f, 0.60f, 0.30f, 0.10f},
        {0.05f, 0.05f, 0.05f, 0.90f, 0.90f},
        {0.10f, 1.00f, 0.10f, 0.90f, 0.90f},
        {0.60f, 0.20f, 0.10f, 0.00f, 0.00f},
    };
    const int count = static_cast<int>(sizeof(samples) / sizeof(samples[0]));
    for (int v = 0; v < count; ++v) {
        i32 xyz[3];
        targetFromDrives(device, samples[v], xyz);
        i32 drives[5];
        FL_REQUIRE(allocateTwoWhiteDrivesQ16(allocation, xyz, drives));

        float as_float[5];
        for (int i = 0; i < 5; ++i) {
            as_float[i] = toFloat(drives[i]);
        }
        float reproduced[3];
        reproduce(device, as_float, reproduced);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_LT(fl::fabsf(reproduced[i] - toFloat(xyz[i])), 0.01f);
        }

        // And the total is still maximal, which is what the lower bound on
        // the total exists to make possible for the bright ones.
        float target_f[3];
        for (int i = 0; i < 3; ++i) {
            target_f[i] = toFloat(xyz[i]);
        }
        const Optimum best = bruteForceMaxWhite(device, target_f);
        FL_REQUIRE(best.feasible);
        const float total = toFloat(drives[3]) + toFloat(drives[4]);
        FL_CHECK_LT(fl::fabsf(total - best.total), 2.0f / 255.0f);

        // This is the one device where the split really is free -- no RGB
        // drive moves with it -- so it is also the only place the choice of
        // end is observable. The reference's tie-break falls through to the
        // lexicographically smallest drives, which puts every unit it can on
        // the second white.
        const float lowest_split = total > 1.0f ? total - 1.0f : 0.0f;
        FL_CHECK_LT(fl::fabsf(toFloat(drives[3]) - lowest_split), 2.0f / 255.0f);
    }
}

FL_TEST_CASE("Two-white RGB-preferred still lights the whites when it must") {
    // RGB-preferred takes the *minimum* feasible total, and that minimum is
    // not always zero: for a target the primaries cannot reach alone, the
    // smallest total that makes it reachable is a positive number, and only
    // the lower bound on the total finds it.
    const FloatDevice device = makeFloatDevice(kCoolWhite, kWarmWhite);
    const TwoWhiteAllocationQ16 rgb_first =
        buildTwoWhite(WhiteAllocationPolicy::RgbPreferred);
    const TwoWhiteAllocationQ16 white_first =
        buildTwoWhite(WhiteAllocationPolicy::WhitePreferred);

    const float sample[5] = {0.10f, 1.00f, 0.10f, 0.90f, 0.90f};
    i32 xyz[3];
    targetFromDrives(device, sample, xyz);

    i32 rgb_drives[5];
    i32 white_drives[5];
    FL_REQUIRE(allocateTwoWhiteDrivesQ16(rgb_first, xyz, rgb_drives));
    FL_REQUIRE(allocateTwoWhiteDrivesQ16(white_first, xyz, white_drives));

    const float rgb_total = toFloat(rgb_drives[3]) + toFloat(rgb_drives[4]);
    const float white_total = toFloat(white_drives[3]) + toFloat(white_drives[4]);
    FL_CHECK_GT(rgb_total, 0.5f);
    FL_CHECK_LT(rgb_total, white_total);

    // Still the same colour: a different point on the same feasible set.
    float as_float[5];
    for (int i = 0; i < 5; ++i) {
        as_float[i] = toFloat(rgb_drives[i]);
    }
    float reproduced[3];
    reproduce(device, as_float, reproduced);
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_LT(fl::fabsf(reproduced[i] - toFloat(xyz[i])), 0.01f);
    }
}


FL_TEST_CASE("Two-white RGB-preferred finds the smallest total on equal whites") {
    // Closes the other half of the degenerate device: with the difference
    // column zero everywhere, every bound falls on the total alone, and
    // RGB-preferred needs the *lower* one of those -- which for a target the
    // primaries cannot reach is a positive number.
    const FloatDevice device = makeFloatDevice(kCoolWhite, kCoolWhite);
    TwoWhiteAllocationQ16 allocation;
    FL_REQUIRE(buildTwoWhiteAllocationQ16(rgbDevice(), kCoolWhite, kCoolWhite,
                                          WhiteAllocationPolicy::RgbPreferred,
                                          &allocation));

    const float sample[5] = {0.10f, 1.00f, 0.10f, 0.90f, 0.90f};
    i32 xyz[3];
    targetFromDrives(device, sample, xyz);
    i32 drives[5];
    FL_REQUIRE(allocateTwoWhiteDrivesQ16(allocation, xyz, drives));

    // The primaries alone cannot reach it, so the smallest feasible total is
    // well above zero.
    const float total = toFloat(drives[3]) + toFloat(drives[4]);
    FL_CHECK_GT(total, 0.5f);

    // And it is the *smallest*: one code less would put a drive out of range.
    float at_zero[3];
    float target_f[3];
    for (int i = 0; i < 3; ++i) {
        target_f[i] = toFloat(xyz[i]);
    }
    applyFloat3(device.inverse, target_f, at_zero);
    bool lower_total_is_infeasible = false;
    const float probe = total - 4.0f / 255.0f;
    for (int i = 0; i < 3; ++i) {
        const float drive = at_zero[i] - probe * device.per_white1[i];
        if (drive > 1.0f + 64.0f / 65536.0f) {
            lower_total_is_infeasible = true;
        }
    }
    FL_CHECK(lower_total_is_infeasible);

    float as_float[5];
    for (int i = 0; i < 5; ++i) {
        as_float[i] = toFloat(drives[i]);
    }
    float reproduced[3];
    reproduce(device, as_float, reproduced);
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_LT(fl::fabsf(reproduced[i] - toFloat(xyz[i])), 0.01f);
    }
}

}  // FL_TEST_FILE

// Gamut-mapping coverage for color pipeline P7 (#4041).

#include "fl/gfx/gamut_map.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/gfx/oklab_q16.h"
#include "fl/math/math.h"
#include "fl/stl/int.h"
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

constexpr i32 kFullDrive = 65536;

/// XYZ of a chromaticity at the given luminance.
void xyzAt(float x, float y, float luminance, i32 (&out)[3]) {
    float xyz[3];
    colorimetric_response::xyY_to_XYZ(x, y, luminance, xyz);
    out[0] = q16(xyz[0]);
    out[1] = q16(xyz[1]);
    out[2] = q16(xyz[2]);
}

}  // namespace

FL_TEST_CASE("Gamut map leaves an in-gamut target completely alone") {
    // The common case, and the one that must cost nothing: a target already
    // inside the hull has to come back as the plain solve would give it.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    const float kDrives[][3] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.25f, 0.75f},
        {1.0f, 0.0f, 0.0f}, {0.0f, 0.6f, 0.4f}, {0.9f, 0.9f, 0.1f},
    };
    EmitterSolveMatrixQ16 solve;
    FL_REQUIRE(buildRgbSolveMatrixQ16(rgbDevice(), &solve));
    for (const auto& want : kDrives) {
        // Build a target from drives we know are in range, by pushing them
        // forward through the emitter matrix.
        float xyz_f[3] = {0.0f, 0.0f, 0.0f};
        const float chroma[3][2] = {
            {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
        for (int e = 0; e < 3; ++e) {
            float emitter[3];
            colorimetric_response::xyY_to_XYZ(chroma[e][0], chroma[e][1], 1.0f, emitter);
            for (int i = 0; i < 3; ++i) {
                xyz_f[i] += want[e] * emitter[i];
            }
        }
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};
        i32 mapped[3];
        i32 plain[3];
        mapAndSolveDrivesQ16(map, xyz, mapped);
        solveRgbDrivesQ16(solve, xyz, plain);
        for (int i = 0; i < 3; ++i) {
            // Clamped, because a colour the device reproduces exactly still
            // round-trips a few ULP outside [0, 1]; the mapper accepts that
            // and clamps rather than compressing an in-gamut colour.
            const i32 want = plain[i] < 0 ? 0
                           : plain[i] > kFullDrive ? kFullDrive : plain[i];
            FL_CHECK_EQ(mapped[i], want);
            FL_CHECK_LT(fl::fabsf(toFloat(plain[i] - want)), 0.002f);
        }
    }
}

FL_TEST_CASE("Gamut map always returns drives inside [0, 1]") {
    // The mapper's whole postcondition. Swept over saturated targets well
    // outside the hull and over-bright ones, both of which leave the plain
    // solve out of range.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    int exercised = 0;
    for (int hue = 0; hue < 36; ++hue) {
        // Walk the spectral locus's rough neighbourhood: chromaticities well
        // outside the sRGB triangle in every direction.
        const float angle = static_cast<float>(hue) * 10.0f * 3.14159265f / 180.0f;
        const float x = 0.33f + 0.45f * fl::cosf(angle);
        const float y = 0.33f + 0.45f * fl::sinf(angle);
        if (x <= 0.01f || y <= 0.01f || x + y >= 0.99f) {
            continue;
        }
        for (float luminance : {0.2f, 1.0f, 3.0f}) {
            i32 xyz[3];
            xyzAt(x, y, luminance, xyz);
            i32 plain[3];
            solveRgbDrivesQ16(map.solve, xyz, plain);
            bool in_range = true;
            for (int i = 0; i < 3; ++i) {
                if (plain[i] < 0 || plain[i] > kFullDrive) {
                    in_range = false;
                }
            }
            if (in_range) {
                continue;  // nothing for the mapper to do here
            }
            ++exercised;
            i32 drives[3];
            mapAndSolveDrivesQ16(map, xyz, drives);
            for (int i = 0; i < 3; ++i) {
                FL_CHECK_GE(drives[i], 0);
                FL_CHECK_LE(drives[i], kFullDrive);
            }
        }
    }
    // Guard against the sweep quietly finding nothing out of gamut, which
    // would make every check above vacuous. The sweep finds 18.
    FL_CHECK_GT(exercised, 12);
}

FL_TEST_CASE("Gamut map preserves hue while compressing chroma") {
    // The objective, and the reason clipping was rejected: the mapped colour
    // must sit on the same OKLab hue line as the target. Scaling (a, b) by a
    // common factor is what guarantees it.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    const float kChroma[][2] = {
        {0.70f, 0.28f}, {0.18f, 0.72f}, {0.10f, 0.03f},
        {0.55f, 0.42f}, {0.08f, 0.55f},
    };
    for (const auto& c : kChroma) {
        i32 xyz[3];
        xyzAt(c[0], c[1], 0.5f, xyz);
        i32 drives[3];
        mapAndSolveDrivesQ16(map, xyz, drives);

        // Push the mapped drives back to XYZ, then compare hue in OKLab.
        float mapped_f[3] = {0.0f, 0.0f, 0.0f};
        const float emitters[3][2] = {
            {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
        for (int e = 0; e < 3; ++e) {
            float emitter[3];
            colorimetric_response::xyY_to_XYZ(emitters[e][0], emitters[e][1], 1.0f,
                                              emitter);
            for (int i = 0; i < 3; ++i) {
                mapped_f[i] += toFloat(drives[e]) * emitter[i];
            }
        }
        const i32 mapped_xyz[3] = {q16(mapped_f[0]), q16(mapped_f[1]), q16(mapped_f[2])};
        i32 target_lab[3];
        i32 mapped_lab[3];
        xyzToOklabQ16(xyz, target_lab);
        xyzToOklabQ16(mapped_xyz, mapped_lab);

        // Same hue means the same ratio b/a, i.e. the cross product of the
        // two chroma vectors vanishes. Compared against the product of their
        // magnitudes so the tolerance is relative, not absolute.
        const float ax = toFloat(target_lab[1]), ay = toFloat(target_lab[2]);
        const float bx = toFloat(mapped_lab[1]), by = toFloat(mapped_lab[2]);
        const float cross = fl::fabsf(ax * by - ay * bx);
        const float scale = fl::sqrtf((ax * ax + ay * ay) * (bx * bx + by * by));
        // Control: the same reconstruction with no mapping at all. Without
        // it a hue drift introduced by this test's own float round trip
        // would be indistinguishable from one introduced by the mapper --
        // and when the halving loop really was rotating hue by 1.5 degrees,
        // this is what proved the measurement was not to blame.
        i32 plain[3];
        solveRgbDrivesQ16(map.solve, xyz, plain);
        float control_f[3] = {0.0f, 0.0f, 0.0f};
        for (int e = 0; e < 3; ++e) {
            float emitter[3];
            colorimetric_response::xyY_to_XYZ(emitters[e][0], emitters[e][1], 1.0f,
                                              emitter);
            for (int i = 0; i < 3; ++i) {
                control_f[i] += toFloat(plain[e]) * emitter[i];
            }
        }
        const i32 control_xyz[3] = {q16(control_f[0]), q16(control_f[1]),
                                    q16(control_f[2])};
        i32 control_lab[3];
        xyzToOklabQ16(control_xyz, control_lab);
        const float cx = toFloat(control_lab[1]), cy = toFloat(control_lab[2]);
        const float control_scale =
            fl::sqrtf((ax * ax + ay * ay) * (cx * cx + cy * cy));

        // Required, not conditional. Skipping the hue check when the mapped
        // result is neutral would let a regression that collapses every
        // colour to grey pass silently -- and every target in this list is
        // saturated enough that the mapper must return chroma.
        FL_REQUIRE_GT(scale, 1e-4f);
        FL_REQUIRE_GT(bx * bx + by * by, 1e-6f);
        // The mapper's drift, and the measurement's own, on the same scale.
        // The second bound is what makes the first meaningful.
        FL_CHECK_LT(cross / scale, 0.005f);
        FL_REQUIRE_GT(control_scale, 1e-6f);
        FL_CHECK_LT(fl::fabsf(ax * cy - ay * cx) / control_scale, 0.001f);
        // And chroma must not have grown.
        FL_CHECK_LE(bx * bx + by * by, (ax * ax + ay * ay) * 1.02f + 1e-6f);
    }
}

FL_TEST_CASE("Gamut map clamps an over-bright neutral to the attainable one") {
    // Chroma compression alone cannot rescue a target that is too bright: at
    // zero chroma it is still outside the hull. The lightness bound handles
    // that, and it is a device constant rather than a search.
    //
    // The target is a D65 neutral, which is the ray the bound is defined on.
    // Note that "all drives near full" would be the wrong expectation: this
    // device's emitters are normalized to unit luminance each, so reaching
    // D65 needs drives in roughly 0.21 : 0.72 : 0.07. What must hold is that
    // the brightest emitter saturates and the neutral stays neutral.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    for (float luminance : {2.0f, 5.0f, 12.0f, 30.0f}) {
        i32 xyz[3];
        xyzAt(0.3127f, 0.3290f, luminance, xyz);
        i32 drives[3];
        mapAndSolveDrivesQ16(map, xyz, drives);

        i32 largest = drives[0];
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_GE(drives[i], 0);
            FL_CHECK_LE(drives[i], kFullDrive);
            if (drives[i] > largest) {
                largest = drives[i];
            }
        }
        // The device is producing the brightest neutral it can, so one
        // emitter must be at full scale. If the bound were bisected from
        // zero with too few iterations, this is what would sag.
        FL_CHECK_GT(largest, kFullDrive - 1024);

        // And it must still be that neutral: the drives keep the ratios the
        // unclamped solve would have given, since scaling a neutral does not
        // change its chromaticity.
        i32 unclamped[3];
        solveRgbDrivesQ16(map.solve, xyz, unclamped);
        const float ratio = toFloat(drives[1]) / toFloat(unclamped[1]);
        for (int i = 0; i < 3; ++i) {
            const float got = toFloat(drives[i]);
            const float want = toFloat(unclamped[i]) * ratio;
            FL_CHECK_LT(fl::fabsf(got - want), 0.01f);
        }
    }
}

FL_TEST_CASE("The lightness bound is the brightest neutral, not a guess") {
    // Pins the closed form the header claims: the bound must equal the OKLab
    // lightness of D65 scaled until its largest drive is exactly full scale.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    const i32 d65[3] = {62289, 65536, 71372};
    i32 neutral_drives[3];
    solveRgbDrivesQ16(map.solve, d65, neutral_drives);
    i32 largest = neutral_drives[0];
    for (int i = 1; i < 3; ++i) {
        if (neutral_drives[i] > largest) {
            largest = neutral_drives[i];
        }
    }
    FL_REQUIRE_GT(largest, 0);
    const float scale = 65536.0f / static_cast<float>(largest);
    const i32 brightest[3] = {q16(toFloat(d65[0]) * scale),
                              q16(toFloat(d65[1]) * scale),
                              q16(toFloat(d65[2]) * scale)};
    i32 lab[3];
    xyzToOklabQ16(brightest, lab);
    FL_CHECK_LT(fl::fabsf(toFloat(map.max_neutral_lightness - lab[0])), 0.001f);

    // And that neutral really is on the boundary: reachable, but not if
    // pushed 2% brighter.
    i32 at_bound[3];
    solveRgbDrivesQ16(map.solve, brightest, at_bound);
    bool feasible = true;
    for (int i = 0; i < 3; ++i) {
        if (at_bound[i] < -64 || at_bound[i] > kFullDrive + 64) {
            feasible = false;
        }
    }
    FL_CHECK(feasible);
}

FL_TEST_CASE("Gamut map reproduces the model the study validated") {
    // The property tests above say the mapper is well-behaved; this says it
    // is the *right* mapper. The expected drives come from the end-to-end
    // integer model in ci/color_gamut_study.py -- the same model scored at
    // 0.153 dE2000 over this corpus in
    // docs/color-gamut-algorithm-selection.md -- run over the first ten
    // out-of-gamut vectors of ci/golden/color-reference-v1.json.
    //
    // Without this, the C++ could be a self-consistent mapper that simply
    // is not the one the study selected.
    struct Vector {
        i32 xyz[3];
        i32 drives[3];
    };
    const Vector kVectors[] = {
        {{     36,     15,      0}, {    20,     0,     0}},
        {{      8,     39,      2}, {     0,    47,     0}},
        {{     10,      3,     61}, {     0,     0,     5}},
        {{   2289,   2027,  10858}, {     1,  1628,   449}},
        {{  41744,  17216,      0}, { 18036,    11,   181}},
        {{   9478,  44433,   1840}, {    52, 39707,  1003}},
        {{  11068,   3886,  69533}, {     4,  3944,  1083}},
        {{  51221,  61650,   1840}, { 14498, 47059,     8}},
        {{  20545,  48320,  71372}, {    65, 41240,  3813}},
        {{  52811,  21103,  69533}, { 18113,     5,  4364}},
    };

    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));
    // The model's lightness bound, which the C++ derives independently.
    FL_CHECK_LT(fl::fabsf(toFloat(map.max_neutral_lightness - 73284)), 0.002f);

    for (const auto& v : kVectors) {
        i32 drives[3];
        mapAndSolveDrivesQ16(map, v.xyz, drives);
        for (int i = 0; i < 3; ++i) {
            // 256 raw units is one code at 8-bit output. The two sides
            // quantize the emitter matrix by different routes -- the model
            // inverts in float64 then rounds, the library goes through
            // colorimetric_response -- so bit-exactness is not the claim.
            FL_CHECK_LT(fl::fabsf(toFloat(drives[i] - v.drives[i])),
                        256.0f / 65536.0f);
        }
    }
}

FL_TEST_CASE("Gamut map rejects a profile that cannot make a neutral") {
    // The header promises this and the code did not deliver it: the check
    // was on the largest neutral drive alone, so a solve like {-x, y, z} --
    // primaries that do not enclose D65, so no neutral at any brightness --
    // passed. The lightness bound derived from it would be the lightness of
    // a colour the device cannot produce, leaving the mapper too permissive.
    //
    // Green pulled far toward yellow leaves D65 outside the triangle.
    EmitterProfile outside = rgbDevice();
    outside.xy_g[0] = 0.4800f;
    outside.xy_g[1] = 0.5100f;
    outside.xy_b[0] = 0.2600f;
    outside.xy_b[1] = 0.2400f;

    EmitterSolveMatrixQ16 solve;
    FL_REQUIRE(buildRgbSolveMatrixQ16(outside, &solve));
    const i32 d65[3] = {62289, 65536, 71372};
    i32 neutral[3];
    solveRgbDrivesQ16(solve, d65, neutral);
    bool any_negative = false;
    for (int i = 0; i < 3; ++i) {
        if (neutral[i] <= 0) {
            any_negative = true;
        }
    }
    // If this fails the fixture stopped being out-of-gamut and the test
    // below would pass for the wrong reason.
    FL_REQUIRE(any_negative);

    GamutMapQ16 map;
    FL_CHECK_FALSE(buildGamutMapQ16(outside, &map));
}

FL_TEST_CASE("Gamut map survives a profile bright enough to overflow the scale") {
    // The neutral scale is 2^32 / largest_neutral_drive. `EmitterProfile`
    // accepts luminances up to 1e6, and a profile that reaches D65 on a
    // drive of one or two raw units puts that at or past i32's range --
    // exactly 2^31 at largest == 2 -- where narrowing is
    // implementation-defined and the bound it produces is nonsense.
    // Luminances chosen so the D65 solve lands on drives of one and two raw
    // units -- the case that puts 2^32 / largest at exactly 2^31. A uniform
    // huge luminance does not reach it: every drive rounds to zero and the
    // neutral check rejects the profile first, which is how the first
    // version of this test managed to assert nothing at all.
    EmitterProfile blazing = rgbDevice();
    blazing.lum_r = 6966.4768f;
    blazing.lum_g = 23435.6736f;
    blazing.lum_b = 2365.8496f;

    // Pin that this fixture really does reach the overflow case, so the
    // test cannot go quiet if the solve or the constants move.
    EmitterSolveMatrixQ16 probe;
    FL_REQUIRE(buildRgbSolveMatrixQ16(blazing, &probe));
    const i32 d65[3] = {62289, 65536, 71372};
    i32 neutral[3];
    solveRgbDrivesQ16(probe, d65, neutral);
    i32 largest = 0;
    for (int i = 0; i < 3; ++i) {
        FL_REQUIRE_GT(neutral[i], 0);
        if (neutral[i] > largest) {
            largest = neutral[i];
        }
    }
    // 2^32 / 2 is 2^31 -- one past i32.
    FL_REQUIRE_LE(largest, 2);

    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(blazing, &map));
    // Whatever comes back must be a sane lightness, not a wrapped one.
    FL_CHECK_GT(map.max_neutral_lightness, 0);
    FL_CHECK_LE(map.max_neutral_lightness, kOklabQ16MaxMagnitude);

    // And the mapper must still return usable drives through it.
    i32 xyz[3];
    xyzAt(0.70f, 0.28f, 0.5f, xyz);
    i32 drives[3];
    mapAndSolveDrivesQ16(map, xyz, drives);
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_GE(drives[i], 0);
        FL_CHECK_LE(drives[i], kFullDrive);
    }
}

FL_TEST_CASE("Gamut map moves smoothly enough to animate") {
    // #4041's acceptance criteria ask that out-of-gamut mapping be
    // continuous. Strictly it is not: the halving search returns a quantized
    // scale factor, so crossing the gamut boundary steps rather than glides.
    // What matters is the size of the step against the output's own
    // quantization, so that is what this measures.
    //
    // Measured at the shipped eight halvings, worst summed change in the
    // three drives between adjacent samples:
    //
    //   path                     worst      as 8-bit codes   worst / mean
    //   ------------------------ ---------- ---------------- ------------
    //   neutral -> deep red      0.002914   0.74             1.3x
    //   boundary crossing (fine) 0.003128   0.80             42.8x
    //   hue sweep, out of gamut  0.003189   0.81             4.6x
    //   neutral luminance ramp   0.004028   1.03             1.2x
    //
    // The 42.8x on the boundary crossing is the discontinuity showing: the
    // worst step there is forty-odd times the typical one. It is still under
    // one 8-bit code, which is why it does not band.
    //
    // Raising the halving count shrinks it to a floor rather than to zero --
    // 0.30 codes at ten halvings, 0.25 at twelve, 0.23 at fourteen -- and the
    // worst step moves elsewhere on the path. So the search resolution is
    // part of it and not all of it; the residual is the entry check's slack
    // boundary and Q16 itself. Worth knowing before anyone spends halvings
    // trying to smooth this out.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    struct Path {
        float x0, y0, x1, y1;
        float luminance;  // zero means ramp it instead of the chromaticity
        int steps;
    };
    const Path kPaths[] = {
        {0.3127f, 0.3290f, 0.72f, 0.26f, 0.4f, 400},
        {0.55f, 0.33f, 0.62f, 0.30f, 0.4f, 4000},
        {0.70f, 0.28f, 0.10f, 0.70f, 0.4f, 2000},
        {0.3127f, 0.3290f, 0.3127f, 0.3290f, 0.0f, 400},
    };

    for (const auto& path : kPaths) {
        i32 previous[3] = {0, 0, 0};
        float worst = 0.0f;
        int mapped_samples = 0;
        for (int step = 0; step <= path.steps; ++step) {
            const float t = static_cast<float>(step) / path.steps;
            const float x = path.x0 + t * (path.x1 - path.x0);
            const float y = path.y0 + t * (path.y1 - path.y0);
            const float luminance =
                path.luminance > 0.0f ? path.luminance : (0.02f + t * 1.6f);
            i32 xyz[3];
            xyzAt(x, y, luminance, xyz);
            i32 plain[3];
            solveRgbDrivesQ16(map.solve, xyz, plain);
            for (int i = 0; i < 3; ++i) {
                if (plain[i] < 0 || plain[i] > kFullDrive) {
                    ++mapped_samples;
                    break;
                }
            }
            i32 drives[3];
            mapAndSolveDrivesQ16(map, xyz, drives);
            if (step > 0) {
                float jump = 0.0f;
                for (int i = 0; i < 3; ++i) {
                    jump += fl::fabsf(toFloat(drives[i] - previous[i]));
                }
                if (jump > worst) {
                    worst = jump;
                }
            }
            for (int i = 0; i < 3; ++i) {
                previous[i] = drives[i];
            }
        }
        // Every path has to actually cross into the mapped branch. A fixture
        // or solve change that left one entirely in gamut would leave this
        // measuring the plain solve's smoothness and saying nothing at all
        // about the mapper.
        FL_REQUIRE_GT(mapped_samples, 0);
        // 1.5 codes at 8-bit: about twice the worst measured, so platform
        // float differences do not make this flaky, while a mapper that
        // started stepping by a visible amount would fail.
        FL_CHECK_LT(worst, 1.5f / 255.0f);
    }
}

FL_TEST_CASE("Gamut map leaves an in-gamut ramp exactly where it found it") {
    // The other half of #4041's acceptance criteria: in-gamut vectors
    // preserve chromaticity, and neutral ramps stay neutral. A mapper that
    // compressed slightly even inside the hull would pass the continuity
    // check above while quietly desaturating everything.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    int exercised = 0;
    for (int step = 1; step <= 40; ++step) {
        const float luminance = static_cast<float>(step) / 40.0f;
        i32 xyz[3];
        xyzAt(0.3127f, 0.3290f, luminance, xyz);
        i32 plain[3];
        solveRgbDrivesQ16(map.solve, xyz, plain);
        bool in_range = true;
        for (int i = 0; i < 3; ++i) {
            if (plain[i] < 0 || plain[i] > kFullDrive) {
                in_range = false;
            }
        }
        if (!in_range) {
            continue;
        }
        ++exercised;
        i32 drives[3];
        mapAndSolveDrivesQ16(map, xyz, drives);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_EQ(drives[i], plain[i]);
        }
        // And the light those drives actually make is still D65. Comparing
        // drives against the plain solve, as above, cannot show this: the
        // two are equal by assertion, so any ratio between them is 1 by
        // construction. Pushing the drives back through the emitter columns
        // and checking the chromaticity is independent of the solve, and is
        // what would catch a wrong matrix that happened to be applied
        // consistently.
        float reproduced[3] = {0.0f, 0.0f, 0.0f};
        const float emitters[3][2] = {
            {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
        for (int e = 0; e < 3; ++e) {
            float column[3];
            colorimetric_response::xyY_to_XYZ(emitters[e][0], emitters[e][1], 1.0f,
                                              column);
            for (int i = 0; i < 3; ++i) {
                reproduced[i] += toFloat(drives[e]) * column[i];
            }
        }
        const float sum = reproduced[0] + reproduced[1] + reproduced[2];
        FL_REQUIRE_GT(sum, 1e-4f);
        FL_CHECK_LT(fl::fabsf(reproduced[0] / sum - 0.3127f), 0.002f);
        FL_CHECK_LT(fl::fabsf(reproduced[1] / sum - 0.3290f), 0.002f);
    }
    FL_CHECK_GT(exercised, 20);
}

FL_TEST_CASE("Gamut map rejects a degenerate profile") {
    EmitterProfile collinear = rgbDevice();
    collinear.xy_g[0] = 0.6400f;
    collinear.xy_g[1] = 0.3300f;
    GamutMapQ16 map;
    FL_CHECK_FALSE(buildGamutMapQ16(collinear, &map));
    FL_CHECK_FALSE(buildGamutMapQ16(rgbDevice(), nullptr));
}

}  // FL_TEST_FILE

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
        // Same direction, not merely the same line. An anti-parallel chroma
        // vector -- hue rotated 180 degrees -- makes the cross product
        // vanish exactly as a correct mapping does, so the drift check below
        // cannot see it on its own.
        FL_CHECK_GT(ax * bx + ay * by, 0.0f);
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

// The white emitter the corpus's `rgbw` device uses: D65 at unit luminance.
constexpr i32 kWhiteD65[3] = {62289, 65536, 71372};

FL_TEST_CASE("RGBW mapper leaves alone what the RGB hull wrongly rejects") {
    // The whole reason this variant exists. Over 200 000 targets drawn from
    // inside a four-emitter device's own zonotope, the RGB-only solve
    // rejects 43% of them -- every one of which the three-emitter mapper
    // would compress despite the device being able to produce it exactly.
    //
    // These five are from that measurement. Each is reachable, and each has
    // an RGB-only drive above full scale.
    const float kReachable[][3] = {
        {2.065f, 1.833f, 5.497f},
        {2.599f, 2.165f, 2.563f},
        {2.827f, 2.086f, 1.201f},
        {1.966f, 2.134f, 2.050f},
        {2.964f, 2.123f, 8.427f},
    };

    GamutMapRgbwQ16 rgbw;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                     WhiteAllocationPolicy::WhitePreferred, &rgbw));
    GamutMapQ16 rgb_only;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &rgb_only));

    for (const auto& sample : kReachable) {
        const i32 xyz[3] = {q16(sample[0]), q16(sample[1]),
                            q16(sample[2])};

        // Pin the premise: the RGB-only solve really does reject these, so
        // this test cannot go quiet if the fixture or the solve moves.
        i32 plain[3];
        solveRgbDrivesQ16(rgb_only.solve, xyz, plain);
        bool rgb_rejects = false;
        for (int i = 0; i < 3; ++i) {
            if (plain[i] < 0 || plain[i] > kFullDrive) {
                rgb_rejects = true;
            }
        }
        FL_REQUIRE(rgb_rejects);

        // The RGBW mapper must return them untouched -- the allocation
        // reproduces the target, so nothing is compressed.
        i32 drives[4];
        mapAndAllocateRgbwQ16(rgbw, xyz, drives);
        i32 direct[4];
        FL_REQUIRE(allocateEmitterDrivesQ16(rgbw.allocation, xyz, direct));
        for (int i = 0; i < 4; ++i) {
            FL_CHECK_EQ(drives[i], direct[i]);
            FL_CHECK_GE(drives[i], 0);
            FL_CHECK_LE(drives[i], kFullDrive);
        }
    }
}

FL_TEST_CASE("RGBW lightness bound is the brighter one the white emitter buys") {
    // A white emitter at D65 and unit luminance adds exactly one unit of
    // neutral, so the brightest attainable neutral goes from 1.398 to 2.398
    // times D65. Both are checked here, because a bound that silently stayed
    // at the three-emitter value would leave the mapper dimming RGBW
    // neutrals for no reason.
    GamutMapRgbwQ16 rgbw;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                     WhiteAllocationPolicy::WhitePreferred, &rgbw));
    GamutMapQ16 rgb_only;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &rgb_only));

    FL_CHECK_GT(rgbw.max_neutral_lightness, rgb_only.max_neutral_lightness);

    // Independently: the lightness of 2.398 x D65, computed here rather than
    // taken from the implementation.
    const i32 expected_neutral[3] = {
        q16(0.9504559f * 2.398271526f),
        q16(1.0f * 2.398271526f),
        q16(1.0890578f * 2.398271526f),
    };
    i32 expected_lab[3];
    xyzToOklabQ16(expected_neutral, expected_lab);
    FL_CHECK_LT(fl::fabsf(toFloat(rgbw.max_neutral_lightness - expected_lab[0])),
                0.002f);

    // And that neutral must actually be reachable, while 5% brighter is not.
    i32 drives[4];
    FL_CHECK(allocateEmitterDrivesQ16(rgbw.allocation, expected_neutral, drives));
    const i32 too_bright[3] = {
        static_cast<i32>(expected_neutral[0] * 1.05f),
        static_cast<i32>(expected_neutral[1] * 1.05f),
        static_cast<i32>(expected_neutral[2] * 1.05f),
    };
    FL_CHECK_FALSE(
        allocateEmitterDrivesQ16(rgbw.allocation, too_bright, drives));
}

FL_TEST_CASE("RGBW lightness bound stays reachable for a skewed white") {
    // Regression. The closed form min_i (1 + dW_i) / d0_i enforces only the
    // *upper* limits on the RGB drives. Full white can push a different
    // channel negative, and then the "brightest neutral" is not in the hull
    // at all -- which would leave the halving search with a zero-chroma
    // candidate it cannot satisfy and the fallback returning four zero
    // drives for a colour that is not black.
    //
    // This white emitter is built to do exactly that: its drives through the
    // RGB matrix are about (0.9, 0.05, 0.05) against a D65 neutral needing
    // (0.21, 0.72, 0.07), so the formula returns 1.468 while the red drive
    // there is 1.468 * 0.21 - 0.9 = -0.59.
    const i32 skewed_white[3] = {q16(1.8955f), q16(1.0f), q16(0.74847f)};

    GamutMapRgbwQ16 rgbw;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), skewed_white,
                                     WhiteAllocationPolicy::WhitePreferred, &rgbw));

    // Pin the premise: this fixture really is the awkward shape, so the test
    // cannot go quiet if the emitter columns or the solve move.
    FL_REQUIRE_GT(rgbw.allocation.per_white[0], rgbw.allocation.per_white[1]);
    FL_REQUIRE_GT(rgbw.allocation.per_white[0], kFullDrive / 2);

    // The stored bound must name a neutral the device can actually make.
    i32 brightest[3];
    {
        // Recover the neutral from the stored lightness by inverting OKLab.
        const i32 lab[3] = {rgbw.max_neutral_lightness, 0, 0};
        oklabToXyzQ16(lab, brightest);
    }
    i32 drives[4];
    FL_CHECK(allocateEmitterDrivesQ16(rgbw.allocation, brightest, drives));

    // And the mapper must not collapse a real colour to black through it.
    i32 xyz[3];
    xyzAt(0.45f, 0.40f, 3.0f, xyz);
    i32 mapped[4];
    mapAndAllocateRgbwQ16(rgbw, xyz, mapped);
    FL_CHECK_GT(mapped[0] + mapped[1] + mapped[2] + mapped[3], kFullDrive / 8);
    for (int i = 0; i < 4; ++i) {
        FL_CHECK_GE(mapped[i], 0);
        FL_CHECK_LE(mapped[i], kFullDrive);
    }
}

FL_TEST_CASE("RGBW mapper honours the allocation policy") {
    // Nothing else here exercises RgbPreferred through the mapper, so a
    // regression that dropped `policy` on the floor in buildGamutMapRgbwQ16
    // -- leaving every device white-preferred -- would pass the whole file.
    GamutMapRgbwQ16 white_first;
    GamutMapRgbwQ16 rgb_first;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                    WhiteAllocationPolicy::WhitePreferred,
                                    &white_first));
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                    WhiteAllocationPolicy::RgbPreferred,
                                    &rgb_first));

    // The policy must reach the stored allocation.
    FL_CHECK(white_first.allocation.policy ==
             WhiteAllocationPolicy::WhitePreferred);
    FL_CHECK(rgb_first.allocation.policy == WhiteAllocationPolicy::RgbPreferred);

    // It changes the drives, never which targets are reachable, so the
    // lightness bound has to come out the same either way.
    FL_CHECK_EQ(white_first.max_neutral_lightness,
                rgb_first.max_neutral_lightness);

    int differed = 0;
    for (int step = 1; step <= 10; ++step) {
        i32 xyz[3];
        xyzAt(0.3127f, 0.3290f, static_cast<float>(step) / 10.0f, xyz);

        i32 with_white[4];
        i32 with_rgb[4];
        mapAndAllocateRgbwQ16(white_first, xyz, with_white);
        mapAndAllocateRgbwQ16(rgb_first, xyz, with_rgb);

        FL_CHECK_LE(with_rgb[3], with_white[3]);
        if (with_rgb[3] < with_white[3]) {
            ++differed;
        }
        for (int i = 0; i < 4; ++i) {
            FL_CHECK_GE(with_rgb[i], 0);
            FL_CHECK_LE(with_rgb[i], kFullDrive);
        }
    }
    // Guard against the two never diverging, which would make the checks
    // above pass whatever the mapper did with the policy.
    FL_CHECK_GT(differed, 5);
}

FL_TEST_CASE("RGBW mapper always returns drives inside [0, 1]") {
    GamutMapRgbwQ16 rgbw;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                     WhiteAllocationPolicy::WhitePreferred, &rgbw));

    int exercised = 0;
    for (int hue = 0; hue < 36; ++hue) {
        const float angle = static_cast<float>(hue) * 10.0f * 3.14159265f / 180.0f;
        const float x = 0.33f + 0.45f * fl::cosf(angle);
        const float y = 0.33f + 0.45f * fl::sinf(angle);
        if (x <= 0.01f || y <= 0.01f || x + y >= 0.99f) {
            continue;
        }
        for (float luminance : {0.2f, 1.0f, 4.0f}) {
            i32 xyz[3];
            xyzAt(x, y, luminance, xyz);
            i32 probe[4];
            if (allocateEmitterDrivesQ16(rgbw.allocation, xyz, probe)) {
                continue;  // nothing for the mapper to do
            }
            ++exercised;
            i32 drives[4];
            mapAndAllocateRgbwQ16(rgbw, xyz, drives);
            for (int i = 0; i < 4; ++i) {
                FL_CHECK_GE(drives[i], 0);
                FL_CHECK_LE(drives[i], kFullDrive);
            }
        }
    }
    FL_CHECK_GT(exercised, 12);
}

FL_TEST_CASE("Gamut map rejects a degenerate profile") {
    EmitterProfile collinear = rgbDevice();
    collinear.xy_g[0] = 0.6400f;
    collinear.xy_g[1] = 0.3300f;
    GamutMapQ16 map;
    FL_CHECK_FALSE(buildGamutMapQ16(collinear, &map));
    FL_CHECK_FALSE(buildGamutMapQ16(rgbDevice(), nullptr));
}


// ---------------------------------------------------------------------------
// Two white emitters (P7, #4041)
// ---------------------------------------------------------------------------

namespace {

/// The corpus's warm white: D50 at unit luminance, in s16.16.
constexpr i32 kWhiteD50Map[3] = {63196, 65536, 54074};

/// XYZ of a five-emitter drive vector on the cool/warm device.
void reproduceRgbww(const float (&drives)[5], float (&out)[3]) {
    const EmitterProfile profile = rgbDevice();
    float columns[5][3];
    colorimetric_response::xyY_to_XYZ(profile.xy_r[0], profile.xy_r[1],
                                      profile.lum_r, columns[0]);
    colorimetric_response::xyY_to_XYZ(profile.xy_g[0], profile.xy_g[1],
                                      profile.lum_g, columns[1]);
    colorimetric_response::xyY_to_XYZ(profile.xy_b[0], profile.xy_b[1],
                                      profile.lum_b, columns[2]);
    for (int i = 0; i < 3; ++i) {
        columns[3][i] = toFloat(kWhiteD65[i]);
        columns[4][i] = toFloat(kWhiteD50Map[i]);
    }
    for (int i = 0; i < 3; ++i) {
        out[i] = 0.0f;
        for (int e = 0; e < 5; ++e) {
            out[i] += columns[e][i] * drives[e];
        }
    }
}

}  // namespace

FL_TEST_CASE("RGBWW mapper leaves alone what the one-white hull wrongly rejects") {
    // The reason this variant exists, and the same argument the RGBW variant
    // makes one emitter down: testing a two-white device against the
    // one-white hull under-reports what it can produce, and every
    // under-reported target gets compressed despite being reachable exactly.
    //
    // Measured here rather than asserted: targets are drawn from inside the
    // device's own five-emitter zonotope, so every one is reachable by
    // construction, and the count is how many the one-white allocation
    // refuses.
    GamutMapRgbwwQ16 rgbww;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &rgbww));
    WhiteAllocationQ16 one_white;
    FL_REQUIRE(buildWhiteAllocationQ16(rgbDevice(), kWhiteD65,
                                       WhiteAllocationPolicy::WhitePreferred,
                                       &one_white));

    int reachable = 0;
    int refused_by_one_white = 0;
    // A deterministic sweep of the zonotope rather than a random one, so the
    // number this reports is the same on every machine.
    for (int r = 0; r <= 3; ++r) {
        for (int g = 0; g <= 3; ++g) {
            for (int b = 0; b <= 3; ++b) {
                for (int w1 = 0; w1 <= 3; ++w1) {
                    for (int w2 = 0; w2 <= 3; ++w2) {
                        const float sample[5] = {
                            static_cast<float>(r) / 3.0f,
                            static_cast<float>(g) / 3.0f,
                            static_cast<float>(b) / 3.0f,
                            static_cast<float>(w1) / 3.0f,
                            static_cast<float>(w2) / 3.0f};
                        float xyz_f[3];
                        reproduceRgbww(sample, xyz_f);
                        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]),
                                            q16(xyz_f[2])};

                        i32 five[5];
                        FL_REQUIRE(allocateTwoWhiteDrivesQ16(rgbww.allocation,
                                                             xyz, five));
                        ++reachable;

                        i32 four[4];
                        if (!allocateEmitterDrivesQ16(one_white, xyz, four)) {
                            ++refused_by_one_white;
                        }
                    }
                }
            }
        }
    }

    FL_CHECK_EQ(reachable, 1024);
    // Not a rare corner. If this ever drops to zero the test has stopped
    // measuring anything and the variant has stopped being justified.
    FL_CHECK_GT(refused_by_one_white, 100);
}

FL_TEST_CASE("RGBWW mapper returns an in-gamut target untouched") {
    GamutMapRgbwwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));

    const float samples[][5] = {
        {0.20f, 0.40f, 0.60f, 0.30f, 0.10f},
        {0.10f, 0.90f, 0.20f, 0.70f, 0.60f},
        {0.05f, 0.05f, 0.05f, 0.90f, 0.90f},
    };
    for (const auto& sample : samples) {
        float xyz_f[3];
        reproduceRgbww(sample, xyz_f);
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};

        i32 drives[5];
        mapAndAllocateRgbwwQ16(map, xyz, drives);

        float as_float[5];
        for (int i = 0; i < 5; ++i) {
            as_float[i] = toFloat(drives[i]);
        }
        float reproduced[3];
        reproduceRgbww(as_float, reproduced);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_LT(fl::fabsf(reproduced[i] - xyz_f[i]), 0.01f);
        }
    }
}

FL_TEST_CASE("RGBWW mapper always returns drives inside [0, 1]") {
    // Including for targets far outside the hull, which is the case the
    // halving search and its fallback exist for.
    GamutMapRgbwwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));

    int exercised = 0;
    for (int step = 0; step <= 20; ++step) {
        const float t = static_cast<float>(step) / 20.0f;
        // A saturated ramp pushed well past anything the device can make.
        const float xyz_f[3] = {6.0f * t + 0.05f, 1.5f * t + 0.02f,
                                9.0f * (1.0f - t) + 0.05f};
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};

        // Count only the targets that actually reach the halving search.
        // Incrementing on every iteration would have made the assertion
        // below restate the loop bound: it could not fail, and if the hull
        // ever grew to contain all of these the compression path would go
        // untested in silence.
        i32 probe[5];
        const bool needs_compression =
            !allocateTwoWhiteDrivesQ16(map.allocation, xyz, probe);

        i32 drives[5];
        mapAndAllocateRgbwwQ16(map, xyz, drives);
        for (int i = 0; i < 5; ++i) {
            FL_CHECK_GE(drives[i], 0);
            FL_CHECK_LE(drives[i], kFullDrive);
        }
        if (needs_compression) {
            ++exercised;
        }
    }
    // Most of this ramp is outside the hull; the assertion is that it is
    // genuinely being compressed, not that the loop ran.
    FL_CHECK_GT(exercised, 10);
}

FL_TEST_CASE("RGBWW mapper moves smoothly enough to animate") {
    // P7's continuity criterion, for the two-white hull. A ramp that crosses
    // out of the gamut must not jump: neighbouring inputs one step apart
    // have to land within a bounded distance of each other, or an animation
    // walking through the boundary shows a visible seam.
    GamutMapRgbwwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));

    const int kSteps = 240;
    float previous[3] = {0.0f, 0.0f, 0.0f};
    float worst_jump = 0.0f;
    for (int step = 0; step <= kSteps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(kSteps);
        // Sweeps from deep inside the hull to well outside it.
        const float xyz_f[3] = {0.2f + 4.0f * t, 0.3f + 3.0f * t,
                                0.4f + 5.0f * t};
        const i32 xyz[3] = {q16(xyz_f[0]), q16(xyz_f[1]), q16(xyz_f[2])};
        i32 drives[5];
        mapAndAllocateRgbwwQ16(map, xyz, drives);

        float as_float[5];
        for (int i = 0; i < 5; ++i) {
            as_float[i] = toFloat(drives[i]);
        }
        float produced[3];
        reproduceRgbww(as_float, produced);

        if (step > 0) {
            for (int i = 0; i < 3; ++i) {
                const float jump = fl::fabsf(produced[i] - previous[i]);
                if (jump > worst_jump) {
                    worst_jump = jump;
                }
            }
        }
        for (int i = 0; i < 3; ++i) {
            previous[i] = produced[i];
        }
    }
    // The input moves about 0.05 per step in Y. A mapper that stepped
    // discontinuously at the hull boundary would show a jump many times
    // that; this bounds it at one input step's worth.
    FL_CHECK_LT(worst_jump, 0.05f);
}

FL_TEST_CASE("RGBWW lightness bound is the brightest neutral it can reach") {
    // Two whites buy more neutral headroom than one, and the bound has to
    // find it -- and still be attainable, which the bisection is for.
    GamutMapRgbwwQ16 two;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &two));
    GamutMapRgbwQ16 one;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                    WhiteAllocationPolicy::WhitePreferred,
                                    &one));
    FL_CHECK_GT(two.max_neutral_lightness, one.max_neutral_lightness);

    // Attainable, not merely optimistic: the neutral at the stored bound
    // must actually allocate.
    const i32 neutral_lab[3] = {two.max_neutral_lightness, 0, 0};
    i32 neutral_xyz[3];
    oklabToXyzQ16(neutral_lab, neutral_xyz);
    i32 drives[5];
    FL_CHECK(allocateTwoWhiteDrivesQ16(two.allocation, neutral_xyz, drives));

    // And a hair above it is not, so the bound is tight rather than timid.
    const i32 above_lab[3] = {two.max_neutral_lightness + 2000, 0, 0};
    i32 above_xyz[3];
    oklabToXyzQ16(above_lab, above_xyz);
    i32 above_drives[5];
    FL_CHECK_FALSE(allocateTwoWhiteDrivesQ16(two.allocation, above_xyz,
                                             above_drives));
}

FL_TEST_CASE("RGBWW mapper rejects a degenerate profile") {
    EmitterProfile broken = rgbDevice();
    broken.xy_g[0] = broken.xy_r[0];
    broken.xy_g[1] = broken.xy_r[1];
    GamutMapRgbwwQ16 map;
    FL_CHECK_FALSE(buildGamutMapRgbwwQ16(broken, kWhiteD65, kWhiteD50Map,
                                         WhiteAllocationPolicy::WhitePreferred,
                                         &map));
}


FL_TEST_CASE("White-emitter builds survive a profile bright enough to overflow") {
    // The three-emitter build clamps `2^32 / largest_neutral_drive` before
    // narrowing it, because `EmitterProfile` accepts luminances up to 1e6 and
    // a profile reaching D65 on one or two raw units of drive puts that at
    // 2^31. Both white builds compute the same quantity, and neither clamped
    // it: `optimistic` starts at `kOklabQ16MaxMagnitude`, so for such a
    // profile the bisection is skipped and the unclamped value is narrowed
    // directly -- implementation-defined, and the stored bound meaningless.
    //
    // Same fixture as the three-emitter case, for the same reason: a uniform
    // huge luminance does not reach the overflow, because every drive rounds
    // to zero and the neutral check rejects the profile first.
    EmitterProfile blazing = rgbDevice();
    blazing.lum_r = 6966.4768f;
    blazing.lum_g = 23435.6736f;
    blazing.lum_b = 2365.8496f;

    // Pin that the fixture really does reach the overflow case.
    EmitterSolveMatrixQ16 probe;
    FL_REQUIRE(buildRgbSolveMatrixQ16(blazing, &probe));
    i32 neutral[3];
    const i32 d65[3] = {62289, 65536, 71372};
    solveRgbDrivesQ16(probe, d65, neutral);
    i32 largest = 0;
    for (int i = 0; i < 3; ++i) {
        FL_REQUIRE_GT(neutral[i], 0);
        if (neutral[i] > largest) {
            largest = neutral[i];
        }
    }
    FL_REQUIRE_LE(largest, 2);

    // Both bound checks are conditional, because a build is allowed to
    // reject this profile outright. Counting them is what stops the test
    // passing while asserting nothing at all: if both builds ever started
    // refusing the fixture, the clamp regression coverage would vanish in
    // silence -- the same vacuity the `exercised` counter above had.
    int built = 0;

    GamutMapRgbwQ16 one;
    if (buildGamutMapRgbwQ16(blazing, kWhiteD65,
                             WhiteAllocationPolicy::WhitePreferred, &one)) {
        ++built;
        // A sane, positive bound rather than a narrowed 2^31.
        FL_CHECK_GT(one.max_neutral_lightness, 0);
        FL_CHECK_LE(one.max_neutral_lightness, kOklabQ16MaxMagnitude);
    }

    GamutMapRgbwwQ16 two;
    if (buildGamutMapRgbwwQ16(blazing, kWhiteD65, kWhiteD50Map,
                              WhiteAllocationPolicy::WhitePreferred, &two)) {
        ++built;
        FL_CHECK_GT(two.max_neutral_lightness, 0);
        FL_CHECK_LE(two.max_neutral_lightness, kOklabQ16MaxMagnitude);
    }

    FL_CHECK_GT(built, 0);
}


// ---------------------------------------------------------------------------
// P7's acceptance criterion, on the white-emitter paths (#4041)
// ---------------------------------------------------------------------------
//
// "In-gamut vectors preserve chromaticity; neutral ramps remain neutral;
// out-of-gamut mapping is continuous and meets the hue/luminance objective."
//
// The three-emitter path has carried all four of those since it was written.
// The white paths -- which are what this phase is actually *for* -- had only
// some of them: RGBW had no continuity, hue or neutral-ramp coverage at all,
// and RGBWW had no neutral-ramp or hue coverage. These close that.

namespace {

/// XYZ of four drives on the `rgbDevice()` + D65 white device.
void reproduceRgbw(const float (&drives)[4], float (&out)[3]) {
    const EmitterProfile profile = rgbDevice();
    float columns[4][3];
    colorimetric_response::xyY_to_XYZ(profile.xy_r[0], profile.xy_r[1],
                                      profile.lum_r, columns[0]);
    colorimetric_response::xyY_to_XYZ(profile.xy_g[0], profile.xy_g[1],
                                      profile.lum_g, columns[1]);
    colorimetric_response::xyY_to_XYZ(profile.xy_b[0], profile.xy_b[1],
                                      profile.lum_b, columns[2]);
    for (int i = 0; i < 3; ++i) {
        columns[3][i] = toFloat(kWhiteD65[i]);
    }
    for (int i = 0; i < 3; ++i) {
        out[i] = 0.0f;
        for (int e = 0; e < 4; ++e) {
            out[i] += columns[e][i] * drives[e];
        }
    }
}

/// How far the mapped chroma has left the target's hue *ray*: 0 on it, 1 for
/// anything the caller should treat as a failure.
///
/// The obvious form -- |cross(a, b)| / (|a| * |b|) -- is not enough on its
/// own, and reports perfect agreement for two things that are catastrophic:
/// a mapped chroma of zero (every colour collapsed to grey) and an
/// anti-parallel one (hue rotated 180 degrees). Both make the cross product
/// vanish. So those return 1 rather than 0, which is far outside any
/// tolerance a caller would set, and the metric means what its callers
/// assert about it.
float hueDivergence(const i32 (&lab_a)[3], const i32 (&lab_b)[3]) {
    const float ax = toFloat(lab_a[1]);
    const float ay = toFloat(lab_a[2]);
    const float bx = toFloat(lab_b[1]);
    const float by = toFloat(lab_b[2]);
    const float target_chroma = ax * ax + ay * ay;
    const float mapped_chroma = bx * bx + by * by;
    // Nothing to preserve, or nothing left of it.
    if (target_chroma < 1e-6f || mapped_chroma < 1e-6f) {
        return 1.0f;
    }
    // Opposite direction: the cross product is zero here too.
    if (ax * bx + ay * by <= 0.0f) {
        return 1.0f;
    }
    const float cross = fl::fabsf(ax * by - ay * bx);
    return cross / fl::sqrtf(target_chroma * mapped_chroma);
}

/// Whether the mapped chroma is no larger than the target's, within the
/// slack the s16.16 round trip needs. Compression is the objective;
/// expansion would be a different colour, not a mapped one.
bool chromaDidNotGrow(const i32 (&lab_a)[3], const i32 (&lab_b)[3]) {
    const float ax = toFloat(lab_a[1]);
    const float ay = toFloat(lab_a[2]);
    const float bx = toFloat(lab_b[1]);
    const float by = toFloat(lab_b[2]);
    return (bx * bx + by * by) <= (ax * ax + ay * ay) * 1.02f + 1e-6f;
}

}  // namespace

FL_TEST_CASE("RGBW mapper leaves a neutral ramp neutral") {
    // A mapper that compressed slightly even inside the hull would pass the
    // continuity check while quietly desaturating everything. The white
    // emitter makes this the interesting case: on a neutral the allocation
    // hands almost the whole ramp to it.
    GamutMapRgbwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                    WhiteAllocationPolicy::WhitePreferred,
                                    &map));

    int exercised = 0;
    for (int step = 1; step <= 40; ++step) {
        const float luminance = static_cast<float>(step) / 40.0f;
        i32 xyz[3];
        xyzAt(0.3127f, 0.3290f, luminance, xyz);

        i32 plain[4];
        if (!allocateEmitterDrivesQ16(map.allocation, xyz, plain)) {
            continue;  // Above the hull; the bound test covers that end.
        }
        ++exercised;

        i32 drives[4];
        mapAndAllocateRgbwQ16(map, xyz, drives);
        for (int i = 0; i < 4; ++i) {
            FL_CHECK_EQ(drives[i], plain[i]);
        }

        // And the light those drives make is still D65. Comparing drives
        // against the plain allocation cannot show that -- they are equal by
        // assertion -- so this reconstructs the chromaticity.
        float as_float[4];
        for (int i = 0; i < 4; ++i) {
            as_float[i] = toFloat(drives[i]);
        }
        float produced[3];
        reproduceRgbw(as_float, produced);
        const float sum = produced[0] + produced[1] + produced[2];
        FL_REQUIRE_GT(sum, 1e-4f);
        FL_CHECK_LT(fl::fabsf(produced[0] / sum - 0.3127f), 0.002f);
        FL_CHECK_LT(fl::fabsf(produced[1] / sum - 0.3290f), 0.002f);
    }
    FL_CHECK_GT(exercised, 20);
}

FL_TEST_CASE("RGBWW mapper leaves a neutral ramp neutral") {
    // Same claim on the five-emitter hull, where a neutral is split across
    // two whites and the primaries at once -- more ways to drift.
    GamutMapRgbwwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));

    int exercised = 0;
    for (int step = 1; step <= 40; ++step) {
        const float luminance = static_cast<float>(step) / 40.0f;
        i32 xyz[3];
        xyzAt(0.3127f, 0.3290f, luminance, xyz);

        i32 plain[5];
        if (!allocateTwoWhiteDrivesQ16(map.allocation, xyz, plain)) {
            continue;
        }
        ++exercised;

        i32 drives[5];
        mapAndAllocateRgbwwQ16(map, xyz, drives);
        for (int i = 0; i < 5; ++i) {
            FL_CHECK_EQ(drives[i], plain[i]);
        }

        float as_float[5];
        for (int i = 0; i < 5; ++i) {
            as_float[i] = toFloat(drives[i]);
        }
        float produced[3];
        reproduceRgbww(as_float, produced);
        const float sum = produced[0] + produced[1] + produced[2];
        FL_REQUIRE_GT(sum, 1e-4f);
        FL_CHECK_LT(fl::fabsf(produced[0] / sum - 0.3127f), 0.002f);
        FL_CHECK_LT(fl::fabsf(produced[1] / sum - 0.3290f), 0.002f);
    }
    FL_CHECK_GT(exercised, 20);
}

FL_TEST_CASE("RGBW mapper preserves hue while compressing chroma") {
    // The hue objective on the four-emitter hull, which was the one corner
    // of P7's criterion still covered on only one path.
    //
    // RGBW's hue was checked above the neutral cap ("RGBW keeps the
    // lightness above its own, higher cap"), where the walk-down chooses the
    // lightness. This is the other way out of the hull: a target inside the
    // cap whose *chroma* the four emitters cannot reach, so the halving
    // search compresses along the hue ray and nothing else may move.
    //
    // Those are different code paths -- one goes through
    // `highestReachableLightness`, this one does not -- so covering one said
    // nothing about the other.
    GamutMapRgbwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));

    const float kChroma[][2] = {
        {0.70f, 0.28f}, {0.18f, 0.72f}, {0.10f, 0.03f},
        {0.55f, 0.42f}, {0.08f, 0.55f},
    };
    int compressed = 0;
    for (const auto& c : kChroma) {
        i32 xyz[3];
        xyzAt(c[0], c[1], 0.5f, xyz);

        i32 probe[4];
        const bool in_gamut = allocateEmitterDrivesQ16(map.allocation, xyz, probe);

        i32 drives[4];
        mapAndAllocateRgbwQ16(map, xyz, drives);
        float as_float[4];
        for (int i = 0; i < 4; ++i) {
            as_float[i] = toFloat(drives[i]);
        }
        float produced[3];
        reproduceRgbw(as_float, produced);
        const i32 mapped_xyz[3] = {q16(produced[0]), q16(produced[1]),
                                   q16(produced[2])};

        i32 target_lab[3];
        i32 mapped_lab[3];
        xyzToOklabQ16(xyz, target_lab);
        xyzToOklabQ16(mapped_xyz, mapped_lab);
        // One bound for all five now. It used to be split, because
        // (0.55, 0.42) diverged by 0.026 -- an order of magnitude above its
        // four neighbours -- and that outlier had its own bracket with a
        // lower bound, so it could not vanish unremarked.
        //
        // It vanished, and this is the remark. #4304 traced it to
        // `allocateEmitterDrivesQ16` clamping a blue drive the solve had put
        // at -62, inside a slack sized in drive space; scaling that slack by
        // each emitter's XYZ column (FastLED#4303) removed it. Measured
        // across the five, worst divergence is now 0.0000.
        const float divergence = hueDivergence(target_lab, mapped_lab);
        FL_CHECK_LT(divergence, 0.005f);
        FL_CHECK(chromaDidNotGrow(target_lab, mapped_lab));
        if (!in_gamut) {
            ++compressed;
        }
    }
    // Vacuity guard, and not a formality: if every target here were already
    // inside the four-emitter hull the mapper would return them untouched
    // and the hue check would be asserting that a copy equals itself. The
    // white emitter makes this hull larger than the RGB one, so a target set
    // chosen for the three-emitter case can quietly stop compressing here.
    //
    // Measured: all five are outside it, so all five are mapped.
    FL_CHECK_EQ(compressed, 5);

    // What is *not* established, stated rather than left to be assumed.
    //
    // `chromaCandidateXyz` scales `a` and `b` by one factor, which preserves
    // hue exactly, so the mapper's own candidate is on the ray by
    // construction, and the divergence arises after it. Which of the
    // allocation or the quantisation produced it was open when this case was
    // written; the next case answers it -- the allocation, and by clamping a
    // drive the solve asked to be negative.
    //
    // The outlier is not a low-chroma artefact, which was the first
    // explanation worth ruling out: its mapped chroma is 0.198 against a
    // target 0.210, so the hue angle is well determined.
}

FL_TEST_CASE("a drive the solve puts out of range is refused, not clamped") {
    // This case used to record the opposite. It measured an in-gamut target
    // at (0.55, 0.41) whose blue drive the solve put at -62 raw, showed that
    // `allocateEmitterDrivesQ16` accepted it as reachable and clamped it to
    // zero, and priced the result at 0.026 of OKLab hue and 0.0158 of XYZ --
    // 26% of the target's own Z, because this device's blue column carries a
    // Z of 13.17 and 62 drive units of it is 0.0125.
    //
    // FastLED#4303 fixed the cause: the allowance was sized in drive space
    // and paid in colour, so it is scaled by each emitter's column now. Blue
    // gets 5 raw units where green gets 64. -62 is outside that, so the
    // target is refused and the gamut mapper compresses it instead of the
    // allocator answering it wrongly.
    //
    // The case is kept rather than deleted because the mechanism is what
    // must not come back: a drive the solve put well out of range being
    // clamped into range and reported as reachable.
    GamutMapRgbwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));

    i32 xyz[3];
    xyzAt(0.55f, 0.41f, 0.5f, xyz);

    // Still what the solve asks for -- the fix changed the verdict, not the
    // arithmetic. Derived rather than written down.
    i32 at_zero[3];
    solveRgbDrivesQ16(map.allocation.rgb_solve, xyz, at_zero);
    FL_CHECK_LT(at_zero[2], 0);
    FL_CHECK_GT(at_zero[2], -64);

    // Outside blue's allowance, so refused.
    FL_CHECK_LT(map.allocation.slack[2], -at_zero[2]);
    i32 allocated[4];
    FL_CHECK(!allocateEmitterDrivesQ16(map.allocation, xyz, allocated));

    // And green's is unchanged, so this is a rescaling and not a blanket
    // tightening -- a uniform slack of 5 would refuse targets nothing is
    // wrong with.
    FL_CHECK_EQ(map.allocation.slack[1], 64);
    FL_CHECK_GT(map.allocation.slack[0], map.allocation.slack[2]);

    // The mapper still answers the target, in gamut, with the hue intact.
    i32 drives[4];
    mapAndAllocateRgbwQ16(map, xyz, drives);
    float as_float[4];
    for (int i = 0; i < 4; ++i) {
        as_float[i] = toFloat(drives[i]);
    }
    float produced[3];
    reproduceRgbw(as_float, produced);
    const i32 produced_q16[3] = {q16(produced[0]), q16(produced[1]),
                                 q16(produced[2])};
    i32 target_lab[3];
    i32 produced_lab[3];
    xyzToOklabQ16(xyz, target_lab);
    xyzToOklabQ16(produced_q16, produced_lab);
    // Measured 0.0000, against the 0.026 this case was written to record.
    FL_CHECK_LT(hueDivergence(target_lab, produced_lab), 0.005f);
}

FL_TEST_CASE("the allocator is accurate across the plane, not just at one point") {
    // Extent. This measured the defect when there was one -- worst 0.0197 of
    // OKLab hue at (0.56, 0.40), with 6 of 291 in-gamut targets above 0.005
    // -- and measures its absence now.
    //
    // Every in-gamut target on a coarse grid of the xy plane at Y = 0.5.
    GamutMapRgbwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));

    int in_gamut = 0;
    int above_005 = 0;
    int above_010 = 0;
    float worst = 0.0f;
    for (int xi = 4; xi <= 72; xi += 2) {
        for (int yi = 4; yi <= 72; yi += 2) {
            const float x = static_cast<float>(xi) * 0.01f;
            const float y = static_cast<float>(yi) * 0.01f;
            if (x + y >= 1.0f) {
                continue;
            }
            i32 xyz[3];
            xyzAt(x, y, 0.5f, xyz);
            i32 drives[4];
            if (!allocateEmitterDrivesQ16(map.allocation, xyz, drives)) {
                continue;
            }
            ++in_gamut;

            float as_float[4];
            for (int i = 0; i < 4; ++i) {
                as_float[i] = toFloat(drives[i]);
            }
            float produced[3];
            reproduceRgbw(as_float, produced);
            const i32 produced_q16[3] = {q16(produced[0]), q16(produced[1]),
                                         q16(produced[2])};
            i32 target_lab[3];
            i32 produced_lab[3];
            xyzToOklabQ16(xyz, target_lab);
            xyzToOklabQ16(produced_q16, produced_lab);
            const float divergence = hueDivergence(target_lab, produced_lab);
            if (divergence > worst) {
                worst = divergence;
            }
            if (divergence > 0.005f) {
                ++above_005;
            }
            if (divergence > 0.010f) {
                ++above_010;
            }
        }
    }

    // Vacuity guard: a grid that found nothing in gamut would pass every
    // bound below by measuring an empty set.
    //
    // 283, down from 291 before FastLED#4303. Those 8 are the targets whose
    // blue drive falls outside the rescaled allowance; they are not lost,
    // the gamut mapper compresses them into the hull instead of the
    // allocator answering them wrongly. Pinned so the cost of the fix is
    // visible and cannot grow unnoticed.
    FL_CHECK_EQ(in_gamut, 283);

    // Measured: worst 0.0034, and nothing above 0.005 at all -- against
    // 0.0197 with 6 above 0.005 and 2 above 0.010 before the fix. The
    // orange-yellow region that used to be wrong is not wrong any more.
    FL_CHECK_LT(worst, 0.005f);
    FL_CHECK_EQ(above_005, 0);
    FL_CHECK_EQ(above_010, 0);
    // Bounded below, because a grid measuring nothing would also report
    // zero: some divergence is expected from Q16 rounding alone.
    FL_CHECK_GT(worst, 0.0f);
}

FL_TEST_CASE("RGBWW mapper preserves hue while compressing chroma") {
    // The last corner of the criterion: the hue objective on the five-emitter
    // hull.
    GamutMapRgbwwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));

    const float kChroma[][2] = {
        {0.70f, 0.28f}, {0.18f, 0.72f}, {0.10f, 0.03f},
        {0.55f, 0.42f}, {0.08f, 0.55f},
    };
    int compressed = 0;
    for (const auto& c : kChroma) {
        i32 xyz[3];
        xyzAt(c[0], c[1], 0.5f, xyz);

        i32 probe[5];
        const bool in_gamut = allocateTwoWhiteDrivesQ16(map.allocation, xyz, probe);

        i32 drives[5];
        mapAndAllocateRgbwwQ16(map, xyz, drives);
        float as_float[5];
        for (int i = 0; i < 5; ++i) {
            as_float[i] = toFloat(drives[i]);
        }
        float produced[3];
        reproduceRgbww(as_float, produced);
        const i32 mapped_xyz[3] = {q16(produced[0]), q16(produced[1]),
                                   q16(produced[2])};

        i32 target_lab[3];
        i32 mapped_lab[3];
        xyzToOklabQ16(xyz, target_lab);
        xyzToOklabQ16(mapped_xyz, mapped_lab);
        FL_CHECK_LT(hueDivergence(target_lab, mapped_lab), 0.02f);
        FL_CHECK(chromaDidNotGrow(target_lab, mapped_lab));
        if (!in_gamut) {
            ++compressed;
        }
    }
    FL_CHECK_GT(compressed, 0);
}

// ---------------------------------------------------------------------------
// Above the brightest neutral (#4245)
//
// The clamp to `max_neutral_lightness` throws away lightness the hull can
// reach off the neutral axis -- up to 29.7% of the headroom on this device.
// `ci/color_lightness_headroom_study.py` established why the obvious repairs
// fail and what works: above the cap the feasible chroma is still one
// interval, it simply stops containing zero, so a seed has to be found before
// either edge can be bisected. These check the embedded mapper against the
// numbers that study published.
// ---------------------------------------------------------------------------

namespace {

/// Re-render mapped drives through the emitters and read back OKLab.
///
/// Re-rendered rather than assumed: the point of the change is what the
/// device actually emits, and a mapper that reported a lightness it did not
/// produce would pass a test built on its own arithmetic.
void emittedLab(const i32 (&drives)[3], i32 (&out_lab)[3]) {
    const float emitters[3][2] = {
        {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
    float xyz[3] = {0.0f, 0.0f, 0.0f};
    for (int e = 0; e < 3; ++e) {
        float emitter[3];
        colorimetric_response::xyY_to_XYZ(emitters[e][0], emitters[e][1], 1.0f,
                                          emitter);
        for (int i = 0; i < 3; ++i) {
            xyz[i] += toFloat(drives[e]) * emitter[i];
        }
    }
    const i32 emitted[3] = {q16(xyz[0]), q16(xyz[1]), q16(xyz[2])};
    xyzToOklabQ16(emitted, out_lab);
}

float chromaOf(const i32 (&lab)[3]) {
    const float a = toFloat(lab[1]);
    const float b = toFloat(lab[2]);
    return fl::sqrtf(a * a + b * b);
}

}  // namespace

FL_TEST_CASE("Above the cap the mapper keeps the lightness it was asked for") {
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));
    const float cap = toFloat(map.max_neutral_lightness);

    // Hue 300 degrees at chroma 0.30, at two lightnesses above the cap. The
    // study reports 1.2860 / 0.3081 and 1.3978 / 0.4431 for these against the
    // shipped 1.1182 for both.
    struct Case {
        float lightness;
        float expect_chroma;
    };
    const Case cases[] = {{1.286f, 0.3081f}, {1.398f, 0.4431f}};
    for (const auto& c : cases) {
        const i32 lab[3] = {q16(c.lightness), q16(0.15f), q16(-0.2598076f)};
        i32 xyz[3];
        oklabToXyzQ16(lab, xyz);
        i32 drives[3];
        mapAndSolveDrivesQ16(map, xyz, drives);

        for (int i = 0; i < 3; ++i) {
            FL_CHECK_GE(drives[i], 0);
            FL_CHECK_LE(drives[i], kFullDrive);
        }

        i32 emitted[3];
        emittedLab(drives, emitted);
        // The requested lightness, not the cap.
        FL_CHECK_LT(fl::fabsf(toFloat(emitted[0]) - c.lightness), 0.01f);
        FL_CHECK_GT(toFloat(emitted[0]), cap + 0.05f);
        FL_CHECK_LT(fl::fabsf(chromaOf(emitted) - c.expect_chroma), 0.01f);
    }
}

FL_TEST_CASE("The lower edge is not optional above the cap") {
    // At lightness 1.398 on hue 300 the feasible chroma is about
    // [0.4432, 0.6000] -- it starts *above* the requested 0.30. Clamping to
    // the upper edge alone would be a search that never looks below the seed,
    // and clamping to the request would return something infeasible: the
    // answer has to move chroma *up* into the interval.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    const float requested_chroma = 0.30f;
    const i32 lab[3] = {q16(1.398f), q16(0.15f), q16(-0.2598076f)};
    i32 xyz[3];
    oklabToXyzQ16(lab, xyz);
    i32 drives[3];
    mapAndSolveDrivesQ16(map, xyz, drives);

    i32 emitted[3];
    emittedLab(drives, emitted);
    FL_CHECK_GT(chromaOf(emitted), requested_chroma * 1.3f);

    // Hue is still exact -- raising chroma along the same ray is what makes
    // that true, and is why this is not simply "return any feasible colour".
    const float ax = toFloat(lab[1]), ay = toFloat(lab[2]);
    const float bx = toFloat(emitted[1]), by = toFloat(emitted[2]);
    const float scale = fl::sqrtf((ax * ax + ay * ay) * (bx * bx + by * by));
    FL_REQUIRE_GT(scale, 1e-4f);
    FL_CHECK_GT(ax * bx + ay * by, 0.0f);
    FL_CHECK_LT(fl::fabsf(ax * by - ay * bx) / scale, 0.005f);
}

FL_TEST_CASE("It falls back to the clamp when no chroma is feasible there") {
    // Lightness 1.398 on hue 120 has no feasible chroma at all -- the study
    // records `(none)` for that row. The interval search finds no seed and the
    // shipped clamp-then-bisect answers instead, which is what makes this
    // never worse than what it replaces.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    const i32 lab[3] = {q16(1.398f), q16(-0.25f), q16(0.4330127f)};
    i32 xyz[3];
    oklabToXyzQ16(lab, xyz);
    i32 drives[3];
    mapAndSolveDrivesQ16(map, xyz, drives);

    for (int i = 0; i < 3; ++i) {
        FL_CHECK_GE(drives[i], 0);
        FL_CHECK_LE(drives[i], kFullDrive);
    }
    i32 emitted[3];
    emittedLab(drives, emitted);
    FL_CHECK_LT(fl::fabsf(toFloat(emitted[0]) -
                          toFloat(map.max_neutral_lightness)),
                0.01f);
}

FL_TEST_CASE("An over-bright neutral still clamps, because it has no chroma") {
    // The regression this could most easily cause. A neutral has no chroma to
    // scale, so scaling it by any factor leaves it neutral and infeasible; the
    // probes find no seed and the lightness clamp still runs. Without this the
    // change could quietly turn every over-bright grey into a saturated
    // colour and the existing neutral test would not see it, since it checks
    // ratios rather than chroma.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));

    for (float luminance : {2.0f, 5.0f, 30.0f}) {
        i32 xyz[3];
        xyzAt(0.3127f, 0.3290f, luminance, xyz);
        i32 drives[3];
        mapAndSolveDrivesQ16(map, xyz, drives);
        i32 emitted[3];
        emittedLab(drives, emitted);
        FL_CHECK_LT(chromaOf(emitted), 0.01f);
        FL_CHECK_LT(fl::fabsf(toFloat(emitted[0]) -
                              toFloat(map.max_neutral_lightness)),
                    0.01f);
    }
}

namespace {

/// OKLab of a four- or five-emitter drive vector, re-rendered through the
/// device rather than read back off the mapper.
template <int N>
void emittedLabWide(const i32 (&drives)[N], const i32 (&white1)[3],
                    const i32 (&white2)[3], i32 (&out_lab)[3]) {
    const float emitters[3][2] = {
        {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
    float xyz[3] = {0.0f, 0.0f, 0.0f};
    for (int e = 0; e < 3; ++e) {
        float emitter[3];
        colorimetric_response::xyY_to_XYZ(emitters[e][0], emitters[e][1], 1.0f,
                                          emitter);
        for (int i = 0; i < 3; ++i) {
            xyz[i] += toFloat(drives[e]) * emitter[i];
        }
    }
    for (int i = 0; i < 3; ++i) {
        xyz[i] += toFloat(drives[3]) * toFloat(white1[i]);
        if (N > 4) {
            xyz[i] += toFloat(drives[4]) * toFloat(white2[i]);
        }
    }
    const i32 emitted[3] = {q16(xyz[0]), q16(xyz[1]), q16(xyz[2])};
    xyzToOklabQ16(emitted, out_lab);
}

}  // namespace

FL_TEST_CASE("RGBW keeps the lightness above its own, higher cap") {
    // The white emitter moves the cap; it does not change the shape of the
    // problem, so the same interval clamp has to run there too.
    //
    // Low chroma on purpose, and asserted outside the hull before anything
    // else. A wide hull swallows most *saturated* targets a little above the
    // cap outright -- `allocateEmitterDrivesQ16` succeeds and the mapper
    // returns before the clamp is reached -- so a test written at chroma 0.30
    // measures nothing and passes with the whole path disabled. That is the
    // shape of test this guard exists to stop, having written one.
    GamutMapRgbwQ16 rgbw;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                     WhiteAllocationPolicy::WhitePreferred, &rgbw));
    const float cap = toFloat(rgbw.max_neutral_lightness);
    const float requested = cap + 0.15f;

    const i32 lab[3] = {q16(requested), q16(0.05f), q16(-0.0866025f)};
    i32 xyz[3];
    oklabToXyzQ16(lab, xyz);
    i32 drives[4];
    FL_REQUIRE_FALSE(allocateEmitterDrivesQ16(rgbw.allocation, xyz, drives));

    mapAndAllocateRgbwQ16(rgbw, xyz, drives);
    for (int i = 0; i < 4; ++i) {
        FL_CHECK_GE(drives[i], 0);
        FL_CHECK_LE(drives[i], kFullDrive);
    }

    i32 emitted[3];
    emittedLabWide<4>(drives, kWhiteD65, kWhiteD65, emitted);

    // The requested lightness itself, not merely something above the cap.
    // "Above the cap" is satisfied by any improvement at all, which is not
    // what this path claims to do.
    FL_CHECK_LT(fl::fabsf(toFloat(emitted[0]) - requested), 0.01f);
    FL_CHECK_GT(toFloat(emitted[0]), cap + 0.05f);

    // And on the hue it was asked for. Lightness alone would accept a colour
    // of the right brightness on any ray at all, which is not what a
    // hue-preserving mapper promises. `hueDivergence` returns 1.0 for a
    // neutral result and for an anti-parallel one, so this bound catches a
    // collapse to grey and a 180-degree rotation as well as a drift.
    FL_CHECK_LT(hueDivergence(lab, emitted), 0.01f);
}

FL_TEST_CASE("RGBW walks down to what it can reach, not to its cap") {
    // The other half of the contract, and the one that keeps this from ever
    // being worse. It used to assert the answer landed *at* the cap, which
    // was the old fallback -- dropping straight there is what made the mapper
    // step by 234 eight-bit codes at the top of the reachable region on the
    // RGB path. The answer now walks down to the highest lightness that is
    // still reachable, so it is at or above the cap and below what was asked.
    GamutMapRgbwQ16 rgbw;
    FL_REQUIRE(buildGamutMapRgbwQ16(rgbDevice(), kWhiteD65,
                                     WhiteAllocationPolicy::WhitePreferred, &rgbw));
    const float cap = toFloat(rgbw.max_neutral_lightness);
    const float requested = cap + 0.60f;

    const i32 lab[3] = {q16(requested), q16(0.05f), q16(-0.0866025f)};
    i32 xyz[3];
    oklabToXyzQ16(lab, xyz);
    i32 drives[4];
    FL_REQUIRE_FALSE(allocateEmitterDrivesQ16(rgbw.allocation, xyz, drives));

    mapAndAllocateRgbwQ16(rgbw, xyz, drives);
    for (int i = 0; i < 4; ++i) {
        FL_CHECK_GE(drives[i], 0);
        FL_CHECK_LE(drives[i], kFullDrive);
    }

    i32 emitted[3];
    emittedLabWide<4>(drives, kWhiteD65, kWhiteD65, emitted);
    // Materially above the cap, not merely at it. The old fallback dropped
    // straight to the cap and would satisfy any `>= cap - epsilon` floor, so
    // that form of the assertion held with the walk-down deleted. Measured
    // here the walk-down clears the cap by 0.22; the 0.10 bound is half of
    // that, far enough above the noise to be a real claim and far enough
    // below the measurement not to pin the search's exact resolution.
    FL_CHECK_GT(toFloat(emitted[0]), cap + 0.10f);
    FL_CHECK_LT(toFloat(emitted[0]), requested);

    // And asking for more gives the same answer, which is what "walks down to
    // the edge" means and what a step past it would break.
    const i32 higher[3] = {q16(requested + 0.20f), q16(0.05f), q16(-0.0866025f)};
    i32 higher_xyz[3];
    oklabToXyzQ16(higher, higher_xyz);
    i32 higher_drives[4];
    mapAndAllocateRgbwQ16(rgbw, higher_xyz, higher_drives);
    i32 higher_emitted[3];
    emittedLabWide<4>(higher_drives, kWhiteD65, kWhiteD65, higher_emitted);
    FL_CHECK_LT(fl::fabsf(toFloat(higher_emitted[0]) - toFloat(emitted[0])),
                0.02f);
}

FL_TEST_CASE("RGBWW keeps the lightness above its own cap") {
    // Two whites raise the cap again, and the same clamp is wired into that
    // mapper. Wiring it without testing it would have left the third path
    // asserted only by the fact that it compiles.
    GamutMapRgbwwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));
    const float cap = toFloat(map.max_neutral_lightness);
    const float requested = cap + 0.15f;

    const i32 lab[3] = {q16(requested), q16(0.05f), q16(-0.0866025f)};
    i32 xyz[3];
    oklabToXyzQ16(lab, xyz);
    i32 drives[5];
    FL_REQUIRE_FALSE(allocateTwoWhiteDrivesQ16(map.allocation, xyz, drives));

    mapAndAllocateRgbwwQ16(map, xyz, drives);
    for (int i = 0; i < 5; ++i) {
        FL_CHECK_GE(drives[i], 0);
        FL_CHECK_LE(drives[i], kFullDrive);
    }

    i32 emitted[3];
    emittedLabWide<5>(drives, kWhiteD65, kWhiteD50Map, emitted);
    FL_CHECK_LT(fl::fabsf(toFloat(emitted[0]) - requested), 0.01f);
    FL_CHECK_GT(toFloat(emitted[0]), cap + 0.05f);

    FL_CHECK_LT(hueDivergence(lab, emitted), 0.01f);
}

FL_TEST_CASE("RGBWW walks down to what it can reach, not to its cap") {
    GamutMapRgbwwQ16 map;
    FL_REQUIRE(buildGamutMapRgbwwQ16(rgbDevice(), kWhiteD65, kWhiteD50Map,
                                     WhiteAllocationPolicy::WhitePreferred,
                                     &map));
    const float cap = toFloat(map.max_neutral_lightness);

    const float requested = cap + 0.60f;
    const i32 lab[3] = {q16(requested), q16(0.05f), q16(-0.0866025f)};
    i32 xyz[3];
    oklabToXyzQ16(lab, xyz);
    i32 drives[5];
    FL_REQUIRE_FALSE(allocateTwoWhiteDrivesQ16(map.allocation, xyz, drives));

    mapAndAllocateRgbwwQ16(map, xyz, drives);
    for (int i = 0; i < 5; ++i) {
        FL_CHECK_GE(drives[i], 0);
        FL_CHECK_LE(drives[i], kFullDrive);
    }

    i32 emitted[3];
    emittedLabWide<5>(drives, kWhiteD65, kWhiteD50Map, emitted);
    // Same bound, same reason, on the two-white path: measured 0.21 above
    // the cap here, so `cap + 0.10f` separates the walk-down from the
    // direct-to-cap fallback it replaced.
    FL_CHECK_GT(toFloat(emitted[0]), cap + 0.10f);
    FL_CHECK_LT(toFloat(emitted[0]), requested);

    const i32 higher[3] = {q16(requested + 0.20f), q16(0.05f), q16(-0.0866025f)};
    i32 higher_xyz[3];
    oklabToXyzQ16(higher, higher_xyz);
    i32 higher_drives[5];
    mapAndAllocateRgbwwQ16(map, higher_xyz, higher_drives);
    i32 higher_emitted[3];
    emittedLabWide<5>(higher_drives, kWhiteD65, kWhiteD50Map, higher_emitted);
    FL_CHECK_LT(fl::fabsf(toFloat(higher_emitted[0]) - toFloat(emitted[0])),
                0.02f);
}

FL_TEST_CASE("Crossing the cap does not step") {
    // The defect #4271 introduced, and the reason the walk-down exists.
    //
    // Before that change every above-cap target clamped to the neutral cap,
    // so a rising ramp went flat there: continuous, if dim. #4271 made the
    // answer track the requested lightness while a feasible chroma interval
    // could be found -- and then drop all the way back to the cap when the
    // probes stopped landing. Swept along hue 300 at chroma 0.10 that step
    // measured 0.91 in summed drive, **234 eight-bit codes**, against the
    // ~1 code the mapper's other paths stay inside.
    //
    // Walking down to the highest reachable lightness instead lands on the
    // edge of the region rather than past it. Worst step over the same
    // sweeps, at the shipped eight halvings and eight probes:
    //
    //   chroma 0.02   1 code
    //   chroma 0.05   1 code
    //   chroma 0.10   2 codes
    //   chroma 0.20   3 codes
    //   chroma 0.30   3 codes
    //
    // The bound below is in drive units, and 0.02 is about five 8-bit codes:
    // loose enough not to pin the search's exact resolution, tight enough
    // that the 0.91 step could never pass it.
    GamutMapQ16 map;
    FL_REQUIRE(buildGamutMapQ16(rgbDevice(), &map));
    const float cap = toFloat(map.max_neutral_lightness);

    for (float chroma : {0.02f, 0.05f, 0.10f, 0.20f, 0.30f}) {
        const float a = chroma * 0.5f;
        const float b = chroma * -0.8660254f;
        i32 previous[3] = {0, 0, 0};
        float worst = 0.0f;
        int above_cap = 0;
        const int steps = 4000;
        for (int step = 0; step <= steps; ++step) {
            const float t = static_cast<float>(step) / steps;
            const float lightness = cap - 0.10f + t * 0.40f;
            if (lightness > cap) {
                ++above_cap;
            }
            const i32 lab[3] = {q16(lightness), q16(a), q16(b)};
            i32 xyz[3];
            oklabToXyzQ16(lab, xyz);
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
        // The sweep has to actually spend time above the cap, or it is
        // measuring the ordinary path and saying nothing about this one.
        FL_CHECK_GT(above_cap, steps / 4);
        FL_CHECK_LT(worst, 0.02f);
    }
}

}  // FL_TEST_FILE

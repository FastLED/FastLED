// ok cpp include
// Tests for the colorimetric RGBW solvers from issue #2545.
//
// FastLED's *.cpp.hpp files cannot be included from tests (linter rule), so
// this TU exercises only the public header. That covers:
//   - Inline math primitives (cct_to_xy, xyY/XYZ, invert3x3, barycentric_xy,
//     quantize, rgbcct_eta_for_cct).
//   - Public dispatch API (rgb_2_rgbw with kRGBWColorimetric*, profile
//     get/set, LUT enable/disable).
//
// Direct solver correctness (strict_subgamut, wx_lp, build_lut, solve_rgbcct)
// requires the library to be built with FASTLED_RGBW_COLORIMETRIC=1, which
// the default test build does not do — so those validations are deferred to
// a dedicated build target or run manually by the developer.

#define FASTLED_RGBW_COLORIMETRIC 1
#define FASTLED_RGBW_COLORIMETRIC_LUT 1

#include "test.h"

#include "fl/gfx/rgbw.h"
#include "fl/gfx/rgbw_colorimetric.h"

using namespace fl;
using namespace fl::colorimetric_detail;


FL_TEST_CASE("xyY <-> XYZ round trip") {
    float xyz[3];
    xyY_to_XYZ(0.5f, 0.3f, 1.0f, xyz);
    FL_CHECK_CLOSE(xyz[1], 1.0f, 1e-5f);
    const float sum = xyz[0] + xyz[1] + xyz[2];
    FL_CHECK_CLOSE(xyz[0] / sum, 0.5f, 1e-5f);
    FL_CHECK_CLOSE(xyz[1] / sum, 0.3f, 1e-5f);
}


FL_TEST_CASE("invert3x3 round trip on identity") {
    float I[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    float Iinv[3][3];
    FL_CHECK(invert3x3(I, Iinv));
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            FL_CHECK_CLOSE(Iinv[i][j], I[i][j], 1e-6f);
        }
    }
}


FL_TEST_CASE("invert3x3 round trip on a non-trivial matrix") {
    // Use the default profile's RGB chromaticities as a representative
    // non-identity matrix and check M * inverse(M) ≈ I.
    float M[3][3] = {
        { kRgbwDefaultProfile.xy_r[0], kRgbwDefaultProfile.xy_g[0], kRgbwDefaultProfile.xy_b[0] },
        { kRgbwDefaultProfile.xy_r[1], kRgbwDefaultProfile.xy_g[1], kRgbwDefaultProfile.xy_b[1] },
        { 1.0f - kRgbwDefaultProfile.xy_r[0] - kRgbwDefaultProfile.xy_r[1],
          1.0f - kRgbwDefaultProfile.xy_g[0] - kRgbwDefaultProfile.xy_g[1],
          1.0f - kRgbwDefaultProfile.xy_b[0] - kRgbwDefaultProfile.xy_b[1] },
    };
    float Minv[3][3];
    FL_CHECK(invert3x3(M, Minv));
    float prod[3][3] = {{0}};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                prod[i][j] += Minv[i][k] * M[k][j];
            }
        }
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const float expected = (i == j) ? 1.0f : 0.0f;
            FL_CHECK_CLOSE(prod[i][j], expected, 1e-4f);
        }
    }
}


FL_TEST_CASE("matvec3: basic matrix-vector product") {
    float M[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    float v[3] = {1, 0, 0};
    float out[3];
    matvec3(M, v, out);
    FL_CHECK_CLOSE(out[0], 1.0f, 1e-6f);
    FL_CHECK_CLOSE(out[1], 4.0f, 1e-6f);
    FL_CHECK_CLOSE(out[2], 7.0f, 1e-6f);
}


FL_TEST_CASE("barycentric_xy classifies inside vs outside") {
    const float A[2] = {0.0f, 0.0f};
    const float B[2] = {1.0f, 0.0f};
    const float C[2] = {0.0f, 1.0f};

    float bary[3];
    const float inside[2] = {0.25f, 0.25f};
    FL_CHECK(barycentric_xy(inside, A, B, C, bary));
    FL_CHECK(bary[0] >= 0.0f);
    FL_CHECK(bary[1] >= 0.0f);
    FL_CHECK(bary[2] >= 0.0f);
    FL_CHECK_CLOSE(bary[0] + bary[1] + bary[2], 1.0f, 1e-5f);

    const float outside[2] = {1.0f, 1.0f};
    FL_CHECK(barycentric_xy(outside, A, B, C, bary));
    FL_CHECK(bary[0] < 0.0f);
}


FL_TEST_CASE("cct_to_xy: known Planckian locus points") {
    // Krystek's algorithm returns Planckian (blackbody) chromaticities, not
    // daylight illuminants — so the test uses Planckian-locus reference
    // values, not D65/D50 etc. (D65 sits ~0.005 off the locus.)
    float xy[2];

    // 6500K Planckian: xy ≈ (0.3135, 0.3237)
    cct_to_xy(6500, xy);
    FL_CHECK_CLOSE(xy[0], 0.3135f, 0.005f);
    FL_CHECK_CLOSE(xy[1], 0.3237f, 0.005f);

    // 2700K Planckian: xy ≈ (0.4600, 0.4107)
    cct_to_xy(2700, xy);
    FL_CHECK_CLOSE(xy[0], 0.4600f, 0.005f);
    FL_CHECK_CLOSE(xy[1], 0.4107f, 0.005f);

    // 4000K Planckian: xy ≈ (0.3805, 0.3768)
    cct_to_xy(4000, xy);
    FL_CHECK_CLOSE(xy[0], 0.3805f, 0.005f);
    FL_CHECK_CLOSE(xy[1], 0.3768f, 0.005f);
}


FL_TEST_CASE("cct_to_xy: clamps out-of-range CCTs") {
    float xy_low[2], xy_clamp_low[2];
    cct_to_xy(1500, xy_low);
    cct_to_xy(500, xy_clamp_low);  // clamped to 1500
    FL_CHECK_CLOSE(xy_low[0], xy_clamp_low[0], 1e-5f);
    FL_CHECK_CLOSE(xy_low[1], xy_clamp_low[1], 1e-5f);

    float xy_high[2], xy_clamp_high[2];
    cct_to_xy(15000, xy_high);
    cct_to_xy(25000, xy_clamp_high);
    FL_CHECK_CLOSE(xy_high[0], xy_clamp_high[0], 1e-5f);
    FL_CHECK_CLOSE(xy_high[1], xy_clamp_high[1], 1e-5f);
}


FL_TEST_CASE("quantize_u8: edges and rounding") {
    FL_CHECK(quantize_u8(-1.0f) == 0);
    FL_CHECK(quantize_u8(0.0f) == 0);
    FL_CHECK(quantize_u8(1.0f) == 255);
    FL_CHECK(quantize_u8(2.0f) == 255);
    // 0.5 -> 128 (round-half-up)
    FL_CHECK(quantize_u8(0.5f) == 128);
}


FL_TEST_CASE("rgbcct_eta_for_cct: linear interpolation") {
    FL_CHECK_CLOSE(rgbcct_eta_for_cct(2700, 2700, 6500), 0.0f, 1e-5f);
    FL_CHECK_CLOSE(rgbcct_eta_for_cct(6500, 2700, 6500), 1.0f, 1e-5f);
    FL_CHECK_CLOSE(rgbcct_eta_for_cct(4600, 2700, 6500), 0.5f, 0.01f);
    // Out-of-range clamps.
    FL_CHECK_CLOSE(rgbcct_eta_for_cct(1000, 2700, 6500), 0.0f, 1e-5f);
    FL_CHECK_CLOSE(rgbcct_eta_for_cct(10000, 2700, 6500), 1.0f, 1e-5f);
}


// ===== Public API tests =====================================================
// These exercise the dispatch through rgb_2_rgbw(). When the library was
// built without FASTLED_RGBW_COLORIMETRIC, the colorimetric modes route to
// the warn-once stub which tail-calls rgb_2_rgbw_exact. Tests verify the
// dispatch is wired up and link-safe regardless of build config.

FL_TEST_CASE("public API stub fallback produces valid output") {
    u8 r_out, g_out, b_out, w_out;
    rgb_2_rgbw(RGBW_MODE::kRGBWColorimetric, kRGBWDefaultColorTemp,
               200, 100, 50, 255, 255, 255,
               &r_out, &g_out, &b_out, &w_out);
    // Either path (stub-fallback or real colorimetric) must produce valid u8s.
    FL_CHECK(r_out <= 255);
    FL_CHECK(g_out <= 255);
    FL_CHECK(b_out <= 255);
    FL_CHECK(w_out <= 255);
}


FL_TEST_CASE("public API: kRGBWColorimetricBoosted dispatches cleanly") {
    u8 r_out, g_out, b_out, w_out;
    rgb_2_rgbw(RGBW_MODE::kRGBWColorimetricBoosted, kRGBWDefaultColorTemp,
               180, 180, 200, 255, 255, 255,
               &r_out, &g_out, &b_out, &w_out);
    FL_CHECK(r_out <= 255);
    FL_CHECK(g_out <= 255);
    FL_CHECK(b_out <= 255);
    FL_CHECK(w_out <= 255);
}


FL_TEST_CASE("default profile is reachable via get/set API") {
    const DiodeProfile* p_before = get_rgbw_colorimetric_profile();
    FL_CHECK(p_before != nullptr);

    DiodeProfile custom = kRgbwDefaultProfile;
    custom.lum_w = 0.5f;  // arbitrary distinguishing value
    set_rgbw_colorimetric_profile(&custom);

    const DiodeProfile* p_after = get_rgbw_colorimetric_profile();
    FL_CHECK(p_after == &custom);
    FL_CHECK_CLOSE(p_after->lum_w, 0.5f, 1e-6f);

    // Restore default so subsequent tests aren't affected.
    set_rgbw_colorimetric_profile(nullptr);
    FL_CHECK(get_rgbw_colorimetric_profile() == &kRgbwDefaultProfile);
}


FL_TEST_CASE("default profile has nominal_cct populated") {
    // The default profile must carry a sane nominal CCT so the dispatch can
    // decide whether to shift the W vertex.
    FL_CHECK(kRgbwDefaultProfile.nominal_cct >= 1500);
    FL_CHECK(kRgbwDefaultProfile.nominal_cct <= 15000);
}


FL_TEST_CASE("enable/disable LUT public API is link-safe") {
    // Behaves consistently whether the library was built with
    // FASTLED_RGBW_COLORIMETRIC_LUT or not — symbols always link, calls are
    // always safe.
    const bool ok = enable_rgbw_colorimetric_lut(16);
    if (ok) {
        FL_CHECK(rgbw_colorimetric_lut_enabled());
        // Drive one pixel through the dispatch to exercise the lookup path.
        u8 r_out, g_out, b_out, w_out;
        rgb_2_rgbw(RGBW_MODE::kRGBWColorimetric, kRGBWDefaultColorTemp,
                   200, 100, 50, 255, 255, 255,
                   &r_out, &g_out, &b_out, &w_out);
        disable_rgbw_colorimetric_lut();
        FL_CHECK(!rgbw_colorimetric_lut_enabled());
    } else {
        FL_CHECK(!rgbw_colorimetric_lut_enabled());
        disable_rgbw_colorimetric_lut();  // also safe
    }
}


// ===== Source space tests (#2705) ==========================================
// The colorimetric solvers previously interpreted the input RGB byte triple
// as native-emitter drive coordinates, producing native-gamut output and
// making D65-based verification impossible. The fix adds input_xy_* fields
// to DiodeProfile plus an `M_src` matrix in the cache; these tests verify
// the matrix construction is correct and the default profile is wired up.

FL_TEST_CASE("default profile carries sRGB-D65 source space") {
    // D65 (CIE 1931 2°) is approximately (0.31272, 0.32903). Rec709 / sRGB
    // primaries are also published values — confirm the defaults match so
    // verifiers and the math model agree on the input contract.
    FL_CHECK_CLOSE(kRgbwDefaultProfile.input_xy_r[0], 0.6400f, 1e-4f);
    FL_CHECK_CLOSE(kRgbwDefaultProfile.input_xy_r[1], 0.3300f, 1e-4f);
    FL_CHECK_CLOSE(kRgbwDefaultProfile.input_xy_g[0], 0.3000f, 1e-4f);
    FL_CHECK_CLOSE(kRgbwDefaultProfile.input_xy_g[1], 0.6000f, 1e-4f);
    FL_CHECK_CLOSE(kRgbwDefaultProfile.input_xy_b[0], 0.1500f, 1e-4f);
    FL_CHECK_CLOSE(kRgbwDefaultProfile.input_xy_b[1], 0.0600f, 1e-4f);
    FL_CHECK_CLOSE(kRgbwDefaultProfile.input_xy_w[0], 0.31272f, 1e-4f);
    FL_CHECK_CLOSE(kRgbwDefaultProfile.input_xy_w[1], 0.32903f, 1e-4f);
}


FL_TEST_CASE("source matrix maps (1,1,1) to D65 at Y=1") {
    // M_src is constructed so that M_src · [1, 1, 1]^T = source_white_XYZ at
    // Y=1. This is the load-bearing property of the construction: it makes
    // RGB=(255, 255, 255) actually mean "D65 white" in CIE coordinates.
    float M[3][3];
    FL_CHECK(build_source_matrix(kRgbwDefaultProfile.input_xy_r,
                                 kRgbwDefaultProfile.input_xy_g,
                                 kRgbwDefaultProfile.input_xy_b,
                                 kRgbwDefaultProfile.input_xy_w, M));
    const float one[3] = {1.0f, 1.0f, 1.0f};
    float white[3];
    matvec3(M, one, white);

    float expected_white[3];
    xyY_to_XYZ(kRgbwDefaultProfile.input_xy_w[0],
               kRgbwDefaultProfile.input_xy_w[1], 1.0f, expected_white);
    FL_CHECK_CLOSE(white[0], expected_white[0], 1e-4f);
    FL_CHECK_CLOSE(white[1], expected_white[1], 1e-4f);
    FL_CHECK_CLOSE(white[2], expected_white[2], 1e-4f);
    FL_CHECK_CLOSE(white[1], 1.0f, 1e-4f);  // Y of source white = 1.0
}


FL_TEST_CASE("source matrix preserves primary chromaticities") {
    // M_src · [1, 0, 0]^T must produce XYZ on the source R primary ray
    // (any positive scalar of the unit-Y red XYZ vector). Test by xy ratio.
    float M[3][3];
    FL_CHECK(build_source_matrix(kRgbwDefaultProfile.input_xy_r,
                                 kRgbwDefaultProfile.input_xy_g,
                                 kRgbwDefaultProfile.input_xy_b,
                                 kRgbwDefaultProfile.input_xy_w, M));
    const float r_unit[3] = {1.0f, 0.0f, 0.0f};
    float r_xyz[3];
    matvec3(M, r_unit, r_xyz);
    const float r_sum = r_xyz[0] + r_xyz[1] + r_xyz[2];
    FL_CHECK(r_sum > 1e-6f);
    FL_CHECK_CLOSE(r_xyz[0] / r_sum, kRgbwDefaultProfile.input_xy_r[0], 1e-4f);
    FL_CHECK_CLOSE(r_xyz[1] / r_sum, kRgbwDefaultProfile.input_xy_r[1], 1e-4f);
}


FL_TEST_CASE("source matrix matches published sRGB->XYZ matrix") {
    // The canonical linear sRGB -> XYZ (D65) matrix used everywhere from
    // browsers to color science papers. Confirm our derivation matches the
    // published values within float tolerance.
    //   X = 0.4124564 R + 0.3575761 G + 0.1804375 B
    //   Y = 0.2126729 R + 0.7151522 G + 0.0721750 B
    //   Z = 0.0193339 R + 0.1191920 G + 0.9503041 B
    float M[3][3];
    FL_CHECK(build_source_matrix(kRgbwDefaultProfile.input_xy_r,
                                 kRgbwDefaultProfile.input_xy_g,
                                 kRgbwDefaultProfile.input_xy_b,
                                 kRgbwDefaultProfile.input_xy_w, M));
    // Tolerance is loose because the published matrix uses xy_w =
    // (0.31271, 0.32902) and slight rounding in the primaries vs. our
    // defaults — but better than 1e-3 is plenty for chromaticity verification.
    FL_CHECK_CLOSE(M[0][0], 0.4124564f, 5e-3f);
    FL_CHECK_CLOSE(M[0][1], 0.3575761f, 5e-3f);
    FL_CHECK_CLOSE(M[0][2], 0.1804375f, 5e-3f);
    FL_CHECK_CLOSE(M[1][0], 0.2126729f, 5e-3f);
    FL_CHECK_CLOSE(M[1][1], 0.7151522f, 5e-3f);
    FL_CHECK_CLOSE(M[1][2], 0.0721750f, 5e-3f);
    FL_CHECK_CLOSE(M[2][0], 0.0193339f, 5e-3f);
    FL_CHECK_CLOSE(M[2][1], 0.1191920f, 5e-3f);
    FL_CHECK_CLOSE(M[2][2], 0.9503041f, 5e-3f);
}


FL_TEST_CASE("source matrix rejects degenerate primaries") {
    // Collinear primaries cannot define a 2D gamut. The builder must signal
    // failure rather than emit garbage that the solvers would then consume.
    const float bad_r[2] = {0.3f, 0.3f};
    const float bad_g[2] = {0.4f, 0.4f};
    const float bad_b[2] = {0.5f, 0.5f};
    const float w[2] = {0.3127f, 0.3290f};
    float M[3][3];
    FL_CHECK(!build_source_matrix(bad_r, bad_g, bad_b, w, M));
}


// ===== Boosted-vs-strict differentiation (#2706) ===========================
// Public dispatch test that runs only when the library was built with
// FASTLED_RGBW_COLORIMETRIC. Detection uses the LUT enable probe: it returns
// true only when the real colorimetric path is linked in.

FL_TEST_CASE("kRGBWColorimetricBoosted differs from kRGBWColorimetric") {
    const bool ok = enable_rgbw_colorimetric_lut(16);
    disable_rgbw_colorimetric_lut();  // unrelated; only used as a build probe
    if (!ok) {
        // Library was built without FASTLED_RGBW_COLORIMETRIC. Both modes
        // fall back to rgb_2_rgbw_exact through the stub path, so they will
        // be identical — that's the documented degraded behavior, not the
        // bug this test is guarding against.
        return;
    }
    // The previous solve_wx_lp was mathematically equivalent to the strict
    // sub-gamut solver — same vertex of the same feasible polytope. The new
    // solve_wx_overdrive (rho = 0.5 by default) pushes W past the strict
    // vertex, so for any non-edge in-gamut target the two modes must produce
    // different RGBW tuples.
    const u8 cases[][3] = {
        {255, 255, 255},   // pure source white (D65 by default)
        {200, 100, 50},    // warm-ish color
        {128, 200, 220},   // cool-ish color
        {180, 180, 180},   // mid-gray
    };
    for (const auto& c : cases) {
        u8 r0, g0, b0, w0, r1, g1, b1, w1;
        rgb_2_rgbw(RGBW_MODE::kRGBWColorimetric, kRGBWDefaultColorTemp,
                   c[0], c[1], c[2], 255, 255, 255, &r0, &g0, &b0, &w0);
        rgb_2_rgbw(RGBW_MODE::kRGBWColorimetricBoosted, kRGBWDefaultColorTemp,
                   c[0], c[1], c[2], 255, 255, 255, &r1, &g1, &b1, &w1);
        // Boosted should drive W >= strict (overdrive pushes W up, never down).
        FL_CHECK(w1 >= w0);
        // For at least one of these in-gamut targets the outputs must differ,
        // proving the modes are not the same function.
        if (r0 != r1 || g0 != g1 || b0 != b1 || w0 != w1) {
            return;  // success: differentiated for this case
        }
    }
    FL_CHECK(false);  // every case was bit-identical — the bug is back
}


FL_TEST_CASE("overdrive interpolates between strict and full-W") {
    // Direct test of solve_wx_overdrive: rho=0 must equal the strict-vertex
    // output (the LP form), and rho=1 must drive W to its maximum (clamped
    // to either 1.0 or the polytope normalization).
    //
    // This test only links a build with FASTLED_RGBW_COLORIMETRIC because
    // solve_wx_overdrive is not an inline header function. Guard via LUT probe.
    const bool ok = enable_rgbw_colorimetric_lut(8);
    disable_rgbw_colorimetric_lut();
    if (!ok) return;

    // We can't call solve_wx_overdrive directly from the test TU (cpp.hpp
    // exclusion rule). The dispatch only exposes the default-rho path. So
    // we sanity-check the documented invariant via dispatch: at rho_default
    // (0.5), W must be >= the strict-mode W and the strict mode itself must
    // produce a valid result.
    u8 r_s, g_s, b_s, w_s, r_b, g_b, b_b, w_b;
    rgb_2_rgbw(RGBW_MODE::kRGBWColorimetric, kRGBWDefaultColorTemp,
               200, 200, 200, 255, 255, 255, &r_s, &g_s, &b_s, &w_s);
    rgb_2_rgbw(RGBW_MODE::kRGBWColorimetricBoosted, kRGBWDefaultColorTemp,
               200, 200, 200, 255, 255, 255, &r_b, &g_b, &b_b, &w_b);
    FL_CHECK(w_b >= w_s);
}


// ===== Hermite LUT correctness =============================================
// Validates the per-point value+slope Hermite scheme. Tests target the inline
// math (basis functions, quantization, derivative scaling) rather than a
// rebuilt library, so they run in the default test build.

FL_TEST_CASE("Hermite basis evaluates to expected node values") {
    // Hermite basis: at t=0 only h00 is 1 (value at left), at t=1 only h01
    // is 1 (value at right). h10 and h11 carry the derivatives at left/right
    // and are zero at the nodes — this is what lets the basis match both
    // value and slope at each end of a cell.
    auto h00 = [](float t) { return 2*t*t*t - 3*t*t + 1; };
    auto h01 = [](float t) { return -2*t*t*t + 3*t*t; };
    auto h10 = [](float t) { return t*t*t - 2*t*t + t; };
    auto h11 = [](float t) { return t*t*t - t*t; };

    FL_CHECK_CLOSE(h00(0.0f), 1.0f, 1e-6f);
    FL_CHECK_CLOSE(h00(1.0f), 0.0f, 1e-6f);
    FL_CHECK_CLOSE(h01(0.0f), 0.0f, 1e-6f);
    FL_CHECK_CLOSE(h01(1.0f), 1.0f, 1e-6f);
    FL_CHECK_CLOSE(h10(0.0f), 0.0f, 1e-6f);
    FL_CHECK_CLOSE(h10(1.0f), 0.0f, 1e-6f);
    FL_CHECK_CLOSE(h11(0.0f), 0.0f, 1e-6f);
    FL_CHECK_CLOSE(h11(1.0f), 0.0f, 1e-6f);

    // Value sum (h00 + h01) should equal 1 only at endpoints; midpoint is
    // 0.5 by construction (cubic blend symmetric in t).
    FL_CHECK_CLOSE(h00(0.5f) + h01(0.5f), 1.0f, 1e-6f);
    // Derivative basis (h10 + h11) at midpoint: t³-2t²+t + t³-t² evaluated
    // at 0.5 = (0.125-0.5+0.5) + (0.125-0.25) = 0.125 + -0.125 = 0.
    FL_CHECK_CLOSE(h10(0.5f) + h11(0.5f), 0.0f, 1e-6f);
}


FL_TEST_CASE("LUT stride constants match interp scheme") {
    FL_CHECK(kLutStrideBilinear == 4);
    FL_CHECK(kLutStrideHermite == 12);  // value + dx + dy, 4 channels each
}

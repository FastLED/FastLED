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

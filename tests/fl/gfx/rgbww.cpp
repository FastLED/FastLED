// ok cpp include
// Tests for the 5-channel RGBWW dispatch + helpers (issue #2558).
//
// The library build does not define FASTLED_RGBW_COLORIMETRIC by default, so
// the colorimetric dispatch here exercises the stub path (warn-once + zero
// output). Math-correctness of solve_rgbcct itself is covered by
// tests/fl/gfx/rgbw_colorimetric.cpp. Encoder + variant migration coverage
// is in tests/fastled_core.cpp.

#include "test.h"

#include "fl/gfx/rgbww.h"
// Need the full RgbcctProfile definition for the profile get/set test;
// rgbww.h only forward-declares it.
#include "fl/gfx/rgbw_colorimetric.h"

using namespace fl;


FL_TEST_CASE("Rgbww: default-constructed has sane fields") {
    Rgbww cfg;
    FL_CHECK(cfg.warm_cct == kRGBWWDefaultWarmCct);
    FL_CHECK(cfg.cool_cct == kRGBWWDefaultCoolCct);
    FL_CHECK(cfg.rgbww_mode == RGBWW_MODE::kRGBWWColorimetric);
    FL_CHECK(cfg.w_placement == EOrderWW::WWDefault);
    FL_CHECK(cfg.profile == nullptr);
    FL_CHECK(cfg.active());
}


FL_TEST_CASE("RgbwwInvalid sentinel: inactive") {
    Rgbww inv = RgbwwInvalid::value();
    FL_CHECK(inv.rgbww_mode == RGBWW_MODE::kRGBWWInvalid);
    FL_CHECK(!inv.active());
}


FL_TEST_CASE("rgbww_partial_reorder: WwWcEnd places RGB-then-WwWc") {
    u8 b0, b1, b2, b3, b4;
    rgbww_partial_reorder(EOrderWW::WwWcEnd,
                          10, 20, 30,    // RGB bytes (already RGB-ordered)
                          77, 88,        // warm-W, cool-W
                          &b0, &b1, &b2, &b3, &b4);
    FL_CHECK(b0 == 10);
    FL_CHECK(b1 == 20);
    FL_CHECK(b2 == 30);
    FL_CHECK(b3 == 77);   // warm-W
    FL_CHECK(b4 == 88);   // cool-W
}


FL_TEST_CASE("rgbww_partial_reorder: WcWwEnd swaps the W pair") {
    u8 b0, b1, b2, b3, b4;
    rgbww_partial_reorder(EOrderWW::WcWwEnd,
                          10, 20, 30,
                          77, 88,
                          &b0, &b1, &b2, &b3, &b4);
    FL_CHECK(b0 == 10);
    FL_CHECK(b1 == 20);
    FL_CHECK(b2 == 30);
    FL_CHECK(b3 == 88);   // cool-W first
    FL_CHECK(b4 == 77);   // warm-W second
}


FL_TEST_CASE("rgbww_partial_reorder: WwWcStart places W pair before RGB") {
    u8 b0, b1, b2, b3, b4;
    rgbww_partial_reorder(EOrderWW::WwWcStart,
                          10, 20, 30,
                          77, 88,
                          &b0, &b1, &b2, &b3, &b4);
    FL_CHECK(b0 == 77);   // warm-W
    FL_CHECK(b1 == 88);   // cool-W
    FL_CHECK(b2 == 10);
    FL_CHECK(b3 == 20);
    FL_CHECK(b4 == 30);
}


FL_TEST_CASE("rgbww_partial_reorder: WcWwStart") {
    u8 b0, b1, b2, b3, b4;
    rgbww_partial_reorder(EOrderWW::WcWwStart,
                          10, 20, 30,
                          77, 88,
                          &b0, &b1, &b2, &b3, &b4);
    FL_CHECK(b0 == 88);
    FL_CHECK(b1 == 77);
    FL_CHECK(b2 == 10);
    FL_CHECK(b3 == 20);
    FL_CHECK(b4 == 30);
}


FL_TEST_CASE("rgb_2_rgbww dispatch: kRGBWWInvalid emits zeros") {
    Rgbww cfg(2700, 6500, RGBWW_MODE::kRGBWWInvalid, EOrderWW::WwWcEnd);
    u8 r_out = 99, g_out = 99, b_out = 99, ww_out = 99, wc_out = 99;
    rgb_2_rgbww(cfg, 200, 100, 50, 255, 255, 255,
                &r_out, &g_out, &b_out, &ww_out, &wc_out);
    FL_CHECK(r_out == 0);
    FL_CHECK(g_out == 0);
    FL_CHECK(b_out == 0);
    FL_CHECK(ww_out == 0);
    FL_CHECK(wc_out == 0);
}


FL_TEST_CASE("rgb_2_rgbww dispatch: colorimetric paths link-safe") {
    // Without FASTLED_RGBW_COLORIMETRIC the colorimetric dispatch outputs
    // zeros + emits warn-once; with it on, the real solve_rgbcct runs.
    // Either way the call must link, return safely, and produce valid bytes.
    Rgbww cfg;
    u8 r_out, g_out, b_out, ww_out, wc_out;
    rgb_2_rgbww(cfg, 180, 180, 200, 255, 255, 255,
                &r_out, &g_out, &b_out, &ww_out, &wc_out);
    FL_CHECK(r_out <= 255);
    FL_CHECK(g_out <= 255);
    FL_CHECK(b_out <= 255);
    FL_CHECK(ww_out <= 255);
    FL_CHECK(wc_out <= 255);

    Rgbww boosted = cfg;
    boosted.rgbww_mode = RGBWW_MODE::kRGBWWColorimetricBoosted;
    rgb_2_rgbww(boosted, 180, 180, 200, 255, 255, 255,
                &r_out, &g_out, &b_out, &ww_out, &wc_out);
    FL_CHECK(r_out <= 255);
    FL_CHECK(g_out <= 255);
    FL_CHECK(b_out <= 255);
    FL_CHECK(ww_out <= 255);
    FL_CHECK(wc_out <= 255);
}


FL_TEST_CASE("rgb_2_rgbww: user function path returns zero when nothing installed") {
    Rgbww cfg(2700, 6500, RGBWW_MODE::kRGBWWUserFunction, EOrderWW::WwWcEnd);
    u8 r_out = 99, g_out = 99, b_out = 99, ww_out = 99, wc_out = 99;
    rgb_2_rgbww(cfg, 200, 100, 50, 255, 255, 255,
                &r_out, &g_out, &b_out, &ww_out, &wc_out);
    FL_CHECK(r_out == 0);
    FL_CHECK(g_out == 0);
    FL_CHECK(b_out == 0);
    FL_CHECK(ww_out == 0);
    FL_CHECK(wc_out == 0);
}


FL_TEST_CASE("rgb_2_rgbww: user function path dispatches to installed callback") {
    static bool called = false;
    static u8 captured_r = 0, captured_g = 0, captured_b = 0;
    auto user_fn = [](const Rgbww& /*cfg*/,
                      u8 r, u8 g, u8 b,
                      u8 /*r_scale*/, u8 /*g_scale*/, u8 /*b_scale*/,
                      u8* out_r, u8* out_g, u8* out_b,
                      u8* out_ww, u8* out_wc) FL_NOEXCEPT {
        called = true;
        captured_r = r; captured_g = g; captured_b = b;
        *out_r = 1; *out_g = 2; *out_b = 3; *out_ww = 4; *out_wc = 5;
    };
    set_rgb_2_rgbww_function(user_fn);

    Rgbww cfg(2700, 6500, RGBWW_MODE::kRGBWWUserFunction, EOrderWW::WwWcEnd);
    u8 r_out, g_out, b_out, ww_out, wc_out;
    rgb_2_rgbww(cfg, 200, 100, 50, 255, 255, 255,
                &r_out, &g_out, &b_out, &ww_out, &wc_out);
    FL_CHECK(called);
    FL_CHECK(captured_r == 200);
    FL_CHECK(captured_g == 100);
    FL_CHECK(captured_b == 50);
    FL_CHECK(r_out == 1);
    FL_CHECK(g_out == 2);
    FL_CHECK(b_out == 3);
    FL_CHECK(ww_out == 4);
    FL_CHECK(wc_out == 5);

    // Restore default state so subsequent tests aren't affected.
    set_rgb_2_rgbww_function(nullptr);
}


FL_TEST_CASE("rgbww colorimetric profile get/set API") {
    const colorimetric_detail::RgbcctProfile* before = get_rgbww_colorimetric_profile();
    FL_CHECK(before != nullptr);
    FL_CHECK(before == &kRgbwwDefaultProfile);

    colorimetric_detail::RgbcctProfile custom = kRgbwwDefaultProfile;
    custom.warm_path.lum_w = 0.5f;  // arbitrary distinguishing value
    set_rgbww_colorimetric_profile(&custom);
    const colorimetric_detail::RgbcctProfile* after = get_rgbww_colorimetric_profile();
    // By-value contract (#2580 A4 / CodeRabbit on #2560): set_* copies the
    // profile into singleton-owned storage; get_* returns a pointer to that
    // owned copy, never the caller's address. Asserting pointer identity
    // with &custom would re-introduce the dangling-pointer bug — verify the
    // value flowed through and that the pointer is distinct from both the
    // caller's stack address and the default profile.
    FL_CHECK(after != nullptr);
    FL_CHECK(after != &custom);
    FL_CHECK(after != &kRgbwwDefaultProfile);
    FL_CHECK_CLOSE(after->warm_path.lum_w, 0.5f, 1e-6f);

    set_rgbww_colorimetric_profile(nullptr);
    FL_CHECK(get_rgbww_colorimetric_profile() == &kRgbwwDefaultProfile);
}


FL_TEST_CASE("kRgbwwDefaultProfile carries native-LED + D65 source space (#2710)") {
    // Symmetric to the RGBW assertion in rgbw_colorimetric.cpp — locks the
    // contract that the RGBWW default profile ships with native LED gamut on
    // both warm and cool paths so warm/cool primaries can't silently drift
    // out of sync with input_xy_* on future tuning.
    const auto& warm = kRgbwwDefaultProfile.warm_path;
    const auto& cool = kRgbwwDefaultProfile.cool_path;

    FL_CHECK_CLOSE(warm.input_xy_r[0], warm.xy_r[0], 1e-6f);
    FL_CHECK_CLOSE(warm.input_xy_r[1], warm.xy_r[1], 1e-6f);
    FL_CHECK_CLOSE(warm.input_xy_g[0], warm.xy_g[0], 1e-6f);
    FL_CHECK_CLOSE(warm.input_xy_g[1], warm.xy_g[1], 1e-6f);
    FL_CHECK_CLOSE(warm.input_xy_b[0], warm.xy_b[0], 1e-6f);
    FL_CHECK_CLOSE(warm.input_xy_b[1], warm.xy_b[1], 1e-6f);
    FL_CHECK_CLOSE(warm.input_xy_w[0], 0.31272f, 1e-4f);
    FL_CHECK_CLOSE(warm.input_xy_w[1], 0.32903f, 1e-4f);

    FL_CHECK_CLOSE(cool.input_xy_r[0], cool.xy_r[0], 1e-6f);
    FL_CHECK_CLOSE(cool.input_xy_r[1], cool.xy_r[1], 1e-6f);
    FL_CHECK_CLOSE(cool.input_xy_g[0], cool.xy_g[0], 1e-6f);
    FL_CHECK_CLOSE(cool.input_xy_g[1], cool.xy_g[1], 1e-6f);
    FL_CHECK_CLOSE(cool.input_xy_b[0], cool.xy_b[0], 1e-6f);
    FL_CHECK_CLOSE(cool.input_xy_b[1], cool.xy_b[1], 1e-6f);
    FL_CHECK_CLOSE(cool.input_xy_w[0], 0.31272f, 1e-4f);
    FL_CHECK_CLOSE(cool.input_xy_w[1], 0.32903f, 1e-4f);
}

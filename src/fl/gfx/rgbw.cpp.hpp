/// @file rgbw.cpp
/// Functions for red, green, blue, white (RGBW) output

#include "fl/stl/stdint.h"

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"

#include "fl/gfx/rgbw.h"
#include "fl/log/log.h"
#include "fl/stl/singleton.h"

#if FASTLED_RGBW_COLORIMETRIC
// Pulls in the type decls + tiny inline helpers. The heavy solver / LUT /
// RGBCCT implementations live in rgbw_colorimetric.cpp.hpp, included once
// by the unity build manifest (fl/gfx/_build.cpp.hpp).
#include "fl/gfx/rgbw_colorimetric.h"
#endif


namespace fl {

// Default diode profile: SK6812 RGBW3535 at ~6000K. Datasheet wavelength
// midpoints (R 625nm, G 523nm, B 468nm) converted to CIE 1931 xy; W at
// nominal 6000K (xy = 0.32208, 0.33805). Luminance ratios normalized so
// W = 1.0; R/G/B set from datasheet typical mcd ratios for a 5050-class
// part. Users with a colorimeter should override via
// set_rgbw_colorimetric_profile().
const DiodeProfile kRgbwDefaultProfile = {
    /* xy_r        */ { 0.700606f, 0.299300f },
    /* xy_g        */ { 0.097940f, 0.831593f },
    /* xy_b        */ { 0.129086f, 0.049450f },
    /* xy_w        */ { 0.322080f, 0.338050f },
    /* lum_r       */ 0.10f,
    /* lum_g       */ 0.37f,
    /* lum_b       */ 0.08f,
    /* lum_w       */ 1.00f,
    /* nominal_cct */ 6000,
};


namespace {
inline u8 min3(u8 a, u8 b, u8 c) {
    if (a < b) {
        if (a < c) {
            return a;
        } else {
            return c;
        }
    } else {
        if (b < c) {
            return b;
        } else {
            return c;
        }
    }
}

inline u8 divide_by_3(u8 x) {
    u16 y = (u16(x) * 85) >> 8;
    return static_cast<u8>(y);
}
} // namespace

// @brief Converts RGB to RGBW using a color transfer method
// from color channels to 3x white.
// @author Jonathanese
void rgb_2_rgbw_exact(u16 w_color_temperature, u8 r, u8 g,
                      u8 b, u8 r_scale, u8 g_scale,
                      u8 b_scale, u8 *out_r, u8 *out_g,
                      u8 *out_b, u8 *out_w) {
    (void)w_color_temperature;
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    u8 min_component = min3(r, g, b);
    *out_r = r - min_component;
    *out_g = g - min_component;
    *out_b = b - min_component;
    *out_w = min_component;
}

void rgb_2_rgbw_max_brightness(u16 w_color_temperature, u8 r,
                               u8 g, u8 b, u8 r_scale,
                               u8 g_scale, u8 b_scale, u8 *out_r,
                               u8 *out_g, u8 *out_b, u8 *out_w) {
    (void)w_color_temperature;
    *out_r = scale8(r, r_scale);
    *out_g = scale8(g, g_scale);
    *out_b = scale8(b, b_scale);
    *out_w = min3(*out_r, *out_g, *out_b);
}

void rgb_2_rgbw_null_white_pixel(u16 w_color_temperature, u8 r,
                                 u8 g, u8 b, u8 r_scale,
                                 u8 g_scale, u8 b_scale,
                                 u8 *out_r, u8 *out_g, u8 *out_b,
                                 u8 *out_w) {
    (void)w_color_temperature;
    *out_r = scale8(r, r_scale);
    *out_g = scale8(g, g_scale);
    *out_b = scale8(b, b_scale);
    *out_w = 0;
}

void rgb_2_rgbw_white_boosted(u16 w_color_temperature, u8 r,
                              u8 g, u8 b, u8 r_scale,
                              u8 g_scale, u8 b_scale, u8 *out_r,
                              u8 *out_g, u8 *out_b, u8 *out_w) {
    (void)w_color_temperature;
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    u8 min_component = min3(r, g, b);
    u8 w;
    bool is_min = true;
    if (min_component <= 84) {
        w = 3 * min_component;
    } else {
        w = 255;
        is_min = false;
    }
    u8 r_prime;
    u8 g_prime;
    u8 b_prime;
    if (is_min) {
        r_prime = r - min_component;
        g_prime = g - min_component;
        b_prime = b - min_component;
    } else {
        u8 w3 = divide_by_3(w);
        r_prime = r - w3;
        g_prime = g - w3;
        b_prime = b - w3;
    }

    *out_r = r_prime;
    *out_g = g_prime;
    *out_b = b_prime;
    *out_w = w;
}

// User-installable RGB→RGBW function pointer, held behind a lazily-constructed
// Singleton<T>. Replaces the previous static-init form
// (`rgb_2_rgbw_function g_user_function = rgb_2_rgbw_exact`), which forced
// `rgb_2_rgbw_exact` to be ODR-used at static init even when no caller ever
// reached the kRGBWUserFunction code path.
//
// The Singleton's instance is constructed on first call to set_… or
// rgb_2_rgbw_user_function below — never at static init. The default value of
// `fn` is `nullptr`; rgb_2_rgbw_user_function uses `rgb_2_rgbw_exact` as the
// fallback algorithm when nothing has been installed.
//
// See issue #2424 for the broader binary-bloat investigation.
namespace {
struct Rgb2RgbwUserState {
    rgb_2_rgbw_function fn = nullptr;
};
} // namespace

void set_rgb_2_rgbw_function(rgb_2_rgbw_function func) {
    fl::Singleton<Rgb2RgbwUserState>::instance().fn = func;
}

void rgb_2_rgbw_user_function(u16 w_color_temperature, u8 r,
                              u8 g, u8 b, u8 r_scale,
                              u8 g_scale, u8 b_scale, u8 *out_r,
                              u8 *out_g, u8 *out_b, u8 *out_w) {
    rgb_2_rgbw_function fn = fl::Singleton<Rgb2RgbwUserState>::instance().fn;
    if (fn == nullptr) {
        fn = rgb_2_rgbw_exact;
    }
    fn(w_color_temperature, r, g, b, r_scale, g_scale, b_scale,
       out_r, out_g, out_b, out_w);
}

// Active colorimetric profile pointer. Defaults to &kRgbwDefaultProfile on
// first access. Held behind a lazy Singleton<T> (same pattern as the user
// function above) so neither the profile constant nor the singleton itself
// is ODR-used at static init when no colorimetric code path is invoked.
namespace {
struct RgbwColorimetricState {
    const DiodeProfile* profile = nullptr;
};
} // namespace

void set_rgbw_colorimetric_profile(const DiodeProfile* profile) FL_NOEXCEPT {
    fl::Singleton<RgbwColorimetricState>::instance().profile = profile;
}

const DiodeProfile* get_rgbw_colorimetric_profile() FL_NOEXCEPT {
    const DiodeProfile* p =
        fl::Singleton<RgbwColorimetricState>::instance().profile;
    return p != nullptr ? p : &kRgbwDefaultProfile;
}

#if FASTLED_RGBW_COLORIMETRIC

namespace {
// Per-process cached profile data (XYZ columns + matrix inverses). Rebuilt
// whenever the active profile pointer OR the requested CCT changes. The CCT
// part of the key is what makes Rgbw::white_color_temp actually shift the
// W vertex chromaticity at solve time.
struct ColorimetricCacheHolder {
    colorimetric_detail::ProfileCache cache;
    const DiodeProfile* cached_for = nullptr;
    int cached_cct = 0;
};

// Resolve the CCT to use for the W vertex: if the dispatch passed a value in
// the valid Krystek range AND it differs from the profile's nominal CCT,
// shift W. Otherwise use the profile's xy_w as-is (cct_override = 0).
inline int resolve_cct_override(const DiodeProfile* p, int requested_cct) FL_NOEXCEPT {
    if (requested_cct < 1500 || requested_cct > 15000) return 0;
    if (requested_cct == p->nominal_cct) return 0;
    return requested_cct;
}

inline const colorimetric_detail::ProfileCache& get_cache(int cct) FL_NOEXCEPT {
    ColorimetricCacheHolder& h =
        fl::Singleton<ColorimetricCacheHolder>::instance();
    const DiodeProfile* active = get_rgbw_colorimetric_profile();
    const int override_cct = resolve_cct_override(active, cct);
    if (h.cached_for != active || h.cached_cct != override_cct) {
        colorimetric_detail::build_profile_cache(active, override_cct, &h.cache);
        h.cached_for = active;
        h.cached_cct = override_cct;
    }
    return h.cache;
}

// ===== LUT state (issue #2545 Phase 2) ==================================
#if FASTLED_RGBW_COLORIMETRIC_LUT
// The LUT itself is owned by the singleton via fl::unique_ptr — null when
// disabled, non-null and fully built when enabled. The lookup code path
// reads through .table->cells.get() and is guaranteed safe whenever
// `enabled` is true.
struct LutStateHolder {
    fl::unique_ptr<colorimetric_detail::LutTable> table;
    const DiodeProfile* built_for = nullptr;
    int built_cct = 0;
    int requested_grid_n = 0;
    bool enabled = false;
};

inline void rebuild_lut_if_stale(LutStateHolder& s, int cct) FL_NOEXCEPT {
    if (!s.enabled || s.requested_grid_n <= 0) return;
    const DiodeProfile* active = get_rgbw_colorimetric_profile();
    const int override_cct = resolve_cct_override(active, cct);
    if (s.table && s.built_for == active && s.built_cct == override_cct
        && s.table->N == s.requested_grid_n) {
        return;  // up-to-date
    }
    s.table = fl::make_unique<colorimetric_detail::LutTable>(
        colorimetric_detail::build_lut(get_cache(cct), s.requested_grid_n));
    s.built_for = active;
    s.built_cct = override_cct;
}
#endif  // FASTLED_RGBW_COLORIMETRIC_LUT
} // namespace

void rgb_2_rgbw_colorimetric(u16 w_color_temperature, u8 r,
                             u8 g, u8 b, u8 r_scale,
                             u8 g_scale, u8 b_scale, u8 *out_r,
                             u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT {
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    const colorimetric_detail::ProfileCache& cache = get_cache(w_color_temperature);
    float rgbw[4];

#if FASTLED_RGBW_COLORIMETRIC_LUT
    LutStateHolder& lut_state = fl::Singleton<LutStateHolder>::instance();
    if (lut_state.enabled) {
        rebuild_lut_if_stale(lut_state, w_color_temperature);
        float X_t[3];
        X_t[0] = cache.P_R[0] * s_r + cache.P_G[0] * s_g + cache.P_B[0] * s_b;
        X_t[1] = cache.P_R[1] * s_r + cache.P_G[1] * s_g + cache.P_B[1] * s_b;
        X_t[2] = cache.P_R[2] * s_r + cache.P_G[2] * s_g + cache.P_B[2] * s_b;
        const float sum = X_t[0] + X_t[1] + X_t[2];
        if (sum < 1e-9f) {
            *out_r = *out_g = *out_b = *out_w = 0;
            return;
        }
        const float xy_t[2] = { X_t[0] / sum, X_t[1] / sum };
        colorimetric_detail::lookup_lut(*lut_state.table, xy_t, X_t[1], rgbw);
        *out_r = colorimetric_detail::quantize_u8(rgbw[0]);
        *out_g = colorimetric_detail::quantize_u8(rgbw[1]);
        *out_b = colorimetric_detail::quantize_u8(rgbw[2]);
        *out_w = colorimetric_detail::quantize_u8(rgbw[3]);
        return;
    }
#endif

    const bool ok =
        colorimetric_detail::solve_strict_subgamut(cache, s_r, s_g, s_b, rgbw);
    if (!ok) {
        rgb_2_rgbw_exact(w_color_temperature, r, g, b, 255, 255, 255,
                         out_r, out_g, out_b, out_w);
        return;
    }
    *out_r = colorimetric_detail::quantize_u8(rgbw[0]);
    *out_g = colorimetric_detail::quantize_u8(rgbw[1]);
    *out_b = colorimetric_detail::quantize_u8(rgbw[2]);
    *out_w = colorimetric_detail::quantize_u8(rgbw[3]);
}

void rgb_2_rgbw_colorimetric_boosted(u16 w_color_temperature, u8 r,
                                     u8 g, u8 b, u8 r_scale,
                                     u8 g_scale, u8 b_scale, u8 *out_r,
                                     u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT {
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    float rgbw[4];
    colorimetric_detail::solve_wx_lp(get_cache(w_color_temperature),
                                     s_r, s_g, s_b, rgbw);
    *out_r = colorimetric_detail::quantize_u8(rgbw[0]);
    *out_g = colorimetric_detail::quantize_u8(rgbw[1]);
    *out_b = colorimetric_detail::quantize_u8(rgbw[2]);
    *out_w = colorimetric_detail::quantize_u8(rgbw[3]);
}

bool enable_rgbw_colorimetric_lut(int grid_n) FL_NOEXCEPT {
#if FASTLED_RGBW_COLORIMETRIC_LUT
    if (grid_n < 4) grid_n = 4;
    if (grid_n > 256) grid_n = 256;
    LutStateHolder& s = fl::Singleton<LutStateHolder>::instance();
    s.enabled = true;
    s.requested_grid_n = grid_n;
    // Force a rebuild on next colorimetric call.
    s.built_for = nullptr;
    s.built_cct = -1;
    return true;
#else
    (void)grid_n;
    return false;
#endif
}

void disable_rgbw_colorimetric_lut() FL_NOEXCEPT {
#if FASTLED_RGBW_COLORIMETRIC_LUT
    LutStateHolder& s = fl::Singleton<LutStateHolder>::instance();
    s.enabled = false;
    s.table.reset();  // unique_ptr destructor frees the cells storage
    s.requested_grid_n = 0;
    s.built_for = nullptr;
    s.built_cct = 0;
#endif
}

bool rgbw_colorimetric_lut_enabled() FL_NOEXCEPT {
#if FASTLED_RGBW_COLORIMETRIC_LUT
    return fl::Singleton<LutStateHolder>::instance().enabled;
#else
    return false;
#endif
}

#else  // FASTLED_RGBW_COLORIMETRIC

// Stub APIs for the LUT/CCT/RGBCCT path — no-ops when colorimetric is off.
bool enable_rgbw_colorimetric_lut(int /*grid_n*/) FL_NOEXCEPT { return false; }
void disable_rgbw_colorimetric_lut() FL_NOEXCEPT {}
bool rgbw_colorimetric_lut_enabled() FL_NOEXCEPT { return false; }

// Stub path: warn-once + fall back to rgb_2_rgbw_exact. Does not pull in the
// simplex solver, profile cache, or float math machinery.
void rgb_2_rgbw_colorimetric(u16 w_color_temperature, u8 r,
                             u8 g, u8 b, u8 r_scale,
                             u8 g_scale, u8 b_scale, u8 *out_r,
                             u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT {
#ifndef FASTLED_SUPPRESS_COLORIMETRIC_FALLBACK_WARNING
    FL_WARN_ONCE("RGBW: kRGBWColorimetric requested but FASTLED_RGBW_COLORIMETRIC is not defined — falling back to kRGBWExactColors. Define FASTLED_RGBW_COLORIMETRIC=1 to enable the colorimetric path.");
#endif
    rgb_2_rgbw_exact(w_color_temperature, r, g, b, r_scale, g_scale, b_scale,
                     out_r, out_g, out_b, out_w);
}

void rgb_2_rgbw_colorimetric_boosted(u16 w_color_temperature, u8 r,
                                     u8 g, u8 b, u8 r_scale,
                                     u8 g_scale, u8 b_scale, u8 *out_r,
                                     u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT {
#ifndef FASTLED_SUPPRESS_COLORIMETRIC_FALLBACK_WARNING
    FL_WARN_ONCE("RGBW: kRGBWColorimetricBoosted requested but FASTLED_RGBW_COLORIMETRIC is not defined — falling back to kRGBWExactColors. Define FASTLED_RGBW_COLORIMETRIC=1 to enable the colorimetric path.");
#endif
    rgb_2_rgbw_exact(w_color_temperature, r, g, b, r_scale, g_scale, b_scale,
                     out_r, out_g, out_b, out_w);
}

#endif  // FASTLED_RGBW_COLORIMETRIC

void rgbw_partial_reorder(EOrderW w_placement, u8 b0, u8 b1,
                          u8 b2, u8 w, u8 *out_b0,
                          u8 *out_b1, u8 *out_b2, u8 *out_b3) {

    u8 out[4] = {b0, b1, b2, 0};
    switch (w_placement) {
    // unrolled loop for speed.
    case W3:
        out[3] = w;
        break;
    case W2:
        out[3] = out[2];  // memmove and copy.
        out[2] = w;
        break;
    case W1:
        out[3] = out[2];
        out[2] = out[1];
        out[1] = w;
        break;
    case W0:
        out[3] = out[2];
        out[2] = out[1];
        out[1] = out[0];
        out[0] = w;
        break;
    }
    *out_b0 = out[0];
    *out_b1 = out[1];
    *out_b2 = out[2];
    *out_b3 = out[3];
}

} // namespace fl

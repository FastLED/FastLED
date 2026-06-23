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
// part.
//
// Source space defaults to **native LED gamut + D65 white** (#2710): the
// input primaries equal the LED's measured primaries so a saturated input
// like RGB=(255,0,0) reaches the full saturation of the LED red diode,
// and RGB=(255,255,255) lands on D65. This is what most FastLED users
// expect — full use of their hardware's chromatic range. Users who want
// named-gamut input semantics (Rec709 / sRGB, Rec2020, DCI-P3 D65/D60)
// should explicitly call `set_input_gamut()` after copying / constructing
// their profile. See #2705 for the source-space transform itself.
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
    /* input_xy_r  */ { 0.700606f, 0.299300f },  // native LED R
    /* input_xy_g  */ { 0.097940f, 0.831593f },  // native LED G
    /* input_xy_b  */ { 0.129086f, 0.049450f },  // native LED B
    /* input_xy_w  */ { 0.31272f, 0.32903f },    // D65 white
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

// Standard primary chromaticities for the named input gamuts (#2710). The
// numbers come straight from each gamut's published spec:
//   Rec.709 / sRGB    — ITU-R BT.709, IEC 61966-2-1
//   Rec.2020          — ITU-R BT.2020
//   DCI-P3 D65        — SMPTE EG 432-1 (consumer display variant)
//   DCI-P3 D60 (ACES) — ACES AP1 white = (0.32168, 0.33767)
// White-point chromaticities likewise: D65 = (0.31272, 0.32903),
// ACES D60 = (0.32168, 0.33767).
namespace {
struct NamedGamut {
    float xy_r[2];
    float xy_g[2];
    float xy_b[2];
    float xy_w[2];
};
constexpr NamedGamut kRec709    = {{0.6400f, 0.3300f}, {0.3000f, 0.6000f},
                                    {0.1500f, 0.0600f}, {0.31272f, 0.32903f}};
constexpr NamedGamut kRec2020   = {{0.7080f, 0.2920f}, {0.1700f, 0.7970f},
                                    {0.1310f, 0.0460f}, {0.31272f, 0.32903f}};
constexpr NamedGamut kDciP3D65  = {{0.6800f, 0.3200f}, {0.2650f, 0.6900f},
                                    {0.1500f, 0.0600f}, {0.31272f, 0.32903f}};
constexpr NamedGamut kDciP3D60  = {{0.6800f, 0.3200f}, {0.2650f, 0.6900f},
                                    {0.1500f, 0.0600f}, {0.32168f, 0.33767f}};
} // namespace

// Forward declaration: real implementation lives inside the colorimetric
// branch below (and is a no-op when colorimetric math is compiled out).
// Called from set_input_gamut to evict per-process caches keyed on a profile
// whose input_xy_* fields were just mutated in place — without this hook the
// pointer+CCT cache key stays equal and stale M_src/LUT data is reused
// after a gamut switch on the currently active profile.
namespace { void invalidate_colorimetric_caches_for(const DiodeProfile* profile) FL_NOEXCEPT; }

void set_input_gamut(DiodeProfile* profile, InputGamut g,
                     const float white_xy[2]) FL_NOEXCEPT {
    if (profile == nullptr) return;
    auto apply = [profile](const float r[2], const float gp[2],
                           const float b[2], const float w[2]) {
        profile->input_xy_r[0] = r[0];  profile->input_xy_r[1] = r[1];
        profile->input_xy_g[0] = gp[0]; profile->input_xy_g[1] = gp[1];
        profile->input_xy_b[0] = b[0];  profile->input_xy_b[1] = b[1];
        profile->input_xy_w[0] = w[0];  profile->input_xy_w[1] = w[1];
    };
    switch (g) {
    case InputGamut::Native: {
        // For Native, the input gamut tracks this profile's LED primaries —
        // copy them over rather than picking a fixed sRGB-like fallback.
        const float d65[2] = {0.31272f, 0.32903f};
        const float* w = (white_xy != nullptr) ? white_xy : d65;
        apply(profile->xy_r, profile->xy_g, profile->xy_b, w);
        invalidate_colorimetric_caches_for(profile);
        return;
    }
    case InputGamut::Rec709:   apply(kRec709.xy_r,   kRec709.xy_g,   kRec709.xy_b,
                                     white_xy != nullptr ? white_xy : kRec709.xy_w);
                                invalidate_colorimetric_caches_for(profile);   return;
    case InputGamut::Rec2020:  apply(kRec2020.xy_r,  kRec2020.xy_g,  kRec2020.xy_b,
                                     white_xy != nullptr ? white_xy : kRec2020.xy_w);
                                invalidate_colorimetric_caches_for(profile);  return;
    case InputGamut::DciP3D65: apply(kDciP3D65.xy_r, kDciP3D65.xy_g, kDciP3D65.xy_b,
                                     white_xy != nullptr ? white_xy : kDciP3D65.xy_w);
                                invalidate_colorimetric_caches_for(profile); return;
    case InputGamut::DciP3D60: apply(kDciP3D60.xy_r, kDciP3D60.xy_g, kDciP3D60.xy_b,
                                     white_xy != nullptr ? white_xy : kDciP3D60.xy_w);
                                invalidate_colorimetric_caches_for(profile); return;
    }
    // Default-fallthrough for forward-compat with future enum additions:
    // leave the profile's input_xy_* untouched — also nothing to invalidate.
}

void set_input_gamut(DiodeProfile* profile, InputGamut g) FL_NOEXCEPT {
    set_input_gamut(profile, g, nullptr);
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
// Always-compiled when the outer FASTLED_RGBW_COLORIMETRIC gate is on. The
// inner FASTLED_RGBW_COLORIMETRIC_LUT gate that used to wrap this block was
// redundant — once you've opted into colorimetric math, the LUT state +
// rebuild_lut_if_stale + build_lut reference are gc-section-droppable for
// sketches that never call enable_rgbw_colorimetric_lut() (the `enabled`
// flag stays false, the fl::unique_ptr<LutTable> stays empty, and only the
// LutStateHolder singleton instance itself is retained — ~32 bytes).
struct LutStateHolder {
    fl::unique_ptr<colorimetric_detail::LutTable> table;
    const DiodeProfile* built_for = nullptr;
    int built_cct = 0;
    int requested_grid_n = 0;
    RgbwLutInterp requested_interp = RgbwLutInterp::Hermite;
    bool enabled = false;
};

inline colorimetric_detail::LutInterp to_internal_interp(
    RgbwLutInterp i) FL_NOEXCEPT {
    return (i == RgbwLutInterp::Hermite)
        ? colorimetric_detail::LutInterp::Hermite
        : colorimetric_detail::LutInterp::Bilinear;
}

inline void rebuild_lut_if_stale(LutStateHolder& s, int cct) FL_NOEXCEPT {
    if (!s.enabled || s.requested_grid_n <= 0) return;
    const DiodeProfile* active = get_rgbw_colorimetric_profile();
    const int override_cct = resolve_cct_override(active, cct);
    const colorimetric_detail::LutInterp interp =
        to_internal_interp(s.requested_interp);
    if (s.table && s.built_for == active && s.built_cct == override_cct
        && s.table->N == s.requested_grid_n
        && s.table->interp == interp) {
        return;  // up-to-date
    }
    s.table = fl::make_unique<colorimetric_detail::LutTable>(
        colorimetric_detail::build_lut(get_cache(cct), s.requested_grid_n,
                                       interp));
    s.built_for = active;
    s.built_cct = override_cct;
}

// Drop both the ProfileCache and the LUT cache when `profile` matches the
// current cache key. set_input_gamut() mutates input_xy_* in place without
// touching the profile pointer or CCT, so the (pointer, cct) cache key
// stays equal and would otherwise serve stale M_src / LUT data.
void invalidate_colorimetric_caches_for(const DiodeProfile* profile) FL_NOEXCEPT {
    ColorimetricCacheHolder& ch =
        fl::Singleton<ColorimetricCacheHolder>::instance();
    if (ch.cached_for == profile) {
        ch.cached_for = nullptr;
        ch.cached_cct = 0;
    }
    LutStateHolder& lh = fl::Singleton<LutStateHolder>::instance();
    if (lh.built_for == profile) {
        lh.built_for = nullptr;
        lh.built_cct = 0;
    }
}
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

    // LUT fast path — gc-sections drops the branch + singleton + lookup_lut
    // for sketches that never call enable_rgbw_colorimetric_lut().
    LutStateHolder& lut_state = fl::Singleton<LutStateHolder>::instance();
    if (lut_state.enabled) {
        rebuild_lut_if_stale(lut_state, w_color_temperature);
        float X_t[3];
        if (cache.has_source_space) {
            // #2705: use source-space matrix so the LUT lookup targets the
            // same chromaticity as the closed-form solver.
            const float s_vec[3] = { s_r, s_g, s_b };
            colorimetric_detail::matvec3(cache.M_src, s_vec, X_t);
        } else {
            X_t[0] = cache.P_R[0] * s_r + cache.P_G[0] * s_g + cache.P_B[0] * s_b;
            X_t[1] = cache.P_R[1] * s_r + cache.P_G[1] * s_g + cache.P_B[1] * s_b;
            X_t[2] = cache.P_R[2] * s_r + cache.P_G[2] * s_g + cache.P_B[2] * s_b;
        }
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
    colorimetric_detail::solve_wx_overdrive(
        get_cache(w_color_temperature),
        s_r, s_g, s_b,
        colorimetric_detail::kDefaultOverdriveRatio,
        rgbw);
    *out_r = colorimetric_detail::quantize_u8(rgbw[0]);
    *out_g = colorimetric_detail::quantize_u8(rgbw[1]);
    *out_b = colorimetric_detail::quantize_u8(rgbw[2]);
    *out_w = colorimetric_detail::quantize_u8(rgbw[3]);
}

bool enable_rgbw_colorimetric_lut(int grid_n,
                                  RgbwLutInterp interp) FL_NOEXCEPT {
    if (grid_n < 4) grid_n = 4;
    if (grid_n > 256) grid_n = 256;
    LutStateHolder& s = fl::Singleton<LutStateHolder>::instance();
    s.enabled = true;
    s.requested_grid_n = grid_n;
    s.requested_interp = interp;
    // Force a rebuild on next colorimetric call.
    s.built_for = nullptr;
    s.built_cct = -1;
    return true;
}

bool enable_rgbw_colorimetric_lut(int grid_n) FL_NOEXCEPT {
    return enable_rgbw_colorimetric_lut(grid_n, RgbwLutInterp::Hermite);
}

void disable_rgbw_colorimetric_lut() FL_NOEXCEPT {
    LutStateHolder& s = fl::Singleton<LutStateHolder>::instance();
    s.enabled = false;
    s.table.reset();  // unique_ptr destructor frees the cells storage
    s.requested_grid_n = 0;
    s.requested_interp = RgbwLutInterp::Hermite;
    s.built_for = nullptr;
    s.built_cct = 0;
}

bool rgbw_colorimetric_lut_enabled() FL_NOEXCEPT {
    return fl::Singleton<LutStateHolder>::instance().enabled;
}

#else  // FASTLED_RGBW_COLORIMETRIC

// No-op cache invalidation stub — the colorimetric cache machinery doesn't
// exist when FASTLED_RGBW_COLORIMETRIC=0, so set_input_gamut has nothing to
// evict. Keeping the forward-declared symbol available means the dispatch
// path above doesn't need its own #if gate.
namespace { void invalidate_colorimetric_caches_for(const DiodeProfile*) FL_NOEXCEPT {} }

// Stub APIs for the LUT/CCT/RGBCCT path — no-ops when colorimetric is off.
bool enable_rgbw_colorimetric_lut(int /*grid_n*/,
                                  RgbwLutInterp /*interp*/) FL_NOEXCEPT {
    return false;
}
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

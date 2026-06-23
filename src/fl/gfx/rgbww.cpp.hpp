/// @file rgbww.cpp.hpp
/// Dispatch + implementations for the 5-channel RGB->RGBWW path
/// (issue #2558, Phase 3 of #2545).
///
/// The colorimetric modes call `solve_rgbcct()` from
/// fl/gfx/rgbw_colorimetric.h whenever FASTLED_RGBW_COLORIMETRIC is defined
/// (that's the one flag that gates the underlying color math library — see
/// PR #2552). Without it, the dispatch emits FL_WARN_ONCE and outputs five
/// zero bytes. Suppress the warn-once with
/// `-DFASTLED_SUPPRESS_RGBWW_FALLBACK_WARNING=1`.
///
/// There is intentionally no separate FASTLED_RGBWW gate: --gc-sections
/// already drops every RGBWW symbol for sketches that never configure a
/// channel with `mWhiteCfg = Rgbww{...}`, so a second compile flag would
/// only add an extra define for users to remember without any flash savings.

#include "fl/stl/stdint.h"

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"

#include "fl/gfx/rgbww.h"
// rgbw_colorimetric.h carries the RgbcctProfile type definition that
// kRgbwwDefaultProfile needs, plus the inline math primitives Phase D uses.
// The non-inline solve_rgbcct symbol itself only exists when
// FASTLED_RGBW_COLORIMETRIC is defined at library build time — see the gated
// dispatch bodies further down.
#include "fl/gfx/rgbw_colorimetric.h"
#include "fl/log/log.h"
#include "fl/stl/singleton.h"


namespace fl {

namespace {
inline void zero_out(u8 *r, u8 *g, u8 *b, u8 *ww, u8 *wc) FL_NOEXCEPT {
    *r = 0; *g = 0; *b = 0; *ww = 0; *wc = 0;
}
} // namespace

// ===== Default profile + active-profile state ==============================
//
// kRgbwwDefaultProfile: two DiodeProfile entries derived from SK6812-RGBWW-
// style datasheets. R/G/B chromaticities and luminances mirror
// kRgbwDefaultProfile (the colorimetric 4-channel default); W vertex is
// hardcoded to the Planckian xy at 2700K (warm) and 6500K (cool) so static
// initialization stays trivial (no CCT conversion at init time).
//   2700K Planckian xy ≈ (0.4600, 0.4107)  (matches cct_to_xy(2700) ± 0.002)
//   6500K Planckian xy ≈ (0.3135, 0.3237)  (matches cct_to_xy(6500) ± 0.002)
// Default profile: native LED gamut + D65 source white on both warm and cool
// paths (#2710). Users wanting Rec709 / Rec2020 / DCI-P3 input semantics
// should call `fl::set_input_gamut()` on their per-strip profile copy.
const colorimetric_detail::RgbcctProfile kRgbwwDefaultProfile = {
    /* warm_path */ {
        /* xy_r        */ { 0.700606f, 0.299300f },
        /* xy_g        */ { 0.097940f, 0.831593f },
        /* xy_b        */ { 0.129086f, 0.049450f },
        /* xy_w (2700) */ { 0.460000f, 0.410700f },
        /* lum_r       */ 0.10f,
        /* lum_g       */ 0.37f,
        /* lum_b       */ 0.08f,
        /* lum_w       */ 1.00f,
        /* nominal_cct */ 2700,
        /* input_xy_r  */ { 0.700606f, 0.299300f },  // native LED R
        /* input_xy_g  */ { 0.097940f, 0.831593f },  // native LED G
        /* input_xy_b  */ { 0.129086f, 0.049450f },  // native LED B
        /* input_xy_w  */ { 0.31272f, 0.32903f },    // D65
    },
    /* cool_path */ {
        /* xy_r        */ { 0.700606f, 0.299300f },
        /* xy_g        */ { 0.097940f, 0.831593f },
        /* xy_b        */ { 0.129086f, 0.049450f },
        /* xy_w (6500) */ { 0.313500f, 0.323700f },
        /* lum_r       */ 0.10f,
        /* lum_g       */ 0.37f,
        /* lum_b       */ 0.08f,
        /* lum_w       */ 1.00f,
        /* nominal_cct */ 6500,
        /* input_xy_r  */ { 0.700606f, 0.299300f },  // native LED R
        /* input_xy_g  */ { 0.097940f, 0.831593f },  // native LED G
        /* input_xy_b  */ { 0.129086f, 0.049450f },  // native LED B
        /* input_xy_w  */ { 0.31272f, 0.32903f },    // D65
    },
};

namespace {
// Owns a copy of the active profile (see #2580 finding A4, originally
// CodeRabbit on #2560). The earlier implementation stashed the caller's raw
// pointer, which became a dangling reference the moment the caller's
// RgbcctProfile (often a stack local in setup()) went out of scope. We now
// copy by value; has_profile == false reverts to kRgbwwDefaultProfile.
struct RgbwwColorimetricState {
    bool has_profile = false;
    colorimetric_detail::RgbcctProfile profile{};
};
} // namespace

void set_rgbww_colorimetric_profile(const colorimetric_detail::RgbcctProfile* profile) FL_NOEXCEPT {
    RgbwwColorimetricState& state = fl::Singleton<RgbwwColorimetricState>::instance();
    if (profile == nullptr) {
        state.has_profile = false;
        return;
    }
    state.profile = *profile;
    state.has_profile = true;
}

const colorimetric_detail::RgbcctProfile* get_rgbww_colorimetric_profile() FL_NOEXCEPT {
    const RgbwwColorimetricState& state = fl::Singleton<RgbwwColorimetricState>::instance();
    return state.has_profile ? &state.profile : &kRgbwwDefaultProfile;
}

// User-installable RGB->RGBWW function pointer, held behind a lazy
// Singleton<T> (same pattern as the rgb_2_rgbw user function — see #2424 for
// the binary-bloat rationale). Default is nullptr; the user function path
// emits zero output when nothing is installed.
namespace {
struct Rgb2RgbwwUserState {
    rgb_2_rgbww_function fn = nullptr;
};
} // namespace

void set_rgb_2_rgbww_function(rgb_2_rgbww_function func) FL_NOEXCEPT {
    fl::Singleton<Rgb2RgbwwUserState>::instance().fn = func;
}

void rgbww_partial_reorder(EOrderWW ww_placement,
                           u8 b0, u8 b1, u8 b2,
                           u8 ww, u8 wc,
                           u8 *out_b0, u8 *out_b1, u8 *out_b2,
                           u8 *out_b3, u8 *out_b4) FL_NOEXCEPT {
    // Five output slots: out[0..4].
    u8 out[5];
    const u8 enc = static_cast<u8>(ww_placement);
    const u8 ww_idx = (enc >> 4) & 0x07;
    const u8 wc_idx = enc & 0x07;
    out[ww_idx] = ww;
    out[wc_idx] = wc;
    // Fill the three remaining slots with b0, b1, b2 in ascending index order.
    u8 b_idx = 0;
    const u8 rgb_bytes[3] = { b0, b1, b2 };
    for (u8 k = 0; k < 5; ++k) {
        if (k != ww_idx && k != wc_idx) {
            out[k] = rgb_bytes[b_idx++];
        }
    }
    *out_b0 = out[0];
    *out_b1 = out[1];
    *out_b2 = out[2];
    *out_b3 = out[3];
    *out_b4 = out[4];
}

void rgb_2_rgbww_user_function(const Rgbww& cfg,
                               u8 r, u8 g, u8 b,
                               u8 r_scale, u8 g_scale, u8 b_scale,
                               u8 *out_r, u8 *out_g, u8 *out_b,
                               u8 *out_ww, u8 *out_wc) FL_NOEXCEPT {
    rgb_2_rgbww_function fn = fl::Singleton<Rgb2RgbwwUserState>::instance().fn;
    if (fn == nullptr) {
        // No user function installed — produce safe zero output.
        zero_out(out_r, out_g, out_b, out_ww, out_wc);
        return;
    }
    fn(cfg, r, g, b, r_scale, g_scale, b_scale,
       out_r, out_g, out_b, out_ww, out_wc);
}


#if FASTLED_RGBW_COLORIMETRIC

namespace {
// Compute the warm/cool blend factor from the input chromaticity. Simple
// x-based heuristic: warmer inputs (higher x, closer to warm white) → lower
// eta (more warm-W); cooler inputs → higher eta. Mathematically not optimal
// — a chromaticity-aware solver could place eta to minimize dE — but cheap,
// monotonic, and good enough for the common ambilight / neutral-pastel case.
// Per-process cache for the input chromaticity matrix used by
// compute_eta_from_input. `build_source_matrix` runs a 3x3 invert, which is
// far too expensive to repeat per pixel (CodeRabbit #2707). The keyed input
// fields are profile-level data — the matrix is invariant until the active
// profile's input_xy_* changes — so we cache it and invalidate only when one
// of those eight floats differs from the cached snapshot.
struct EtaSourceMatrixCache {
    float xy_r[2] = {0.0f, 0.0f};
    float xy_g[2] = {0.0f, 0.0f};
    float xy_b[2] = {0.0f, 0.0f};
    float xy_w[2] = {0.0f, 0.0f};
    float M_src[3][3] = {{0}};
    bool has_M_src = false;
    bool initialized = false;
};

inline bool xy_equal(const float a[2], const float b[2]) FL_NOEXCEPT {
    return a[0] == b[0] && a[1] == b[1];
}

inline float compute_eta_from_input(const colorimetric_detail::RgbcctProfile& profile,
                                    float s_r, float s_g, float s_b) FL_NOEXCEPT {
    // Compute the input chromaticity in the *source* color space (#2705) when
    // the warm path carries populated input_xy_* fields. Falling back to the
    // emitter-space projection used previously would bias eta toward the LED
    // gamut, so a neutral source white wouldn't blend symmetrically across
    // warm/cool when the source primaries (e.g. sRGB) differ from the LED
    // primaries — exactly the regression flagged on this PR.
    EtaSourceMatrixCache& cache =
        fl::Singleton<EtaSourceMatrixCache>::instance();
    const fl::DiodeProfile& wp = profile.warm_path;
    const bool key_changed =
        !cache.initialized ||
        !xy_equal(cache.xy_r, wp.input_xy_r) ||
        !xy_equal(cache.xy_g, wp.input_xy_g) ||
        !xy_equal(cache.xy_b, wp.input_xy_b) ||
        !xy_equal(cache.xy_w, wp.input_xy_w);
    if (key_changed) {
        cache.xy_r[0] = wp.input_xy_r[0]; cache.xy_r[1] = wp.input_xy_r[1];
        cache.xy_g[0] = wp.input_xy_g[0]; cache.xy_g[1] = wp.input_xy_g[1];
        cache.xy_b[0] = wp.input_xy_b[0]; cache.xy_b[1] = wp.input_xy_b[1];
        cache.xy_w[0] = wp.input_xy_w[0]; cache.xy_w[1] = wp.input_xy_w[1];
        cache.has_M_src = wp.input_xy_w[1] > 1e-6f &&
                          colorimetric_detail::build_source_matrix(
                              wp.input_xy_r, wp.input_xy_g,
                              wp.input_xy_b, wp.input_xy_w, cache.M_src);
        cache.initialized = true;
    }

    float X = 0.0f, Y = 0.0f, Z = 0.0f;
    if (cache.has_M_src) {
        const float s[3] = { s_r, s_g, s_b };
        float xyz[3];
        colorimetric_detail::matvec3(cache.M_src, s, xyz);
        X = xyz[0]; Y = xyz[1]; Z = xyz[2];
    } else {
        // Legacy emitter-space fallback for profiles without populated source.
        // Three xyY_to_XYZ calls per pixel — degraded but still cheap, and
        // only reached when the user explicitly opts out of source space.
        float P_R[3], P_G[3], P_B[3];
        colorimetric_detail::xyY_to_XYZ(wp.xy_r[0], wp.xy_r[1], wp.lum_r, P_R);
        colorimetric_detail::xyY_to_XYZ(wp.xy_g[0], wp.xy_g[1], wp.lum_g, P_G);
        colorimetric_detail::xyY_to_XYZ(wp.xy_b[0], wp.xy_b[1], wp.lum_b, P_B);
        X = P_R[0]*s_r + P_G[0]*s_g + P_B[0]*s_b;
        Y = P_R[1]*s_r + P_G[1]*s_g + P_B[1]*s_b;
        Z = P_R[2]*s_r + P_G[2]*s_g + P_B[2]*s_b;
    }
    const float sum = X + Y + Z;
    if (sum < 1e-9f) return 0.5f;
    const float input_x = X / sum;
    const float warm_x = profile.warm_path.xy_w[0];
    const float cool_x = profile.cool_path.xy_w[0];
    if (cool_x == warm_x) return 0.5f;
    const float eta = (input_x - warm_x) / (cool_x - warm_x);
    if (eta < 0.0f) return 0.0f;
    if (eta > 1.0f) return 1.0f;
    return eta;
}
// Resolve the per-call RgbcctProfile, honoring cfg.warm_cct / cool_cct.
//
// (#2558) CodeRabbit caught that the previous implementation ignored the
// per-strip CCT fields whenever cfg.profile == nullptr — every Rgbww config
// produced the same warm=2700/cool=6500 default. Now we shift the W vertex
// of a per-call profile to whatever CCTs the user requested. When the
// requested CCTs match the default profile's nominal_cct values exactly, we
// short-circuit to the global default to avoid the per-pixel xy recompute.
inline const colorimetric_detail::RgbcctProfile&
resolve_active_rgbcct_profile(const Rgbww& cfg,
                              colorimetric_detail::RgbcctProfile& scratch) FL_NOEXCEPT {
    if (cfg.profile != nullptr) {
        return *cfg.profile;
    }
    const colorimetric_detail::RgbcctProfile* base = get_rgbww_colorimetric_profile();
    if (static_cast<int>(cfg.warm_cct) == base->warm_path.nominal_cct
        && static_cast<int>(cfg.cool_cct) == base->cool_path.nominal_cct) {
        return *base;
    }
    // Build a temp profile with W vertices at the requested CCTs.
    scratch = *base;
    if (static_cast<int>(cfg.warm_cct) != base->warm_path.nominal_cct) {
        float xy[2];
        colorimetric_detail::cct_to_xy(cfg.warm_cct, xy);
        scratch.warm_path.xy_w[0] = xy[0];
        scratch.warm_path.xy_w[1] = xy[1];
        scratch.warm_path.nominal_cct = cfg.warm_cct;
    }
    if (static_cast<int>(cfg.cool_cct) != base->cool_path.nominal_cct) {
        float xy[2];
        colorimetric_detail::cct_to_xy(cfg.cool_cct, xy);
        scratch.cool_path.xy_w[0] = xy[0];
        scratch.cool_path.xy_w[1] = xy[1];
        scratch.cool_path.nominal_cct = cfg.cool_cct;
    }
    return scratch;
}

} // namespace

void rgb_2_rgbww_colorimetric(const Rgbww& cfg,
                              u8 r, u8 g, u8 b,
                              u8 r_scale, u8 g_scale, u8 b_scale,
                              u8 *out_r, u8 *out_g, u8 *out_b,
                              u8 *out_ww, u8 *out_wc) FL_NOEXCEPT {
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    if ((r | g | b) == 0) { zero_out(out_r, out_g, out_b, out_ww, out_wc); return; }

    colorimetric_detail::RgbcctProfile scratch;
    const colorimetric_detail::RgbcctProfile& profile =
        resolve_active_rgbcct_profile(cfg, scratch);

    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    const float eta = compute_eta_from_input(profile, s_r, s_g, s_b);

    float rgbww[5];
    colorimetric_detail::solve_rgbcct(profile, s_r, s_g, s_b, eta, rgbww);

    *out_r  = colorimetric_detail::quantize_u8(rgbww[0]);
    *out_g  = colorimetric_detail::quantize_u8(rgbww[1]);
    *out_b  = colorimetric_detail::quantize_u8(rgbww[2]);
    *out_ww = colorimetric_detail::quantize_u8(rgbww[3]);
    *out_wc = colorimetric_detail::quantize_u8(rgbww[4]);
}

void rgb_2_rgbww_colorimetric_boosted(const Rgbww& cfg,
                                      u8 r, u8 g, u8 b,
                                      u8 r_scale, u8 g_scale, u8 b_scale,
                                      u8 *out_r, u8 *out_g, u8 *out_b,
                                      u8 *out_ww, u8 *out_wc) FL_NOEXCEPT {
    // Phase D MVP: the boosted variant uses the same RGBCCT line-blend as the
    // strict path but skews eta toward 0.5 (equal warm+cool) so the combined
    // W contribution is larger when the input chromaticity sits near neutral.
    // A future revision can use a wx_lp-style maximization here.
    r = scale8(r, r_scale);
    g = scale8(g, g_scale);
    b = scale8(b, b_scale);
    if ((r | g | b) == 0) { zero_out(out_r, out_g, out_b, out_ww, out_wc); return; }

    colorimetric_detail::RgbcctProfile scratch;
    const colorimetric_detail::RgbcctProfile& profile =
        resolve_active_rgbcct_profile(cfg, scratch);

    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    const float eta_chroma = compute_eta_from_input(profile, s_r, s_g, s_b);
    // Push eta halfway toward 0.5 (equal blend) for more total W participation.
    const float eta = 0.5f * eta_chroma + 0.5f * 0.5f;

    float rgbww[5];
    colorimetric_detail::solve_rgbcct(profile, s_r, s_g, s_b, eta, rgbww);

    *out_r  = colorimetric_detail::quantize_u8(rgbww[0]);
    *out_g  = colorimetric_detail::quantize_u8(rgbww[1]);
    *out_b  = colorimetric_detail::quantize_u8(rgbww[2]);
    *out_ww = colorimetric_detail::quantize_u8(rgbww[3]);
    *out_wc = colorimetric_detail::quantize_u8(rgbww[4]);
}

#else  // FASTLED_RGBW_COLORIMETRIC

// Stub path when the colorimetric math library is not compiled in. Emits
// FL_WARN_ONCE (suppressible) and five zero bytes. Does not pull in the
// solver, profile cache, or float math machinery — same gc-section behavior
// as the rest of the colorimetric Phase 1 dispatch in PR #2552.
void rgb_2_rgbww_colorimetric(const Rgbww& cfg,
                              u8 r, u8 g, u8 b,
                              u8 r_scale, u8 g_scale, u8 b_scale,
                              u8 *out_r, u8 *out_g, u8 *out_b,
                              u8 *out_ww, u8 *out_wc) FL_NOEXCEPT {
    (void)cfg; (void)r; (void)g; (void)b;
    (void)r_scale; (void)g_scale; (void)b_scale;
#ifndef FASTLED_SUPPRESS_RGBWW_FALLBACK_WARNING
    FL_WARN_ONCE("RGBWW: kRGBWWColorimetric requires FASTLED_RGBW_COLORIMETRIC=1 "
                 "(the math library that provides solve_rgbcct). Outputting zeros.");
#endif
    zero_out(out_r, out_g, out_b, out_ww, out_wc);
}

void rgb_2_rgbww_colorimetric_boosted(const Rgbww& cfg,
                                      u8 r, u8 g, u8 b,
                                      u8 r_scale, u8 g_scale, u8 b_scale,
                                      u8 *out_r, u8 *out_g, u8 *out_b,
                                      u8 *out_ww, u8 *out_wc) FL_NOEXCEPT {
    (void)cfg; (void)r; (void)g; (void)b;
    (void)r_scale; (void)g_scale; (void)b_scale;
#ifndef FASTLED_SUPPRESS_RGBWW_FALLBACK_WARNING
    FL_WARN_ONCE("RGBWW: kRGBWWColorimetricBoosted requires FASTLED_RGBW_COLORIMETRIC=1. "
                 "Outputting zeros.");
#endif
    zero_out(out_r, out_g, out_b, out_ww, out_wc);
}

#endif  // FASTLED_RGBW_COLORIMETRIC

} // namespace fl

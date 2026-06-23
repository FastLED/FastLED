/// @file rgbww.h
/// 5-channel RGB + warm-W + cool-W (RGBWW / RGBCCT) configuration types
/// (issue #2558, Phase 3 of #2545). Mirrors fl/gfx/rgbw.h but carries two
/// CCTs for the layered solver. The actual rgb_2_rgbww dispatch and encoder
/// pipeline arrive in later phases — this header defines TYPES ONLY so the
/// rest of the codebase can reference Rgbww / RGBWW_MODE / EOrderWW.

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward decl — full definition in fl/gfx/rgbw_colorimetric.h. Avoiding the
// transitive include keeps Rgbww's compile cost minimal in callers that don't
// need the colorimetric solver.
namespace colorimetric_detail {
struct RgbcctProfile;
}

/// White-byte ordering for 5-channel RGBWW output. Two white bytes consume
/// two of the five wire slots; RGB fills the remaining three in EOrder order.
/// `WwWcEnd` (warm then cool, both at end) matches typical SK6812-RGBWW
/// data sheets.
enum class EOrderWW : fl::u8 {
    WwWcEnd   = 0x34,   ///< RGB followed by warm-W, cool-W
    WcWwEnd   = 0x43,   ///< RGB followed by cool-W, warm-W
    WwWcStart = 0x01,   ///< warm-W, cool-W, then RGB
    WcWwStart = 0x10,   ///< cool-W, warm-W, then RGB
    WWDefault = WwWcEnd,
};

/// RGB -> RGBWW conversion modes (issue #2558, Phase 3 of #2545). All modes
/// are colorimetric — there is no min(RGB)-style analog that generalizes to
/// two whites. The real solver runs whenever `FASTLED_RGBW_COLORIMETRIC=1`
/// (the same flag that gates the 4-channel colorimetric path); otherwise
/// dispatch emits FL_WARN_ONCE and outputs zeros. No separate FASTLED_RGBWW
/// flag exists — gc-sections drops unused RGBWW symbols for sketches that
/// never configure a channel with `mWhiteCfg = Rgbww{...}`.
enum class RGBWW_MODE : fl::u8 {
    kRGBWWInvalid,
    kRGBWWColorimetric,
    kRGBWWColorimetricBoosted,
    // IMPORTANT: kRGBWWUserFunction MUST remain the last enumerator. New
    // modes are added immediately above this line so the user-function
    // ordinal stays as the dispatch max value.
    kRGBWWUserFunction,
};

/// Default warm / cool reference CCTs for built-in profiles.
enum {
    kRGBWWDefaultWarmCct = 2700,
    kRGBWWDefaultCoolCct = 6500,
};

/// Per-strip RGBWW configuration. Parallels fl::Rgbw but carries the two
/// reference CCTs the layered solver needs (warm-W and cool-W).
struct Rgbww {
    explicit Rgbww(fl::u16 warm = kRGBWWDefaultWarmCct,
                   fl::u16 cool = kRGBWWDefaultCoolCct,
                   RGBWW_MODE mode = RGBWW_MODE::kRGBWWColorimetric,
                   EOrderWW placement = EOrderWW::WWDefault) FL_NOEXCEPT
        : warm_cct(warm), cool_cct(cool),
          rgbww_mode(mode), w_placement(placement),
          profile(nullptr) {}

    fl::u16 warm_cct;
    fl::u16 cool_cct;
    RGBWW_MODE rgbww_mode;
    EOrderWW w_placement;
    /// Optional override for the diode profile (warm + cool W chromaticity +
    /// luminance). nullptr means "use kRgbwwDefaultProfile" — wired up in
    /// Phase D. Storage is borrowed; caller must keep the profile alive for
    /// the lifetime of any controller using this Rgbww.
    const colorimetric_detail::RgbcctProfile* profile;

    FASTLED_FORCE_INLINE bool active() const FL_NOEXCEPT {
        return rgbww_mode != RGBWW_MODE::kRGBWWInvalid;
    }
};

/// Sentinel: disables RGBWW (variant should hold fl::Empty instead, but this
/// is kept for symmetry with RgbwInvalid).
struct RgbwwInvalid : public Rgbww {
    RgbwwInvalid() FL_NOEXCEPT : Rgbww() {
        rgbww_mode = RGBWW_MODE::kRGBWWInvalid;
    }
    static Rgbww value() FL_NOEXCEPT { return RgbwwInvalid(); }
};

/// Default RGBWW configuration: colorimetric mode at warm=2700K / cool=6500K,
/// default profile, end-aligned warm-then-cool W byte placement.
struct RgbwwDefault : public Rgbww {
    RgbwwDefault() FL_NOEXCEPT : Rgbww() {}
    static Rgbww value() FL_NOEXCEPT { return RgbwwDefault(); }
};

// ===== rgb_2_rgbww dispatch surface (#2558) =================================
//
// Mirror of the rgb_2_rgbw dispatch family. Real-path implementations run
// when FASTLED_RGBW_COLORIMETRIC=1 is set at library build time; without it,
// the library ships stubs that emit FL_WARN_ONCE and output zeros.
//
// Signature note: unlike rgb_2_rgbw_function (scalar fields), the RGBWW
// signature takes the Rgbww config struct by-ref to carry the two CCTs and
// the optional profile pointer in one parameter.

typedef void (*rgb_2_rgbww_function)(const Rgbww& cfg,
                                     fl::u8 r, fl::u8 g, fl::u8 b,
                                     fl::u8 r_scale, fl::u8 g_scale, fl::u8 b_scale,
                                     fl::u8 *out_r, fl::u8 *out_g, fl::u8 *out_b,
                                     fl::u8 *out_ww, fl::u8 *out_wc);

/// Colorimetric strict sub-gamut solver for RGBWW (gist sec 5 + sec 11-12,
/// using solve_rgbcct from rgbw_colorimetric.h). Requires FASTLED_RGBW_COLORIMETRIC;
/// stub emits FL_WARN_ONCE + zeros otherwise.
void rgb_2_rgbww_colorimetric(const Rgbww& cfg,
                              fl::u8 r, fl::u8 g, fl::u8 b,
                              fl::u8 r_scale, fl::u8 g_scale, fl::u8 b_scale,
                              fl::u8 *out_r, fl::u8 *out_g, fl::u8 *out_b,
                              fl::u8 *out_ww, fl::u8 *out_wc) FL_NOEXCEPT;

/// Colorimetric white-overdrive solver for RGBWW (wx_lp_legacy + RGBCCT
/// layered blend). Requires FASTLED_RGBW_COLORIMETRIC; stub emits FL_WARN_ONCE + zeros.
void rgb_2_rgbww_colorimetric_boosted(const Rgbww& cfg,
                                      fl::u8 r, fl::u8 g, fl::u8 b,
                                      fl::u8 r_scale, fl::u8 g_scale, fl::u8 b_scale,
                                      fl::u8 *out_r, fl::u8 *out_g, fl::u8 *out_b,
                                      fl::u8 *out_ww, fl::u8 *out_wc) FL_NOEXCEPT;

/// User-installable RGB->RGBWW function. Set via set_rgb_2_rgbww_function().
/// Falls back to all-zero output when no user function has been installed.
void rgb_2_rgbww_user_function(const Rgbww& cfg,
                               fl::u8 r, fl::u8 g, fl::u8 b,
                               fl::u8 r_scale, fl::u8 g_scale, fl::u8 b_scale,
                               fl::u8 *out_r, fl::u8 *out_g, fl::u8 *out_b,
                               fl::u8 *out_ww, fl::u8 *out_wc) FL_NOEXCEPT;

void set_rgb_2_rgbww_function(rgb_2_rgbww_function func) FL_NOEXCEPT;

// ===== Default RGBCCT profile + active-profile API (Phase D of #2558) =======
//
// Ships the default warm-W (2700K) + cool-W (6500K) profile used when an
// Rgbww config carries `profile = nullptr`. Users with a colorimeter or
// custom RGBWW chipset install their own via set_rgbww_colorimetric_profile().
// Pointer is borrowed — caller keeps the profile alive for the lifetime of
// any controller referencing it. Pass nullptr to revert to the default.

extern const colorimetric_detail::RgbcctProfile kRgbwwDefaultProfile;

void set_rgbww_colorimetric_profile(const colorimetric_detail::RgbcctProfile* profile) FL_NOEXCEPT;
const colorimetric_detail::RgbcctProfile* get_rgbww_colorimetric_profile() FL_NOEXCEPT;

/// @brief Dispatch RGB->RGBWW for a given mode.
/// Reorder a 5-channel pixel given an EOrderWW placement. The three RGB bytes
/// (b0, b1, b2) are assumed to already be in native LED RGB order — this
/// function only handles the warm-W / cool-W insertion. Outputs the final
/// 5-byte stream in wire order.
///
/// Encoding convention (matches EOrderWW enum values):
///   high nibble = warm-W destination index (0..4)
///   low  nibble = cool-W destination index (0..4)
/// The remaining three indices receive b0, b1, b2 in ascending order.
void rgbww_partial_reorder(EOrderWW ww_placement,
                           fl::u8 b0, fl::u8 b1, fl::u8 b2,
                           fl::u8 ww, fl::u8 wc,
                           fl::u8 *out_b0, fl::u8 *out_b1, fl::u8 *out_b2,
                           fl::u8 *out_b3, fl::u8 *out_b4) FL_NOEXCEPT;

FASTLED_FORCE_INLINE void
rgb_2_rgbww(const Rgbww& cfg,
            fl::u8 r, fl::u8 g, fl::u8 b,
            fl::u8 r_scale, fl::u8 g_scale, fl::u8 b_scale,
            fl::u8 *out_r, fl::u8 *out_g, fl::u8 *out_b,
            fl::u8 *out_ww, fl::u8 *out_wc) FL_NOEXCEPT {
    switch (cfg.rgbww_mode) {
    case RGBWW_MODE::kRGBWWInvalid:
        // Inactive: emit zeros across all five channels.
        *out_r = *out_g = *out_b = *out_ww = *out_wc = 0;
        return;
    case RGBWW_MODE::kRGBWWColorimetric:
        rgb_2_rgbww_colorimetric(cfg, r, g, b, r_scale, g_scale, b_scale,
                                 out_r, out_g, out_b, out_ww, out_wc);
        return;
    case RGBWW_MODE::kRGBWWColorimetricBoosted:
        rgb_2_rgbww_colorimetric_boosted(cfg, r, g, b, r_scale, g_scale, b_scale,
                                         out_r, out_g, out_b, out_ww, out_wc);
        return;
    case RGBWW_MODE::kRGBWWUserFunction:
        rgb_2_rgbww_user_function(cfg, r, g, b, r_scale, g_scale, b_scale,
                                  out_r, out_g, out_b, out_ww, out_wc);
        return;
    }
    *out_r = *out_g = *out_b = *out_ww = *out_wc = 0;
}

} // namespace fl

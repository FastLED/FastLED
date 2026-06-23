/// @file rgbw.h
/// Functions for red, green, blue, white (RGBW) output

#pragma once

#include "fl/stl/stdint.h"

#include "eorder.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {

enum class RGBW_MODE {
    kRGBWInvalid,
    kRGBWNullWhitePixel,
    kRGBWExactColors,
    kRGBWBoostedWhite,
    kRGBWMaxBrightness,
    // Chromaticity-aware modes (issue #2545). Off by default at compile time:
    // require `#define FASTLED_RGBW_COLORIMETRIC 1` to enable the real math
    // path. Without the define, selecting either mode emits FL_WARN_ONCE and
    // falls back to kRGBWExactColors.
    kRGBWColorimetric,
    kRGBWColorimetricBoosted,
    // IMPORTANT: kRGBWUserFunction MUST remain the last enumerator. New modes
    // are added immediately above this line so the user-function ordinal does
    // not shift and the dispatch table can rely on it being the maximum value.
    kRGBWUserFunction,
};

// Per-strip diode chromaticity + peak luminance, used by the colorimetric
// modes. The default constant (`kRgbwDefaultProfile`) ships with the library
// and uses datasheet-derived values for SK6812 RGBW3535 at 6000K. Users with a
// colorimeter can build their own and install it via
// `set_rgbw_colorimetric_profile`.
//
// Source-space fields (input_xy_*, issue #2705) define the color space the
// input RGB triple is interpreted in. The default profile (#2710) ships
// with **native LED gamut + D65 white** — input primaries equal the LED's
// own xy_r/g/b — so a saturated input like RGB=(255,0,0) reaches the LED
// red diode at full drive and RGB=(255,255,255) lands on D65. Solvers
// compute `X_t = M_src · source_rgb` where M_src is the standard CIE
// primary-matrix construction from these four chromaticities.
//
// Users wanting standard color-space input semantics (Rec709 / sRGB,
// Rec2020, DCI-P3 D65/D60) should opt in explicitly via
// `fl::set_input_gamut(profile, fl::InputGamut::Rec709)` etc.
//
// If input_xy_w[1] is left at 0.0f (the default for value-initialized
// profiles `DiodeProfile{}`), solvers fall back to the legacy
// interpretation in which the input RGB triple IS the device-emitter
// drive coordinates — preserved for backward compatibility.
struct DiodeProfile {
    float xy_r[2];      // R diode chromaticity (CIE 1931 xy)
    float xy_g[2];      // G diode chromaticity
    float xy_b[2];      // B diode chromaticity
    float xy_w[2];      // W diode chromaticity (at nominal_cct)
    float lum_r;        // R diode peak luminance (relative to W = 1.0)
    float lum_g;
    float lum_b;
    float lum_w;        // typically 1.0 by convention
    int nominal_cct;    // CCT at which xy_w was measured (e.g. 6000)

    // Source / input color space (#2705). See above for default behavior.
    float input_xy_r[2];  // source R primary chromaticity (default: native LED R)
    float input_xy_g[2];  // source G primary chromaticity (default: native LED G)
    float input_xy_b[2];  // source B primary chromaticity (default: native LED B)
    float input_xy_w[2];  // source white chromaticity (default D65)
};

// Named source color spaces for opting into standard input-gamut semantics
// (#2710). The default `kRgbwDefaultProfile` ships as `Native`; pick a
// named gamut here when feeding content authored for that space (e.g.
// sRGB / Rec.709 for ambilight, Rec.2020 for wide-gamut video, DCI-P3
// for cinema-mastered material).
enum class InputGamut : u8 {
    Native = 0,    // Input primaries == this profile's LED primaries + D65 white
    Rec709,        // sRGB / Rec.709 primaries + D65 white
    Rec2020,       // Rec.2020 (UHDTV) primaries + D65 white
    DciP3D65,      // DCI-P3 D65 — consumer display variant (e.g. Apple)
    DciP3D60,      // DCI-P3 D60 — ACES / cinema mastering variant
};

// Reconfigure `profile`'s input_xy_r/g/b/w to one of the named gamuts.
// For `InputGamut::Native`, copies the profile's own xy_r/g/b into
// input_xy_r/g/b and sets input_xy_w to D65. For named gamuts, uses the
// standard published primary chromaticities + that gamut's reference white.
// Mutates `profile` in place; subsequent solver calls observe the new gamut
// (the cache is keyed on the profile pointer and rebuilds automatically).
// No-op if `profile == nullptr`.
void set_input_gamut(DiodeProfile* profile, InputGamut g) FL_NOEXCEPT;

// Same as the above, but lets you optionally override the input white point.
// `white_xy` either points to a 2-float (x, y) chromaticity OR is `nullptr`
// to fall back to the gamut's standard reference white (equivalent to the
// 2-argument overload above). Use the override for niche cases (D50
// photography workflow, D60 ACES cinema, a custom calibration target) where
// the standard gamut's reference white doesn't match your content.
void set_input_gamut(DiodeProfile* profile, InputGamut g,
                     const float white_xy[2]) FL_NOEXCEPT;

// Default profile: SK6812 RGBW3535 @ ~6000K (datasheet wavelengths -> xy +
// typical luminance ratios). Always declared so user code referencing it
// compiles whether or not FASTLED_RGBW_COLORIMETRIC is defined.
extern const DiodeProfile kRgbwDefaultProfile;

// Install a user-supplied profile. The pointer is stored — caller must keep
// the DiodeProfile alive for as long as a colorimetric mode is active.
// No-op when FASTLED_RGBW_COLORIMETRIC is undefined.
void set_rgbw_colorimetric_profile(const DiodeProfile* profile) FL_NOEXCEPT;

// Currently active profile (defaults to &kRgbwDefaultProfile).
const DiodeProfile* get_rgbw_colorimetric_profile() FL_NOEXCEPT;

// Interpolation scheme used by the colorimetric LUT (#2720). Bilinear stores
// 4 channel values per grid cell (8 B/cell at i16 quantization) — cheapest in
// flash/RAM. Hermite additionally stores ∂/∂t_x and ∂/∂t_y per channel
// (24 B/cell, 3x storage) and reaches lower error at the same grid size,
// enabling small grids (N=8..16) on memory-constrained targets.
enum class RgbwLutInterp : u8 {
    Bilinear = 0,
    Hermite = 1,
};

// Enable the 2D + 1D factored LUT path (issue #2545 Phase 2). When enabled,
// the colorimetric modes use an interpolated LUT instead of solving per pixel.
// Available whenever FASTLED_RGBW_COLORIMETRIC=1 — no separate
// FASTLED_RGBW_COLORIMETRIC_LUT flag exists; gc-sections drops the LUT path
// for sketches that never call this. `grid_n` is clamped to [4, 256] and
// controls the LUT edge length. LUT is rebuilt whenever the active profile,
// CCT, grid size, or interp scheme changes. Returns false on allocation
// failure (or always false when FASTLED_RGBW_COLORIMETRIC is undefined).
//
// Memory cost = grid_n * grid_n * (interp == Hermite ? 24 : 8) bytes:
//   N=8   ->  Bilinear   512 B / Hermite  1 536 B
//   N=16  ->  Bilinear 2 048 B / Hermite  6 144 B
//   N=32  ->  Bilinear 8 192 B / Hermite 24 576 B
//   N=64  ->  Bilinear 32 KB  / Hermite ~96 KB
//
// The single-arg overload preserves source compatibility and forwards to
// RgbwLutInterp::Hermite (the historical default since #2707).
bool enable_rgbw_colorimetric_lut(int grid_n,
                                  RgbwLutInterp interp) FL_NOEXCEPT;
bool enable_rgbw_colorimetric_lut(int grid_n) FL_NOEXCEPT;

// Free the LUT and revert colorimetric calls to the closed-form solver.
void disable_rgbw_colorimetric_lut() FL_NOEXCEPT;

// Returns true if the LUT path is currently active.
bool rgbw_colorimetric_lut_enabled() FL_NOEXCEPT;

// Compile-time memory-cost accessor. Mirrors the storage math used by the
// LUT builder so users can size their RAM/flash budget before calling
// enable_rgbw_colorimetric_lut(). The runtime API clamps grid_n to [4, 256],
// so this helper applies the same clamp so callers do not under-estimate
// for grid_n < 4 or over-estimate for grid_n > 256. Returns 0 for grid_n < 1.
constexpr unsigned long rgbw_colorimetric_lut_memory_bytes(
    int grid_n, RgbwLutInterp interp) FL_NOEXCEPT {
    return (grid_n < 1)
        ? 0UL
        : (static_cast<unsigned long>(
                  grid_n < 4 ? 4 : (grid_n > 256 ? 256 : grid_n))
            * static_cast<unsigned long>(
                  grid_n < 4 ? 4 : (grid_n > 256 ? 256 : grid_n))
            * (interp == RgbwLutInterp::Hermite ? 24UL : 8UL));
}

enum {
    kRGBWDefaultColorTemp = 6000,
};

struct Rgbw {
    explicit Rgbw(u16 white_color_temp = fl::kRGBWDefaultColorTemp,
                  fl::RGBW_MODE rgbw_mode = fl::RGBW_MODE::kRGBWExactColors,
                  fl::EOrderW _w_placement = EOrderW::WDefault)
 FL_NOEXCEPT : white_color_temp(white_color_temp), w_placement(_w_placement),
          rgbw_mode(rgbw_mode) {}
    u16 white_color_temp = kRGBWDefaultColorTemp;
    fl::EOrderW w_placement = EOrderW::WDefault;
    RGBW_MODE rgbw_mode = RGBW_MODE::kRGBWExactColors;
    FASTLED_FORCE_INLINE bool active() const FL_NOEXCEPT {
        return rgbw_mode != RGBW_MODE::kRGBWInvalid;
    }

    static u32 size_as_rgb(u32 num_of_rgbw_pixels) FL_NOEXCEPT {
        // The ObjectFLED controller expects the raw pixel byte data in
        // multiples of 3. In the case of src data not a multiple of 3, then we
        // need to add pad bytes so that the delegate controller doesn't walk
        // off the end of the array and invoke a buffer overflow panic.
        num_of_rgbw_pixels = (num_of_rgbw_pixels * 4 + 2) / 3;
        u32 extra = num_of_rgbw_pixels % 3 ? 1 : 0;
        num_of_rgbw_pixels += extra;
        return num_of_rgbw_pixels;
    }
};

struct RgbwInvalid : public Rgbw {
    RgbwInvalid() FL_NOEXCEPT {
        white_color_temp = kRGBWDefaultColorTemp;
        rgbw_mode = RGBW_MODE::kRGBWInvalid;
    }
    static Rgbw value() FL_NOEXCEPT {
        RgbwInvalid invalid;
        return invalid;
    }
};

struct RgbwDefault : public Rgbw {
    RgbwDefault() FL_NOEXCEPT {
        white_color_temp = kRGBWDefaultColorTemp;
        rgbw_mode = RGBW_MODE::kRGBWExactColors;
    }
    static Rgbw value() FL_NOEXCEPT {
        RgbwDefault _default;
        return _default;
    }
};

struct RgbwWhiteIsOff : public Rgbw {
    RgbwWhiteIsOff() FL_NOEXCEPT {
        white_color_temp = kRGBWDefaultColorTemp;
        rgbw_mode = RGBW_MODE::kRGBWNullWhitePixel;
    }
    static Rgbw value() FL_NOEXCEPT {
        RgbwWhiteIsOff _default;
        return _default;
    }
};

typedef void (*rgb_2_rgbw_function)(u16 w_color_temperature, u8 r,
                                    u8 g, u8 b, u8 r_scale,
                                    u8 g_scale, u8 b_scale,
                                    u8 *out_r, u8 *out_g,
                                    u8 *out_b, u8 *out_w);

/// @brief Converts RGB to RGBW using a color transfer method
/// from saturated color channels to white. This is designed to produce
/// the most accurate white point for a given color temperature and
/// reduces power usage of the chip since a white led is much more efficient
/// than three color channels of the same power mixing together. However
/// the pixel will never achieve full brightness since the white channel is
/// 3x more efficient than the color channels mixed together, so in this mode
/// the max brightness of a given pixel is reduced.
///
/// ```
/// RGB(255, 255, 255) -> RGBW(0, 0, 0, 85)
/// RGB(255, 0, 0) -> RGBW(255, 0, 0, 0)
/// ```
void rgb_2_rgbw_exact(u16 w_color_temperature, u8 r, u8 g,
                      u8 b, u8 r_scale, u8 g_scale,
                      u8 b_scale, u8 *out_r, u8 *out_g,
                      u8 *out_b, u8 *out_w) FL_NOEXCEPT;

/// The minimum brigthness of the RGB channels is used to set the W channel.
/// This will allow the max brightness of the led chipset to be used. However
/// the leds will appear over-desaturated in this mode.
///
/// ```
/// RGB(255, 255, 255) -> RGBW(255, 255, 255, 255)
/// RGB(1, 0, 0) -> RGBW(1, 0, 0, 1)
/// ```
void rgb_2_rgbw_max_brightness(u16 w_color_temperature, u8 r,
                               u8 g, u8 b, u8 r_scale,
                               u8 g_scale, u8 b_scale, u8 *out_r,
                               u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT;

/// @brief Converts RGB to RGBW with the W channel set to black, always.
///
/// ```
/// RGB(255, 255, 255) -> RGBW(255, 255, 255, 0)
/// ```
void rgb_2_rgbw_null_white_pixel(u16 w_color_temperature, u8 r,
                                 u8 g, u8 b, u8 r_scale,
                                 u8 g_scale, u8 b_scale,
                                 u8 *out_r, u8 *out_g, u8 *out_b,
                                 u8 *out_w) FL_NOEXCEPT;

/// @brief Converts RGB to RGBW with a boosted white channel.
void rgb_2_rgbw_white_boosted(u16 w_color_temperature, u8 r,
                              u8 g, u8 b, u8 r_scale,
                              u8 g_scale, u8 b_scale, u8 *out_r,
                              u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT;

void rgb_2_rgbw_user_function(u16 w_color_temperature, u8 r,
                              u8 g, u8 b, u8 r_scale,
                              u8 g_scale, u8 b_scale, u8 *out_r,
                              u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT;

// Strict sub-gamut colorimetric solver (gist sec 5, issue #2545). Maps the
// input through the diode chromaticity model; routes the chromaticity to one
// of the RGW / RBW / BGW sub-gamuts and solves the 3x3 system. Color-accurate
// but uses fewer than all 4 channels per pixel. Requires
// FASTLED_RGBW_COLORIMETRIC; stub falls back to rgb_2_rgbw_exact + warn-once
// otherwise.
void rgb_2_rgbw_colorimetric(u16 w_color_temperature, u8 r,
                             u8 g, u8 b, u8 r_scale,
                             u8 g_scale, u8 b_scale, u8 *out_r,
                             u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT;

// White-overdrive colorimetric solver (wx_lp_legacy from gist sec 9).
// Closed-form scalar LP: maximize W subject to RGB residual >= 0. Brighter
// than kRGBWColorimetric for in-gamut targets, same color accuracy. Requires
// FASTLED_RGBW_COLORIMETRIC; stub falls back to rgb_2_rgbw_exact + warn-once
// otherwise.
void rgb_2_rgbw_colorimetric_boosted(u16 w_color_temperature, u8 r,
                                     u8 g, u8 b, u8 r_scale,
                                     u8 g_scale, u8 b_scale, u8 *out_r,
                                     u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT;

void set_rgb_2_rgbw_function(rgb_2_rgbw_function func) FL_NOEXCEPT;

/// @brief   Converts RGB to RGBW using one of the functions.
/// @details Dynamic version of the rgb_w_rgbw function with less chance for
///          the compiler to optimize.
FASTLED_FORCE_INLINE void
rgb_2_rgbw(RGBW_MODE mode, u16 w_color_temperature, u8 r, u8 g,
           u8 b, u8 r_scale, u8 g_scale, u8 b_scale,
           u8 *out_r, u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT {
    switch (mode) {
    case RGBW_MODE::kRGBWInvalid:
    case RGBW_MODE::kRGBWNullWhitePixel:
        rgb_2_rgbw_null_white_pixel(w_color_temperature, r, g, b, r_scale,
                                    g_scale, b_scale, out_r, out_g, out_b,
                                    out_w);
        return;
    case RGBW_MODE::kRGBWExactColors:
        rgb_2_rgbw_exact(w_color_temperature, r, g, b, r_scale, g_scale,
                         b_scale, out_r, out_g, out_b, out_w);
        return;
    case RGBW_MODE::kRGBWBoostedWhite:
        rgb_2_rgbw_white_boosted(w_color_temperature, r, g, b, r_scale, g_scale,
                                 b_scale, out_r, out_g, out_b, out_w);
        return;
    case RGBW_MODE::kRGBWMaxBrightness:
        rgb_2_rgbw_max_brightness(w_color_temperature, r, g, b, r_scale,
                                  g_scale, b_scale, out_r, out_g, out_b, out_w);
        return;
    case RGBW_MODE::kRGBWUserFunction:
        rgb_2_rgbw_user_function(w_color_temperature, r, g, b, r_scale, g_scale,
                                 b_scale, out_r, out_g, out_b, out_w);
        return;
    case RGBW_MODE::kRGBWColorimetric:
        rgb_2_rgbw_colorimetric(w_color_temperature, r, g, b, r_scale, g_scale,
                                b_scale, out_r, out_g, out_b, out_w);
        return;
    case RGBW_MODE::kRGBWColorimetricBoosted:
        rgb_2_rgbw_colorimetric_boosted(w_color_temperature, r, g, b, r_scale,
                                        g_scale, b_scale, out_r, out_g, out_b,
                                        out_w);
        return;
    }
    rgb_2_rgbw_null_white_pixel(w_color_temperature, r, g, b, r_scale, g_scale,
                                b_scale, out_r, out_g, out_b, out_w);
}

// @brief Converts RGB to RGBW using one of the functions.
template <RGBW_MODE MODE>
FASTLED_FORCE_INLINE void
rgb_2_rgbw(u16 w_color_temperature, u8 r, u8 g, u8 b,
           u8 r_scale, u8 g_scale, u8 b_scale, u8 *out_r,
           u8 *out_g, u8 *out_b, u8 *out_w) FL_NOEXCEPT {
    // We trust that the compiler will inline all of this.
    rgb_2_rgbw(MODE, w_color_temperature, r, g, b, r_scale, g_scale, b_scale,
               out_r, out_g, out_b, out_w);
}

// Assuming all RGB pixels are already ordered in native led ordering, then this
// function will reorder them so that white is also the correct position.
// b0-b2 are actually rgb that are already in native LED order.
// and out_b0-out_b3 are the output RGBW in native LED chipset order.
// w is the white component that needs to be inserted into the RGB data at
// the correct position.
void rgbw_partial_reorder(fl::EOrderW w_placement, u8 b0, u8 b1,
                          u8 b2, u8 w, u8 *out_b0,
                          u8 *out_b1, u8 *out_b2, u8 *out_b3) FL_NOEXCEPT;

} // namespace fl

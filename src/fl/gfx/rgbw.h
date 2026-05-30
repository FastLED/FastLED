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
};

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

// Enable the 2D + 1D factored LUT path (issue #2545 Phase 2). When enabled,
// the colorimetric modes use a bilinear-interpolated LUT instead of solving
// per pixel. Available whenever FASTLED_RGBW_COLORIMETRIC=1 — no separate
// FASTLED_RGBW_COLORIMETRIC_LUT flag exists; gc-sections drops the LUT path
// for sketches that never call this. `grid_n` controls the LUT edge length
// (8 KB at N=32, ~2 KB at N=16, ~32 KB at N=64). LUT is rebuilt whenever
// the active profile or CCT changes. Returns false on allocation failure
// (or always false when FASTLED_RGBW_COLORIMETRIC is undefined).
bool enable_rgbw_colorimetric_lut(int grid_n) FL_NOEXCEPT;

// Free the LUT and revert colorimetric calls to the closed-form solver.
void disable_rgbw_colorimetric_lut() FL_NOEXCEPT;

// Returns true if the LUT path is currently active.
bool rgbw_colorimetric_lut_enabled() FL_NOEXCEPT;

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

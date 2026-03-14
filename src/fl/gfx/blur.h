#pragma once

#include "fl/gfx/canvas.h"
#include "fl/stl/int.h"
#include "fl/stl/span.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/type_traits.h"

namespace fl {
namespace gfx {

/// @defgroup ColorBlurs Color Blurring Functions
/// Functions for blurring colors
/// @{

/// One-dimensional blur filter (span version).
/// Spreads light to 2 line neighbors.
///   * 0 = no spread at all
///   * 64 = moderate spreading
///   * 172 = maximum smooth, even spreading
///   * 173..255 = wider spreading, but increasing flicker
///
/// Total light is NOT entirely conserved, so many repeated
/// calls to 'blur' will also result in the light fading,
/// eventually all the way to black; this is by design so that
/// it can be used to (slowly) clear the LEDs to black.
/// @param leds span of LEDs to blur
/// @param blur_amount the amount of blur to apply
void blur1d(fl::span<CRGB> leds, fract8 blur_amount);

/// Legacy raw-pointer version of blur1d.
inline void blur1d(CRGB *leds, u16 numLeds, fract8 blur_amount) {
    blur1d(fl::span<CRGB>(leds, numLeds), blur_amount);
}

/// Two-dimensional blur filter (span version).
/// Spreads light to 8 XY neighbors.
///   * 0 = no spread at all
///   * 64 = moderate spreading
///   * 172 = maximum smooth, even spreading
///   * 173..255 = wider spreading, but increasing flicker
///
/// Total light is NOT entirely conserved, so many repeated
/// calls to 'blur' will also result in the light fading,
/// eventually all the way to black; this is by design so that
/// it can be used to (slowly) clear the LEDs to black.
/// @param leds span of LEDs to blur
/// @param width the width of the matrix
/// @param height the height of the matrix
/// @param blur_amount the amount of blur to apply
void blur2d(fl::span<CRGB> leds, u8 width, u8 height, fract8 blur_amount,
            const XYMap &xymap);

/// Legacy raw-pointer version of blur2d.
inline void blur2d(CRGB *leds, u8 width, u8 height, fract8 blur_amount,
                   const XYMap &xymap) {
    blur2d(fl::span<CRGB>(leds, u16(width) * u16(height)), width, height,
           blur_amount, xymap);
}

/// Legacy version of blur2d, which does not require an XYMap but instead
/// implicitly binds to XY() function. If you are hitting a linker error here,
/// then use blur2d(..., const fl::XYMap& xymap) instead.
void blur2d(CRGB *leds, u8 width, u8 height, fract8 blur_amount)
    FASTLED_DEPRECATED("Use blur2d(..., const XYMap& xymap) instead");

/// Perform a blur1d() on every row of a rectangular matrix (span version).
/// @see blur1d()
/// @param leds span of LEDs to blur
/// @param width the width of the matrix
/// @param height the height of the matrix
/// @param blur_amount the amount of blur to apply
void blurRows(fl::span<CRGB> leds, u8 width, u8 height, fract8 blur_amount,
              const XYMap &xymap);

/// Legacy raw-pointer version of blurRows.
inline void blurRows(CRGB *leds, u8 width, u8 height, fract8 blur_amount,
                     const XYMap &xymap) {
    blurRows(fl::span<CRGB>(leds, u16(width) * u16(height)), width, height,
             blur_amount, xymap);
}

/// Perform a blur1d() on every column of a rectangular matrix (span version).
/// @copydetails blurRows()
void blurColumns(fl::span<CRGB> leds, u8 width, u8 height, fract8 blur_amount,
                 const XYMap &xymap);

/// Legacy raw-pointer version of blurColumns.
inline void blurColumns(CRGB *leds, u8 width, u8 height, fract8 blur_amount,
                        const XYMap &xymap) {
    blurColumns(fl::span<CRGB>(leds, u16(width) * u16(height)), width, height,
                blur_amount, xymap);
}

/// Two-dimensional blur filter (Canvas version, no XYMap).
/// Uses direct rectangular indexing for cache-coherent access.
/// @param canvas the canvas to blur in-place
/// @param blur_amount alpha8 blur amount
void blur2d(Canvas<CRGB> &canvas, alpha8 blur_amount);

/// Perform a blur1d() on every row of a rectangular matrix (Canvas version).
/// @param canvas the canvas to blur in-place
/// @param blur_amount alpha8 blur amount
void blurRows(Canvas<CRGB> &canvas, alpha8 blur_amount);

/// Perform a blur1d() on every column of a rectangular matrix (Canvas version).
/// @param canvas the canvas to blur in-place
/// @param blur_amount alpha8 blur amount
void blurColumns(Canvas<CRGB> &canvas, alpha8 blur_amount);

/// @} ColorBlurs

/// @brief Compile-time Gaussian blur with independent H/V radii.
/// @tparam hRadius Horizontal blur radius (0–4). Kernel width = 2*hRadius+1.
/// @tparam vRadius Vertical blur radius (0–4). Kernel height = 2*vRadius+1.
/// @tparam RGB_T Pixel type (must have .r, .g, .b members and typedef fp).
/// @param canvas The canvas to blur in-place.
/// @param dimFactor UNORM8 brightness scale [0, 255] where 255 = no dim.
template <int hRadius, int vRadius, typename RGB_T>
void blurGaussian(Canvas<RGB_T> &canvas, alpha8 dimFactor);

/// @brief Higher-precision dim overload (UNORM16).
/// @param dimFactor UNORM16 brightness scale [0, 65535] where 65535 = no dim.
template <int hRadius, int vRadius, typename RGB_T>
void blurGaussian(Canvas<RGB_T> &canvas, alpha16 dimFactor);

/// @brief Convenience overload with no dimming.
template <int hRadius, int vRadius, typename RGB_T>
inline void blurGaussian(Canvas<RGB_T> &canvas) {
    blurGaussian<hRadius, vRadius>(canvas, alpha8(255));
}

} // namespace gfx

// Span-based bindings — forward fl::blur* to fl::gfx::blur*
inline void blur1d(fl::span<CRGB> leds, fract8 blur_amount) {
    gfx::blur1d(leds, blur_amount);
}
inline void blur2d(fl::span<CRGB> leds, u8 width, u8 height,
                   fract8 blur_amount, const XYMap &xymap) {
    gfx::blur2d(leds, width, height, blur_amount, xymap);
}
inline void blurRows(fl::span<CRGB> leds, u8 width, u8 height,
                     fract8 blur_amount, const XYMap &xymap) {
    gfx::blurRows(leds, width, height, blur_amount, xymap);
}
inline void blurColumns(fl::span<CRGB> leds, u8 width, u8 height,
                        fract8 blur_amount, const XYMap &xymap) {
    gfx::blurColumns(leds, width, height, blur_amount, xymap);
}

// Canvas-based bindings — forward fl::blur* to fl::gfx::blur*
inline void blur2d(gfx::Canvas<CRGB> &canvas, alpha8 blur_amount) {
    gfx::blur2d(canvas, blur_amount);
}
inline void blurRows(gfx::Canvas<CRGB> &canvas, alpha8 blur_amount) {
    gfx::blurRows(canvas, blur_amount);
}
inline void blurColumns(gfx::Canvas<CRGB> &canvas, alpha8 blur_amount) {
    gfx::blurColumns(canvas, blur_amount);
}

// Legacy raw-pointer bindings — forward fl::blur* to fl::gfx::blur*
inline void blur1d(CRGB *leds, u16 numLeds, fract8 blur_amount) {
    gfx::blur1d(leds, numLeds, blur_amount);
}
inline void blur2d(CRGB *leds, u8 width, u8 height, fract8 blur_amount,
                   const XYMap &xymap) {
    gfx::blur2d(leds, width, height, blur_amount, xymap);
}
FASTLED_DEPRECATED("Use blur2d(..., const XYMap& xymap) instead")
inline void blur2d(CRGB *leds, u8 width, u8 height, fract8 blur_amount) {
    gfx::blur2d(leds, width, height, blur_amount);
}
inline void blurRows(CRGB *leds, u8 width, u8 height, fract8 blur_amount,
                     const XYMap &xymap) {
    gfx::blurRows(leds, width, height, blur_amount, xymap);
}
inline void blurColumns(CRGB *leds, u8 width, u8 height, fract8 blur_amount,
                        const XYMap &xymap) {
    gfx::blurColumns(leds, width, height, blur_amount, xymap);
}

} // namespace fl

/// @file    scale_up.h
/// @brief   Expands a grid using bilinear interpolation for upscaling effects
///
/// This effect wrapper allows rendering at a lower resolution and then scaling up
/// to the display resolution using bilinear interpolation. This is useful for:
/// - Underpowered devices that can't handle full resolution rendering
/// - Dynamic resolution scaling when grid size changes
/// - Optimizing performance by rendering complex effects at lower resolution

#pragma once

#include "fl/stl/stdint.h"

#include "crgb.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/xymap.h"       // Needed for constructor parameter
#include "fl/fx/fx2d.h"


/// @name ScaleUp Precision Modes
/// @{
/// These constants configure the scaling algorithm's precision and performance tradeoffs.
/// Set FASTLED_SCALE_UP to one of these values to control behavior.
///
/// @note To override FASTLED_SCALE_UP, it must be defined via build system flags (e.g.,
///       platformio.ini with `build_flags = -DFASTLED_SCALE_UP=0`). Including it in source
///       files will not work as this header will already be included. This optimization is
///       only necessary for sketches at maximum memory capacity; normal usage does not
///       require overriding the default DECIDE_AT_RUNTIME mode.

/// @brief Optimized for power-of-2 grid sizes (fastest, lowest memory)
/// Uses bit-shifting operations for division. Ideal for AVR and other memory-constrained
/// devices when grid dimensions are powers of 2 (e.g., 8x8, 16x16, 32x32).
#define FASTLED_SCALE_UP_ALWAYS_POWER_OF_2 0

/// @brief High-precision mode for arbitrary grid sizes
/// Uses more memory than POWER_OF_2 mode but handles any grid dimensions correctly.
/// Suitable for most modern devices with non-power-of-2 grids.
#define FASTLED_SCALE_UP_HIGH_PRECISION 1

/// @brief Runtime selection based on grid size (adaptive)
/// Uses more executable memory as both POWER_OF_2 and HIGH_PRECISION implementations
/// are compiled in. Automatically selects the faster power-of-2 algorithm when applicable,
/// otherwise falls back to high-precision mode.
#define FASTLED_SCALE_UP_DECIDE_AT_RUNTIME 2

/// @brief Floating-point implementation (slowest, not recommended)
/// Provided for completeness but offers no performance benefits over integer versions.
/// Avoid using this mode in production.
#define FASTLED_SCALE_UP_FORCE_FLOATING_POINT 3

/// @}

#ifndef FASTLED_SCALE_UP
#define FASTLED_SCALE_UP FASTLED_SCALE_UP_DECIDE_AT_RUNTIME
#endif

namespace fl {

FASTLED_SHARED_PTR(ScaleUp);

/// @brief Effect wrapper that upscales delegate effects using bilinear interpolation
///
/// ScaleUp renders a delegate effect at a lower resolution and then scales the result
/// up to the target display size using bilinear filtering. This creates smooth interpolation
/// between pixels, making the upscaling less noticeable than nearest-neighbor scaling.
///
/// @note The delegate effect is rendered at the lower resolution defined by the XYMap
///       passed to the constructor, then expanded to the display resolution.
class ScaleUp : public Fx2d {
  public:
    /// @brief Construct a ScaleUp effect wrapper
    /// @param xymap The XYMap defining the low-resolution render target
    /// @param fx The delegate effect to render at lower resolution
    ScaleUp(const XYMap& xymap, Fx2dPtr fx);

    /// @brief Render the effect by drawing delegate at low-res and scaling up
    /// @param context Drawing context containing the target XYMap and frame buffer
    void draw(DrawContext context) override;

    fl::string fxName() const override { return "scale_up"; }

    /// @brief Expand a low-resolution buffer to high-resolution using bilinear interpolation
    ///
    /// Performs bilinear filtering to smoothly upscale pixel data from a source buffer to a
    /// destination buffer. Each output pixel is calculated by interpolating the four nearest
    /// input pixels, creating smooth transitions and reducing blocky artifacts.
    ///
    /// The actual interpolation algorithm used depends on the FASTLED_SCALE_UP compile-time
    /// setting:
    /// - POWER_OF_2: Fast bit-shift version (requires power-of-2 dimensions)
    /// - HIGH_PRECISION: Integer math for arbitrary dimensions
    /// - DECIDE_AT_RUNTIME: Automatically selects based on input dimensions
    /// - FORCE_FLOATING_POINT: Floating-point math (slowest, not recommended)
    ///
    /// @note This method is exposed primarily for unit testing and is not part of the public API.
    ///       Normal usage should go through draw() which handles buffer management automatically.
    ///
    /// @param input Source buffer containing low-resolution pixel data
    /// @param output Destination buffer for high-resolution output (must be pre-allocated)
    /// @param width Width of the low-resolution input buffer
    /// @param height Height of the low-resolution input buffer
    /// @param mXyMap The target high-resolution XYMap defining output dimensions and layout
    void expand(const CRGB *input, CRGB *output, uint16_t width,
                uint16_t height, const XYMap& mXyMap);

  private:
    /// @brief Direct copy without expansion (used when resolutions match)
    /// @param input Source buffer
    /// @param output Destination buffer
    /// @param width Buffer width
    /// @param height Buffer height
    void noExpand(const CRGB *input, CRGB *output, uint16_t width,
                  uint16_t height);

    Fx2dPtr mDelegate;  ///< The wrapped effect that renders at low resolution
    fl::vector<CRGB, fl::allocator_psram<CRGB>> mSurface;  ///< Low-resolution render buffer
};

} // namespace fl

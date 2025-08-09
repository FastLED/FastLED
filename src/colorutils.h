#pragma once

/*
Legacy header. Prefer to use fl/colorutils.h instead since
*/
#include "fl/colorutils.h"

using fl::fadeLightBy;
using fl::fade_video;
using fl::fadeToBlackBy;
using fl::nscale8_video;
using fl::fade_raw;
using fl::nscale8;
using fl::fadeUsingColor;
using fl::blend;
using fl::CRGBPalette16;
using fl::CRGBPalette32;
using fl::CRGBPalette256;
using fl::CHSVPalette16;
using fl::CHSVPalette32;
using fl::CHSVPalette256;
// using fl::TProgmemRGBPalette16;
// using fl::TProgmemHSVPalette16;
using fl::HeatColor;
using fl::TRGBGradientPaletteEntryUnion;
using fl::TDynamicRGBGradientPalette_byte;
using fl::TDynamicRGBGradientPalette_bytes;
// using fl::TProgmemRGBGradientPalette_bytes;
// using fl::TProgmemRGBGradientPalette_byte;
// using fl::TProgmemRGBPalette16;
// using fl::TProgmemRGBGradientPaletteRef;

using fl::UpscalePalette;


using fl::TDynamicRGBGradientPaletteRef;
using fl::TGradientDirectionCode;
using fl::TBlendType;
using fl::ColorFromPalette;
using fl::ColorFromPaletteExtended;
using fl::fill_palette;
using fl::fill_gradient;
using fl::fill_rainbow;
using fl::fill_rainbow_circular;
using fl::fill_solid;
using fl::fill_palette_circular;
using fl::map_data_into_colors_through_palette;
using fl::nblendPaletteTowardPalette;
using fl::napplyGamma_video;
using fl::blurColumns;
using fl::blurRows;
using fl::blur1d;
using fl::blur2d;
using fl::nblend;
using fl::applyGamma_video;

using fl::fill_gradient_RGB;
using fl::fill_gradient_HSV;

// TGradientDirectionCode values.
using fl::SHORTEST_HUES;
using fl::LONGEST_HUES;
using fl::FORWARD_HUES;
using fl::BACKWARD_HUES;

// TBlendType values.
using fl::NOBLEND;
using fl::BLEND;
using fl::LINEARBLEND;        ///< Linear interpolation between palette entries, with wrap-around from end to the beginning again
using fl::LINEARBLEND_NOWRAP;


/// Defines a static RGB palette very compactly using a series
/// of connected color gradients.
///
/// For example, if you want the first 3/4ths of the palette to be a slow
/// gradient ramping from black to red, and then the remaining 1/4 of the
/// palette to be a quicker ramp to white, you specify just three points: the
/// starting black point (at index 0), the red midpoint (at index 192),
/// and the final white point (at index 255).  It looks like this:
///   @code
///   index:  0                                    192          255
///           |----------r-r-r-rrrrrrrrRrRrRrRrRRRR-|-RRWRWWRWWW-|
///   color: (0,0,0)                           (255,0,0)    (255,255,255)
///   @endcode
///
/// Here's how you'd define that gradient palette using this macro:
///   @code{.cpp}
///   DEFINE_GRADIENT_PALETTE( black_to_red_to_white_p ) {
///        0,    0,   0,   0,  /* at index 0,   black(0,0,0) */
///       192, 255,   0,   0,  /* at index 192, red(255,0,0) */
///       255, 255, 255, 255   /* at index 255, white(255,255,255) */
///   };
///   @endcode
///
/// This format is designed for compact storage.  The example palette here
/// takes up just 12 bytes of PROGMEM (flash) storage, and zero bytes
/// of SRAM when not currently in use.
///
/// To use one of these gradient palettes, simply assign it into a
/// CRGBPalette16 or a CRGBPalette256, like this:
///   @code{.cpp}
///   CRGBPalette16 pal = black_to_red_to_white_p;
///   @endcode
///
/// When the assignment is made, the gradients are expanded out into
/// either 16 or 256 palette entries, depending on the kind of palette
/// object they're assigned to.
///
/// @warning The last "index" position **MUST** be 255! Failure to end
/// with index 255 will result in program hangs or crashes.  
/// @par
/// @warning At this point, these gradient palette definitions **MUST**
/// be stored in PROGMEM on AVR-based Arduinos. If you use the
/// `DEFINE_GRADIENT_PALETTE` macro, this is taken of automatically.
///
/// TProgmemRGBGradientPalette_byte must remain in the global namespace.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#define DEFINE_GRADIENT_PALETTE(X) \
  FL_ALIGN_PROGMEM \
  extern const TProgmemRGBGradientPalette_byte X[] FL_PROGMEM =

/// Forward-declaration macro for DEFINE_GRADIENT_PALETTE(X)
#define DECLARE_GRADIENT_PALETTE(X) \
  FL_ALIGN_PROGMEM \
  extern const TProgmemRGBGradientPalette_byte X[] FL_PROGMEM

#pragma GCC diagnostic pop

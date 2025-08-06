#pragma once

#include "fl/stdint.h"
#include "fl/int.h"
#include "fastled_progmem.h"  // For FL_ALIGN_PROGMEM

// TODO: Figure out how to namespace these.
typedef fl::u32 TProgmemRGBPalette16[16] FL_ALIGN_PROGMEM; ///< CRGBPalette16 entries stored in
///< PROGMEM memory with proper alignment for DWORD reads
typedef fl::u32 TProgmemHSVPalette16[16] FL_ALIGN_PROGMEM; ///< CHSVPalette16 entries stored in
///< PROGMEM memory with proper alignment for DWORD reads
/// Alias for TProgmemRGBPalette16
#define TProgmemPalette16 TProgmemRGBPalette16
typedef fl::u32 TProgmemRGBPalette32[32] FL_ALIGN_PROGMEM; ///< CRGBPalette32 entries stored in
///< PROGMEM memory with proper alignment for DWORD reads
typedef fl::u32 TProgmemHSVPalette32[32] FL_ALIGN_PROGMEM; ///< CHSVPalette32 entries stored in
///< PROGMEM memory with proper alignment for DWORD reads
/// Alias for TProgmemRGBPalette32
#define TProgmemPalette32 TProgmemRGBPalette32

/// Byte of an RGB gradient, stored in PROGMEM memory
typedef const fl::u8 TProgmemRGBGradientPalette_byte;
/// Pointer to bytes of an RGB gradient, stored in PROGMEM memory
/// @see DEFINE_GRADIENT_PALETTE
/// @see DECLARE_GRADIENT_PALETTE
typedef const TProgmemRGBGradientPalette_byte *TProgmemRGBGradientPalette_bytes;
/// Alias of ::TProgmemRGBGradientPalette_bytes
typedef TProgmemRGBGradientPalette_bytes TProgmemRGBGradientPaletteRef;

namespace fl {

/// Hue direction for calculating fill gradients.
/// Since "hue" is a value around a color wheel, there are always two directions
/// to sweep from one hue to another.
typedef enum {
    FORWARD_HUES,  ///< Hue always goes clockwise around the color wheel
    BACKWARD_HUES, ///< Hue always goes counter-clockwise around the color wheel
    SHORTEST_HUES, ///< Hue goes whichever way is shortest
    LONGEST_HUES   ///< Hue goes whichever way is longest
} TGradientDirectionCode;

} // namespace fl

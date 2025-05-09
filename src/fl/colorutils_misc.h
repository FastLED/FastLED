#pragma once

#include <stdint.h>

// TODO: Figure out how to namespace these.
typedef uint32_t TProgmemRGBPalette16[16]; ///< CRGBPalette16 entries stored in
                                           ///< PROGMEM memory
typedef uint32_t TProgmemHSVPalette16[16]; ///< CHSVPalette16 entries stored in
                                           ///< PROGMEM memory
/// Alias for TProgmemRGBPalette16
#define TProgmemPalette16 TProgmemRGBPalette16
typedef uint32_t TProgmemRGBPalette32[32]; ///< CRGBPalette32 entries stored in
                                           ///< PROGMEM memory
typedef uint32_t TProgmemHSVPalette32[32]; ///< CHSVPalette32 entries stored in
                                           ///< PROGMEM memory
/// Alias for TProgmemRGBPalette32
#define TProgmemPalette32 TProgmemRGBPalette32

/// Byte of an RGB gradient, stored in PROGMEM memory
typedef const uint8_t TProgmemRGBGradientPalette_byte;
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

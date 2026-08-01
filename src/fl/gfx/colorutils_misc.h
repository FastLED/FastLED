#pragma once

#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/int.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"  // IWYU pragma: keep

// TODO: Figure out how to namespace these.
typedef fl::u32 TProgmemRGBPalette16[16]; ///< CRGBPalette16 entries stored in
                                           ///< PROGMEM memory
/// CHSVPalette16 entries stored in PROGMEM memory.
///
/// A distinct struct rather than another `fl::u32[16]` alias. When this was an
/// array typedef it was *the same type* as TProgmemRGBPalette16 -- the names
/// documented an intent the compiler could not enforce -- so an RGB progmem
/// palette bound silently to every CHSV palette constructor and
/// `loadProgmemPalette(CHSV*, ...)` copied red/green/blue straight into
/// hue/sat/val. Reported as FastLED#807 in 2019.
///
/// The implicit conversion keeps every legitimate use working unchanged:
/// brace initialisation, `pal[i]` indexing, and passing one where a
/// `const fl::u32 *` is expected. Only the unsafe direction is now rejected,
/// because a bare `fl::u32[16]` will no longer convert *to* this type.
struct TProgmemHSVPalette16 {
    fl::u32 entries[16];
    operator const fl::u32 *() const FL_NO_EXCEPT { return entries; }
};
/// Alias for TProgmemRGBPalette16
#define TProgmemPalette16 TProgmemRGBPalette16
typedef fl::u32 TProgmemRGBPalette32[32]; ///< CRGBPalette32 entries stored in
                                           ///< PROGMEM memory
/// CHSVPalette32 entries stored in PROGMEM memory.
/// @copydetails TProgmemHSVPalette16
struct TProgmemHSVPalette32 {
    fl::u32 entries[32];
    operator const fl::u32 *() const FL_NO_EXCEPT { return entries; }
};
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

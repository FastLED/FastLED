#pragma once

/// @file pixel_fwd.h
/// Forward declarations for the core pixel types.
///
/// CRGB and hsv8 (CHSV) live in their own namespace, fl::pixel, and are
/// re-exported into fl (and into the global namespace by crgb.h / chsv.h).
/// Argument-dependent lookup on a pixel type therefore searches only
/// fl::pixel, which holds just the pixel structs and their operators, never
/// fl's generic templates (fl::swap, fl::sort, fl::move, fl::memcpy, ...).
/// Without this, legacy calls such as `memset(leds, 0, n)`,
/// `std::reverse(leds, leds + n)` or `free(leds)` were ambiguous between the
/// libc/std function and its fl:: counterpart (FastLED#4827).
///
/// Forward-declare pixel types by including this header, not by writing
/// `namespace fl { struct CRGB; }`, which conflicts with the using-declaration.

namespace fl {
namespace pixel {
struct CRGB;
struct hsv8;
}  // namespace pixel

using pixel::CRGB;  // ok bare using
using pixel::hsv8;  // ok bare using
}  // namespace fl

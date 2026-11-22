#pragma once
// IWYU pragma: private
// ok no namespace fl - platform capability macros only.

#include "platforms/is_platform.h"

// Internal storage policy, not a sketch setting. A public controller must have
// the same layout in the library and sketch even when the sketch overrides its
// memory tier or NO_CORRECTION before including FastLED.
// Keep storage on the measured ESP32 path and host/WASM builds; physical small
// targets retain their original controller size. Feature flags can disable use
// of the cache, but cannot remove this storage on a capable platform.
#if defined(FL_IS_ESP32) || defined(FL_IS_STUB) || defined(FL_IS_WASM)
#define FL_CONTROLLER_HAS_COLOR_ADJUSTMENT_CACHE
#endif

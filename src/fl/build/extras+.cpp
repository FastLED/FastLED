/// @file extras+.cpp
/// @brief Unity build: includes from src/extras/
///
/// `src/extras/` collects opt-in shims that emit code only when a
/// specific build-flag macro is defined. Including this aggregator
/// from the library's unity build is always safe — each shim's body
/// is gated on its own opt-in macro and reduces to nothing when the
/// flag is unset.

// Marks FastLED's own translation units. Sketch-side startup symbols
// (esp32_lazy_serial0_main.h) must not be emitted here (FastLED#5365).
#define FL_BUILDING_FASTLED_LIBRARY 1

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "extras/_build.cpp.hpp"

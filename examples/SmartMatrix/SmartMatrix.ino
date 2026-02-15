// @filter: (platform is teensy) and (memory is high)

/// @file    SmartMatrix.ino
/// @brief   SmartMatrix example with platform detection
/// @example SmartMatrix.ino

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"
#include "fl/has_include.h"

#if FL_HAS_INCLUDE("SmartMatrix.h")
#include "SmartMatrixSketch.h"
#else
void setup() {
}
void loop() {
}
#endif  // FL_HAS_INCLUDE("SmartMatrix.h")

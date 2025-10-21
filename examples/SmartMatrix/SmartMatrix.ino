// @filter: (platform is teensy) and (memory is high)

/// @file    SmartMatrix.ino
/// @brief   SmartMatrix example with platform detection
/// @example SmartMatrix.ino

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#if __has_include("SmartMatrix.h")
#include "SmartMatrixSketch.h"
#else
void setup() {
}
void loop() {
}
#endif  // __has_include("SmartMatrix.h")

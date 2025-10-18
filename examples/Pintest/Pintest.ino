// @filter: ((platform is avr) or (platform is teensy)) and not (platform is attiny88) and not (platform is attinyxy4) and not (platform is attinyxy6)

/// @file    Pintest.ino
/// @brief   Pintest example with platform detection
/// @example Pintest.ino

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "Pintest.h"

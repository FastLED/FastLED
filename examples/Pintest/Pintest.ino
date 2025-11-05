// @filter: ((platform is avr) and not (board is attiny*) and not (board is atmega8*)) or (platform is teensy)

/// @file    Pintest.ino
/// @brief   Pintest example with platform detection
/// @example Pintest.ino

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "Pintest.h"

/// @file    Pintest.ino
/// @brief   Pintest example with platform detection
/// @example Pintest.ino

// Platform detection logic
#if !defined(__AVR_ATtiny88__) && !defined(ARDUINO_attinyxy4) && !defined(ARDUINO_attinyxy6) && (defined(__AVR__) || (defined(__arm__) && defined(TEENSYDUINO)))
// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "Pintest.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

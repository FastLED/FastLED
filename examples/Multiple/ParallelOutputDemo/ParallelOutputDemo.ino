/// @file    ParallelOutputDemo.ino
/// @brief   ParallelOutputDemo example with platform detection
/// @example ParallelOutputDemo.ino

// Platform detection logic
#if defined(CORE_TEENSY) && defined(TEENSYDUINO) && defined(HAS_PORTDC)
// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "ParallelOutputDemo.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

/// @file    PJRCSpectrumAnalyzer.ino
/// @brief   PJRCSpectrumAnalyzer example with platform detection
/// @example PJRCSpectrumAnalyzer.ino

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "fl/sketch_macros.h"

// Platform detection logic
// Requires lots of memory for 60x32 LED matrix + Audio library + FFT
// SKETCH_HAS_LOTS_OF_MEMORY = 0 for Teensy 3.0/3.1/3.2 (all have insufficient RAM)
#if defined(__arm__) && defined(TEENSYDUINO) && SKETCH_HAS_LOTS_OF_MEMORY
#include "PJRCSpectrumAnalyzer.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

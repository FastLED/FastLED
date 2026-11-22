/// @file fl.fs+.cpp
/// @brief Unity build entry-point for the filesystem subsystem.
///
/// Storage backends are deliberately NOT here -- `fl.fs.sd+.cpp` and
/// `fl.fs.embedded+.cpp` carry those, so a sketch that never mounts a
/// given medium does not link its driver chain.

// Marks FastLED's own translation units. Sketch-side startup symbols
// (esp32_lazy_serial0_main.h) must not be emitted here (FastLED#5365).
#define FL_BUILDING_FASTLED_LIBRARY 1

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/fs/_build.cpp.hpp"

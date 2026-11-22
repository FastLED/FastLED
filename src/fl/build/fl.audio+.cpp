/// @file fl.audio+.cpp
/// @brief Unity build: recursive includes from src/fl/audio/

// Marks FastLED's own translation units. Sketch-side startup symbols
// (esp32_lazy_serial0_main.h) must not be emitted here (FastLED#5365).
#define FL_BUILDING_FASTLED_LIBRARY 1

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/audio/_build.cpp.hpp"

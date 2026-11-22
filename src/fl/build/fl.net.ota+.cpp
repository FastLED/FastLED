/// @file fl.net.ota+.cpp
/// @brief Unity build entry-point for the `fl::net::OTA` wrapper, split out of
/// `fl.net+.cpp` (which every ESP32 sketch links) so the wrapper's reference
/// to the platform OTA backend exists only when a sketch uses OTA
/// (FastLED #4726).

// Marks FastLED's own translation units. Sketch-side startup symbols
// (esp32_lazy_serial0_main.h) must not be emitted here (FastLED#5365).
#define FL_BUILDING_FASTLED_LIBRARY 1

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/net/ota/_build.cpp.hpp"

/// @file fl.net.ota+.cpp
/// @brief Unity build entry-point for the `fl::net::OTA` wrapper, split out of
/// `fl.net+.cpp` (which every ESP32 sketch links) so the wrapper's reference
/// to the platform OTA backend exists only when a sketch uses OTA
/// (FastLED #4726).

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/net/ota/_build.cpp.hpp"

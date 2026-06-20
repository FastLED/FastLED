/// @file fl.fled.load_channels+.cpp
/// @brief Unity build entry-point for CFastLED::loadChannels, split out
/// of `fl.fled+.cpp` so the linker can drop the TU when the sketch never
/// calls `FastLED.loadChannels(...)`. See FastLED #3311 PR5.

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/fled/load_channels/_build.cpp.hpp"

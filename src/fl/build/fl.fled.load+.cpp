/// @file fl.fled.load+.cpp
/// @brief Unity build entry-point for CFastLED::load, split out of
/// `fl.fled+.cpp` so the linker can drop the whole load() path (load +
/// loadChannels + loadScreenMap + FledDispatcher) when the sketch never
/// calls `FastLED.load(...)`. See FastLED #3311 PR5.

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/fled/load/_build.cpp.hpp"

/// @file fl/fled/load_channels/_build.cpp.hpp
/// @brief Unity-build aggregate for CFastLED::loadChannels. Pulled in only
/// by `src/fl/build/fl.fled.load_channels+.cpp` so the linker can drop the
/// whole TU when nobody calls `FastLED.loadChannels(...)`.

#include "fl/fled/load_channels/fastled_load_channels.cpp.hpp"

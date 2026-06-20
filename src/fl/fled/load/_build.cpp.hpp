/// @file fl/fled/load/_build.cpp.hpp
/// @brief Unity-build aggregate for CFastLED::load. Pulled in only by
/// `src/fl/build/fl.fled.load+.cpp` so the linker can drop the whole TU
/// when nobody calls `FastLED.load(...)`. Note that load() composes
/// loadChannels + loadScreenMap + script dispatch, so any sketch that
/// links this TU also pulls those siblings via their normal references.

#include "fl/fled/load/fastled_load.cpp.hpp"

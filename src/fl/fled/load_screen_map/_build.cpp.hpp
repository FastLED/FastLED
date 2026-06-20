/// @file fl/fled/load_screen_map/_build.cpp.hpp
/// @brief Unity-build aggregate for CFastLED::loadScreenMap. Pulled in
/// only by `src/fl/build/fl.fled.load_screen_map+.cpp` so the linker can
/// drop the whole TU when nobody calls `FastLED.loadScreenMap(...)`.

#include "fl/fled/load_screen_map/fastled_load_screen_map.cpp.hpp"

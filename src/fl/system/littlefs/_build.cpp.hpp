/// @file fl/system/littlefs/_build.cpp.hpp
/// @brief Unity-build aggregate for LittleFS support. Pulled in only by
/// `src/fl/build/fl.system.littlefs+.cpp` — never by the parent
/// `src/fl/system/_build.cpp.hpp`. That separation is what lets the
/// linker tree-shake the LittleFS chain when a sketch never calls
/// `make_littlefs_filesystem()`, mirroring the SD split in
/// `fl/system/sd/` (FastLED #2773 item 1.2).

#include "fl/system/littlefs/fs_littlefs.cpp.hpp"

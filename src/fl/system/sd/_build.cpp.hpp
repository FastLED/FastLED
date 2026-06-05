/// @file fl/system/sd/_build.cpp.hpp
/// @brief Unity-build aggregate for SD-card support. Pulled in only by
/// `src/fl/build/fl.system.sd+.cpp` — never by the parent
/// `src/fl/system/_build.cpp.hpp`. That separation is what lets the
/// linker tree-shake the entire SD chain (libSD.a, libFS.a, Arduino's
/// VFSImpl, ~16 KB on ESP32-S3) when the user never calls
/// `FileSystem::beginSd()`. See FastLED #2773 item 1.2.

#include "fl/system/sd/file_system_sd.cpp.hpp"

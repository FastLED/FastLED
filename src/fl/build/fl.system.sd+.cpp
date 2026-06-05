/// @file fl.system.sd+.cpp
/// @brief Unity build entry-point for SD-card support, split out of
/// `fl.system+.cpp` so the linker can drop the entire SD chain
/// (`libSD.a`, `libFS.a`, Arduino's `VFSImpl`, ~16 KB on ESP32-S3)
/// when the user never calls `FileSystem::beginSd()`.
///
/// Replaces the earlier `FASTLED_USE_SDCARD` macro-gate (#2778 v1) —
/// no user opt-in required.
///
/// See FastLED #2773 item 1.2 and `fl/system/sd/file_system_sd.cpp.hpp`.

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/system/sd/_build.cpp.hpp"

/// @file fl.system.littlefs+.cpp
/// @brief Unity build entry-point for LittleFS support, kept out of
/// `fl.system+.cpp` so the linker can drop the LittleFS chain when a
/// sketch never calls `fl::make_littlefs_filesystem()`.
///
/// Same mechanism as `fl.system.sd+.cpp` — see
/// `fl/system/sd/file_system_sd.cpp.hpp` for the rationale.

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/system/littlefs/_build.cpp.hpp"

/// @file fl.system.embedded_fs+.cpp
/// @brief Unity build entry-point for embedded (on-chip flash) storage,
/// kept out of `fl.system+.cpp` so the linker can drop the filesystem
/// chain when a sketch never calls `fl::getEmbeddedFs()`.
///
/// The platform backend composes into this same TU via
/// `platforms/embedded_fs.h`, so entry point and implementation are
/// dropped together. Same mechanism as `fl.system.sd+.cpp`.

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/system/embedded_fs/_build.cpp.hpp"

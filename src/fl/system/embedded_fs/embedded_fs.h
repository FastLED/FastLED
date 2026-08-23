#pragma once

/// @file fl/system/embedded_fs/embedded_fs.h
/// @brief Storage built into the microcontroller itself.
///
/// "Embedded" here names *where the bytes live* — on-chip flash, no card,
/// no wiring — not which filesystem driver puts them there. That choice is
/// the platform's: LittleFS on ESP32 and ESP8266, and other backends as
/// they are added. A sketch never says "LittleFS", the same way it never
/// says "SdFat" to use an SD card (FastLED #4007).
///
/// Usage — pair it with the generic seam `FileSystem` already exposes:
///
/// @code
///     fl::FileSystem fs;
///     if (fs.begin(fl::getEmbeddedFs())) {
///         fl::ifstream f = fs.openRead("data/frame.rgb");
///     }
/// @endcode
///
/// Returns null on platforms with no embedded storage, so the call is safe
/// to make unconditionally and the failure surfaces at `begin()` rather
/// than at link time.
///
/// Tree-shaking: the body lives in `embedded_fs.cpp.hpp`, compiled into its
/// own translation unit (`src/fl/build/fl.system.embedded_fs+.cpp`). A
/// sketch that never calls `getEmbeddedFs()` never references that TU, so
/// the linker drops it and the whole filesystem chain with it — the same
/// mechanism that keeps the SD chain out of non-SD sketches (#2773).

#include "fl/stl/noexcept.h"
#include "fl/system/file_system.h" // IWYU pragma: export

namespace fl {

/// Filesystem backed by the microcontroller's own flash, or null where the
/// platform has none.
///
/// @param format_on_fail  Format and mount when the storage is absent or
///                        corrupt. Off by default: reformatting discards
///                        whatever was there, which is rarely wanted
///                        unprompted. Platforms with no such hook ignore it
///                        and simply fail to mount.
FsImplPtr getEmbeddedFs(bool format_on_fail = false) FL_NO_EXCEPT;

} // namespace fl

// IWYU pragma: private

/// @file fl/system/embedded_fs/_build.cpp.hpp
/// @brief Unity-build aggregate for embedded (on-chip flash) storage.
///
/// Pulled in only by `src/fl/build/fl.system.embedded_fs+.cpp` — never by
/// the parent `src/fl/system/_build.cpp.hpp`. That separation is what lets
/// the linker drop the whole chain when a sketch never calls
/// `fl::getEmbeddedFs()`, mirroring the SD split in `fl/system/sd/`
/// (FastLED #2773 item 1.2).

// begin current directory includes
#include "fl/system/embedded_fs/embedded_fs.cpp.hpp"

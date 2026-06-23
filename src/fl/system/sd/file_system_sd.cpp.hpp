/// @file fl/system/sd/file_system_sd.cpp.hpp
/// @brief SD-card support split off from `file_system.cpp.hpp` so the
/// linker can tree-shake the entire SD chain when nobody calls
/// `FileSystem::beginSd()`.
///
/// Mechanism: this TU is compiled into its own `.o` file
/// (`fl.system.sd+.cpp.o`, via `src/fl/build/fl.system.sd+.cpp`).
/// `FileSystem::beginSd(int)` and `make_sdcard_filesystem(int)` are the
/// only symbols this TU defines. The linker only pulls the `.o` into
/// the final binary when one of those symbols is referenced — i.e.
/// when the user's sketch actually calls `fs.beginSd(cs_pin)`. Sketches
/// that only use `FileSystem::begin(platform_filesystem)` (or that
/// don't touch `FileSystem` at all) see `fl.system.sd+.cpp.o` dropped
/// outright, taking `libSD.a` / `libFS.a` / Arduino's `VFSImpl` chain
/// (~16 KB on ESP32-S3 NEOPIXEL Blink) with it.
///
/// This replaces the earlier `FASTLED_USE_SDCARD` macro-gate
/// (#2778 v1) — no user opt-in required, the linker handles it.
///
/// See FastLED #2773 item 1.2 for the original audit.

#include "fl/system/file_system.h"
#include "fl/stl/has_include.h"
#include "fl/log/log.h"

#ifdef FASTLED_TESTING
// Stub filesystem maps to the host's real disk (for unit tests).
// `make_sdcard_filesystem` strong impl lives in
// `platforms/stub/fs_stub.cpp.hpp` (part of `platforms+.cpp.o`).
// IWYU pragma: begin_keep
#include "platforms/stub/fs_stub.hpp" // ok platform headers
// IWYU pragma: end_keep
#define FASTLED_HAS_SDCARD_IMPL 1

#elif defined(FL_IS_WASM)
// WASM filesystem backs onto the browser. `make_sdcard_filesystem`
// strong impl lives in `platforms/wasm/fs_wasm.cpp.hpp`.
// IWYU pragma: begin_keep
#include "platforms/wasm/fs_wasm.h" // ok platform headers
// IWYU pragma: end_keep
#define FASTLED_HAS_SDCARD_IMPL 1

#elif FL_HAS_INCLUDE(<SD.h>) && FL_HAS_INCLUDE(<fs.h>)
// Arduino SD library is on the include path. `fs_sdcard_arduino.hpp`
// provides an `inline` `make_sdcard_filesystem` definition that uses
// the real Arduino `SD`/`FS` libraries. Pulling those into
// `fl.system.sd+.cpp.o` is fine — the linker only links this TU when
// the user calls beginSd, so non-SD sketches see none of it.
#include "platforms/fs_sdcard_arduino.hpp"
#define FASTLED_HAS_SDCARD_IMPL 1

#else
#define FASTLED_HAS_SDCARD_IMPL 0
#endif

namespace fl {

#if !FASTLED_HAS_SDCARD_IMPL
// Weak fallback when no platform-specific SD implementation is
// available in the build environment. Returns nullptr so
// `FileSystem::beginSd()` cleanly returns `false`.
FL_LINK_WEAK FsImplPtr make_sdcard_filesystem(int cs_pin) {
    FASTLED_UNUSED(cs_pin);
    return FsImplPtr();
}
#endif

bool FileSystem::beginSd(int cs_pin) {
    mFs = make_sdcard_filesystem(cs_pin);
    if (!mFs) {
        return false;
    }
    mFs->begin();
    return true;
}

} // namespace fl

#pragma once

// IWYU pragma: private

/// @file platforms/wasm/fs/embedded_fs_wasm.hpp
/// @brief Browser-VFS fragment backing `fl::getEmbeddedFs()` on WASM.
///
/// The point is simulation. A sketch written against ESP on-chip flash --
/// `fs.begin(fl::getEmbeddedFs())` -- should run unmodified in the web
/// preview, so the browser stands in for LittleFS. Without this the WASM
/// build fell through to the no-op fragment and `getEmbeddedFs()` returned
/// null, which is backwards for a simulator: the platform whose whole job is
/// to imitate the device was the one platform that could not.
///
/// It returns the same `FsImplWasm` that `make_sdcard_filesystem()` returns,
/// deliberately. There is exactly one filesystem in the browser -- files
/// injected from JS into a shared `FileRegistry` -- so "embedded flash" and
/// "SD card" are two names for it here. Handing back the same backend means a
/// sketch that opens an asset through either API sees the same bytes, which is
/// what makes swapping the storage target a build-time decision rather than a
/// code change.
///
/// A plain header, not a `.cpp.hpp`: it composes into whichever translation
/// unit includes it, keeping the backend inside `fl.fs.embedded+.cpp.o` so it
/// still drops out when a sketch never asks for embedded storage. Same shape
/// as the ESP fragment beside it.

#include "fl/stl/compiler_control.h"
#include "fl/fs/fs.h"
#include "platforms/wasm/fs_wasm.h"

namespace fl {
namespace platforms {

inline FsImplPtr makeEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    // Nothing to format: the browser VFS is populated by the loader before
    // the sketch runs, and a failed mount is not a state it can reach.
    FASTLED_UNUSED(format_on_fail);
    // cs_pin is meaningless here -- FsImplWasm ignores it. Passing 0 rather
    // than inventing a second factory keeps one backend and one registry.
    return fl::make_sdcard_filesystem(0);
}

} // namespace platforms
} // namespace fl

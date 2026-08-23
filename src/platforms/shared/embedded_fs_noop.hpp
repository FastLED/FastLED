#pragma once

// IWYU pragma: private

/// @file platforms/shared/embedded_fs_noop.hpp
/// @brief No-op embedded-storage fragment for platforms with no on-chip
/// filesystem.
///
/// Returning null rather than refusing to compile is the point: a sketch
/// can call `fl::getEmbeddedFs()` unconditionally and branch on the
/// result, instead of guarding every call site with platform macros. The
/// failure then surfaces at `FileSystem::begin()`, where the sketch is
/// already checking, rather than at link time.

#include "fl/stl/compiler_control.h"
#include "fl/system/file_system.h"

namespace fl {
namespace platforms {

inline FsImplPtr makeEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    FASTLED_UNUSED(format_on_fail);
    return FsImplPtr();
}

} // namespace platforms
} // namespace fl

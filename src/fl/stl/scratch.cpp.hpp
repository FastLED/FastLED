// IWYU pragma: private
/// @file fl/stl/scratch.cpp.hpp
/// @brief Cold exhaustion hook of fl::scratch's global side stack. It does
/// not depend on FL_SCRATCH_STACK_BYTES (the arena itself is a template
/// static in scratch.h), and --gc-sections drops it when nothing uses
/// fl::scratch.

#include "fl/stl/scratch.h"
#include "fl/stl/compiler_control.h"
#include "fl/log/log.h"

namespace fl {
namespace detail {

void scratch_exhausted(fl::size bytes) FL_NO_EXCEPT {
    // FL_WARN_ONCE compiles to nothing on small/release builds.
    FL_UNUSED(bytes);
    // Once: an exhausted render path would otherwise warn every frame.
    FL_WARN_ONCE("fl::scratch: side stack exhausted (" << bytes
                 << " B requested); raise FL_SCRATCH_STACK_BYTES");
}

} // namespace detail
} // namespace fl

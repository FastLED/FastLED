// IWYU pragma: private
/// @file fl/stl/scratch.cpp.hpp
/// @brief The global side-stack arena behind fl::scratch<T> when
/// FL_SCRATCH_STACK_BYTES > 0. The arena, its state and the cold exhaustion
/// hook are only referenced by fl::scratch instantiations, so --gc-sections
/// drops them from builds that never use fl::scratch.

#include "fl/stl/scratch.h"

#if FL_SCRATCH_STACK_BYTES > 0

#include "fl/stl/align.h"
#include "fl/log/log.h"

namespace fl {
namespace detail {

// FL_LINT_ALLOW_GLOBAL(only inline fl::scratch code references it; -fdata-sections + --gc-sections drop it when unused, measured on avr-gcc and xtensa; a Singleton would add an init guard to every lease)
FL_ALIGN_MAX u8 scratch_arena[FL_SCRATCH_STACK_BYTES];
// FL_LINT_ALLOW_GLOBAL(same as scratch_arena; zero-initialised so it lands in .bss)
side_stack_state scratch_state;  // zero-initialised: .bss

void scratch_exhausted(fl::size bytes) FL_NO_EXCEPT {
    FL_UNUSED(bytes);  // FL_WARN compiles to nothing on small/release builds
    // Once: an exhausted render path would otherwise warn every frame.
    FL_WARN_ONCE("fl::scratch: side stack exhausted (" << bytes << " B requested, "
            << scratch_state.top << "/" << fl::size(FL_SCRATCH_STACK_BYTES)
            << " B used); raise FL_SCRATCH_STACK_BYTES");
}

} // namespace detail

fl::size scratch_high_water() FL_NO_EXCEPT {
    return detail::scratch_state.high_water;
}

void scratch_reset_high_water() FL_NO_EXCEPT {
    detail::scratch_state.high_water = detail::scratch_state.top;
}

} // namespace fl

#endif  // FL_SCRATCH_STACK_BYTES > 0

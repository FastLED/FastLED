#pragma once

#include "fl/stl/compiler_control.h"

namespace fl {

/// @brief No-op exit function for embedded systems
/// In embedded environments, calling exit is typically not meaningful,
/// so this is a placeholder that does nothing.
/// A template so an unqualified `exit(...)` always picks ::exit (FastLED #4834).
template <typename FlLibcTag = void>
FASTLED_FORCE_INLINE void exit(int code) {
    (void)code;  // Suppress unused parameter warning
    // No-op: intentionally does nothing
}

}  // namespace fl

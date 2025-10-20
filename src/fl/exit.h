#pragma once

namespace fl {

/// @brief No-op exit function for embedded systems
/// In embedded environments, calling exit is typically not meaningful,
/// so this is a placeholder that does nothing.
inline void exit(int code) {
    (void)code;  // Suppress unused parameter warning
    // No-op: intentionally does nothing
}

}  // namespace fl

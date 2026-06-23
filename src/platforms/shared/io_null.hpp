#pragma once
#include "fl/stl/noexcept.h"

// IWYU pragma: private


namespace fl {
namespace platforms {

// Low-level Teensy print functions that avoid ALL Arduino dependencies
// This uses no-op implementations to prevent "_write" linker issues
// Users should initialize their own Serial ports if they need actual output

inline void print(const char* str) FL_NOEXCEPT {
    if (!str) return;

    // No-op implementation to avoid "_write" linker dependencies
    // Teensy users should use Serial1.print() directly in their sketch for output
    // This prevents "undefined reference to _write" errors completely
    (void)str; // Suppress unused parameter warning
}

inline void println(const char* str) FL_NOEXCEPT {
    if (!str) return;

    // No-op implementation to avoid "_write" linker dependencies
    // Teensy users should use Serial1.println() directly in their sketch for output
    (void)str; // Suppress unused parameter warning
}

// Input functions - no-op implementations for Teensy
inline int available() FL_NOEXCEPT {
    // No-op implementation to avoid Arduino dependencies
    // Teensy users should use Serial1.available() directly in their sketch for input
    return 0;
}

inline int read() FL_NOEXCEPT {
    // No-op implementation to avoid Arduino dependencies
    // Teensy users should use Serial1.read() directly in their sketch for input
    return -1;
}

inline int readLineNative(char delimiter, char* out, int outLen) FL_NOEXCEPT {
    (void)delimiter; (void)out; (void)outLen;
    return -1;  // Not supported
}

} // namespace platforms
} // namespace fl

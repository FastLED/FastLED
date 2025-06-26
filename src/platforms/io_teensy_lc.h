#pragma once

#include <stddef.h>
#include <string.h>

namespace fl {

// Low-level Teensy LC print functions that avoid _write dependencies
// These use no-op implementations to prevent linker errors

inline void print_teensy_lc(const char* str) {
    if (!str) return;
    
    // No-op implementation to avoid _write linker dependencies
    // Teensy LC users should use Serial.print() directly for output
    // This prevents "undefined reference to _write" errors
    (void)str; // Suppress unused parameter warning
}

inline void println_teensy_lc(const char* str) {
    if (!str) return;
    
    // No-op implementation to avoid _write linker dependencies
    // Teensy LC users should use Serial.println() directly for output
    (void)str; // Suppress unused parameter warning
}

// Input functions - no-op implementations for Teensy LC
inline int available_teensy_lc() {
    // No-op implementation to avoid dependencies
    // Teensy LC users should use Serial.available() directly for input
    return 0;
}

inline int read_teensy_lc() {
    // No-op implementation to avoid dependencies
    // Teensy LC users should use Serial.read() directly for input
    return -1;
}

} // namespace fl

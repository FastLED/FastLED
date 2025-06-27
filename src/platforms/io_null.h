#pragma once


namespace fl {

// Low-level Teensy print functions that avoid ALL Arduino dependencies
// This uses no-op implementations to prevent "_write" linker issues
// Users should initialize their own Serial ports if they need actual output

inline void print_null(const char* str) {
    if (!str) return;
    
    // No-op implementation to avoid "_write" linker dependencies
    // Teensy users should use Serial1.print() directly in their sketch for output
    // This prevents "undefined reference to _write" errors completely
    (void)str; // Suppress unused parameter warning
}

inline void println_null(const char* str) {
    if (!str) return;
    
    // No-op implementation to avoid "_write" linker dependencies  
    // Teensy users should use Serial1.println() directly in their sketch for output
    (void)str; // Suppress unused parameter warning
}

// Input functions - no-op implementations for Teensy
inline int available_null() {
    // No-op implementation to avoid Arduino dependencies
    // Teensy users should use Serial1.available() directly in their sketch for input
    return 0;
}

inline int read_null() {
    // No-op implementation to avoid Arduino dependencies
    // Teensy users should use Serial1.read() directly in their sketch for input
    return -1;
}

} // namespace fl

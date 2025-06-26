#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string.h>

namespace fl {

// Low-level Teensy print functions using standard I/O
// These use fputs/write for maximum efficiency and avoid Arduino dependencies

inline void print_teensy(const char* str) {
    if (!str) return;
    
    // Use fputs to write to stdout - works on all Teensy platforms
    // that support standard I/O streams
    if (fputs(str, stdout) == EOF) {
        // Fallback to write() system call if fputs fails
        size_t len = strlen(str);
        write(STDOUT_FILENO, str, len);
    }
}

inline void println_teensy(const char* str) {
    if (!str) return;
    print_teensy(str);
    
    // Write newline using fputs first, fallback to write
    if (fputs("\n", stdout) == EOF) {
        write(STDOUT_FILENO, "\n", 1);
    }
    
    // Flush output to ensure immediate display
    fflush(stdout);
}

// Input functions using standard I/O
inline int available_teensy() {
    // Check if stdin has data available
    // Note: This is a simplified implementation - true non-blocking
    // input detection would require platform-specific code
    return 0; // Conservative default - no data available
}

inline int read_teensy() {
    // Read a single character from stdin
    return getchar();
}

} // namespace fl

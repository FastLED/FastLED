#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

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
    // Check if stdin has data available using POSIX select()
    // This provides non-blocking detection of available input data
    
    fd_set readfds;
    struct timeval timeout;
    int result;
    
    // Initialize file descriptor set with stdin
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    // Set timeout to 0 for immediate return (non-blocking)
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    // Use select() to check if data is available on stdin
    result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    
    if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        // Data is available for reading
        return 1;  // At least 1 byte available (exact count not determinable with select)
    }
    
    // No data available or error occurred
    return 0;
}

inline int read_teensy() {
    // Read a single character from stdin
    return getchar();
}

} // namespace fl

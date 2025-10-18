#pragma once

#ifdef _WIN32
#include <io.h>  // for _write
#else
#include <unistd.h>  // for write and fsync
#endif

namespace fl {

// Print functions
inline void print_native(const char* str, bool flush=true) {
    if (!str) return;

    // Native/Testing: Use direct system calls to stderr
    // Calculate length without strlen()
    size_t len = 0;
    const char* p = str;
    while (*p++) len++;

#ifdef _WIN32
    // Windows version: _write to stderr (2 is stderr file descriptor)
    // Note: _write performs native I/O and doesn't require additional flushing
    _write(2, str, len);  // 2 = stderr
    (void)flush;  // flush parameter unused on Windows with direct _write
#else
    // Unix/Linux version
    ::write(2, str, len);  // 2 = stderr
    // Force flush stderr during testing to prevent output loss on crashes
    if (flush) {
        fsync(2);
    }
#endif
}

inline void println_native(const char* str) {
    if (!str) return;
    print_native(str, false);
    print_native("\n", true);
}

// Input functions
inline int available_native() {
    // Native platforms - no input available in most cases
    // This is mainly for testing environments
    return 0;
}

inline int read_native() {
    // Native platforms - no input available in most cases
    // This is mainly for testing environments
    return -1;
}

} // namespace fl

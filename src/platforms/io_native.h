#pragma once

#ifdef _WIN32
#include <io.h>  // for _write
#else
#include <unistd.h>  // for write
#endif

namespace fl {

// Print functions
inline void print_native(const char* str) {
    if (!str) return;
    
    // Native/Testing: Use direct system calls to stderr
    // Calculate length without strlen()
    size_t len = 0;
    const char* p = str;
    while (*p++) len++;
    
#ifdef _WIN32
    // Windows version
    _write(2, str, len);  // 2 = stderr
#else
    // Unix/Linux version
    ::write(2, str, len);  // 2 = stderr
#endif
}

inline void println_native(const char* str) {
    if (!str) return;
    print_native(str);
    print_native("\n");
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

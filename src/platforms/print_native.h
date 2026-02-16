#pragma once

#ifdef _WIN32
// IWYU pragma: begin_keep
#include <io.h>  // for _write
// IWYU pragma: end_keep
#else
// IWYU pragma: begin_keep
#include <unistd.h>  // for write
// IWYU pragma: end_keep
#endif

namespace fl {

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

} // namespace fl

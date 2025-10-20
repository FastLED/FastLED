#pragma once

#ifndef FASTLED_TEENSY_USE_PRINTF
// DISABLED: printf causes ~5KB memory bloat for simple applications like Blink
// Users can manually enable with #define FASTLED_TEENSY_USE_PRINTF 1 if needed
// #if defined(__IMXRT1062__) && defined(DEBUG)
// #define FASTLED_TEENSY_USE_PRINTF 1
// #else
#define FASTLED_TEENSY_USE_PRINTF 0
// #endif
#endif  // !defined(FASTLED_TEENSY_USE_PRINTF)

#if !FASTLED_TEENSY_USE_PRINTF
#include "io_null.h"
#endif

#if FASTLED_TEENSY_USE_PRINTF
// Note: printf includes moved to avoid stdlib in header
// These are only used when FASTLED_TEENSY_USE_PRINTF is enabled
// and are typically disabled in production builds due to 5KB memory bloat

namespace fl {

// Forward declaration for printf - compiler provides this
extern "C" int printf(const char*, ...);

inline void print_teensy(const char* str) {
    if (!str) return;
    printf("%s", str);
}
inline void println_teensy(const char* str) {
    if (!str) return;
    printf("%s\n", str);
}
inline int available_teensy() {
    return 0;
}
inline int read_teensy() {
    return -1;
}

} // namespace fl


#else // !FASTLED_TEENSY_USE_PRINTF

namespace fl {
inline void print_teensy(const char* str) {
    print_null(str);
}
inline void println_teensy(const char* str) {
    println_null(str);
}
inline int available_teensy() {
    return available_null();
}
inline int read_teensy() {
    return read_null();
}
} // namespace fl

#endif // !FASTLED_TEENSY_USE_PRINTF

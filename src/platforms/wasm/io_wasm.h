#pragma once

/// WASM I/O implementation (print, println, available, read)
/// Consolidates print and input functions for the WASM platform.
/// Print functions use printf to output to JavaScript console.
/// Input functions always return empty/no-data (WASM doesn't support serial input by default).

#include <emscripten.h>
#include <cstdio>

namespace fl {

/// Print string to console (via printf)
inline void print_wasm(const char* str) {
    if (!str) return;
    ::printf("%s", str);
}

/// Print string with newline to console (via printf)
inline void println_wasm(const char* str) {
    if (!str) return;
    ::printf("%s\n", str);
}

/// Always returns 0 (no input available in WASM)
inline int available_wasm() {
    return 0;
}

/// Always returns -1 (no data to read in WASM)
inline int read_wasm() {
    return -1;
}

} // namespace fl

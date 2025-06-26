#pragma once

#include <emscripten.h>
#include <cstdio>

namespace fl {

// Print functions
inline void print_wasm(const char* str) {
    if (!str) return;
    // WASM: Use JavaScript console.log (via printf)
    ::printf("%s", str);
}

inline void println_wasm(const char* str) {
    if (!str) return;
    ::printf("%s\n", str);
}

// Input functions
inline int available_wasm() {
    // WASM Serial emulation always returns 0 (no input available)
    return 0;
}

inline int read_wasm() {
    // WASM Serial emulation always returns -1 (no data)
    return -1;
}

} // namespace fl

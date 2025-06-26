#pragma once

#include <emscripten.h>
#include <cstdio>

namespace fl {

inline void print_wasm(const char* str) {
    if (!str) return;
    // WASM: Use JavaScript console.log (via printf)
    ::printf("%s", str);
}

inline void println_wasm(const char* str) {
    if (!str) return;
    ::printf("%s\n", str);
}

} // namespace fl

#pragma once

namespace fl {
    // WebAssembly / Emscripten: Default desktop-like type mapping
    // short is 16-bit, int is 32-bit
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // WASM32: pointers are 32-bit, WASM64: pointers are 64-bit
    #if defined(__wasm64__) || defined(__wasm64)
        typedef unsigned long long size;
        typedef unsigned long long uptr;
        typedef long long iptr;
        typedef long long ptrdiff;
    #else
        // WASM32 (default)
        // Note: Emscripten uses 'long' for intptr_t and ptrdiff_t (both 32-bit)
        // to match system headers, even though 'int' is also 32-bit
        typedef unsigned long size;
        typedef unsigned long uptr;
        typedef long iptr;
        typedef long ptrdiff;
    #endif
}

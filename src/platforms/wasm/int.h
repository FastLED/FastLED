#pragma once

namespace fl {
    // WebAssembly / Emscripten: Use compiler builtins to match system headers exactly
    // This prevents typedef conflicts with Emscripten's C++ standard library
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;
    typedef long long i64;
    typedef unsigned long long u64;

    // WASM32: pointers are 32-bit, WASM64: pointers are 64-bit
    // Use compiler builtins (__SIZE_TYPE__, __INTPTR_TYPE__, etc.) to ensure
    // our types match exactly what Emscripten's <stddef.h> and <stdint.h> use.
    // This is critical for C++ standard library compatibility (operator new, etc.)
    #if defined(__wasm64__) || defined(__wasm64)
        typedef unsigned long long size;
        typedef unsigned long long uptr;
        typedef long long iptr;
        typedef long long ptrdiff;
    #else
        // WASM32 (default): Use exact types to match emscripten system headers
        // Note: Emscripten's system headers define these as 'unsigned long' and 'long',
        // even though compiler builtins like __SIZE_TYPE__ report 'unsigned int'.
        // We must match the actual system header definitions to avoid typedef conflicts.
        typedef unsigned long size;
        typedef unsigned long uptr;
        typedef long iptr;
        typedef long ptrdiff;
    #endif
}

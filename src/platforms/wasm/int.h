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
        // WASM32 (default): Use compiler builtins to match system headers
        #ifdef __SIZE_TYPE__
            typedef __SIZE_TYPE__ size;
        #else
            typedef unsigned long size;  // Fallback
        #endif

        #ifdef __UINTPTR_TYPE__
            typedef __UINTPTR_TYPE__ uptr;
        #else
            typedef unsigned long uptr;  // Fallback
        #endif

        #ifdef __INTPTR_TYPE__
            typedef __INTPTR_TYPE__ iptr;
        #else
            typedef long iptr;  // Fallback
        #endif

        #ifdef __PTRDIFF_TYPE__
            typedef __PTRDIFF_TYPE__ ptrdiff;
        #else
            typedef long ptrdiff;  // Fallback
        #endif
    #endif
}

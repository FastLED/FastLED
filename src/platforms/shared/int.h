#pragma once

namespace fl {
    // Default platform (desktop/generic): assume short 16-bit, int 32-bit
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;             // 'int' is 32-bit on all desktop and most embedded targets
    typedef unsigned int u32;

    // 64-bit types and pointer-width types: Linux/Unix x86_64 uses 'long', Windows x64 uses 'long long'
    #if defined(__LP64__) || defined(_LP64)
        // LP64 model (Linux/Unix x86_64): long and pointers are 64-bit
        typedef long i64;
        typedef unsigned long u64;
        typedef unsigned long size;   // size_t equivalent
        typedef unsigned long uptr;   // uintptr_t equivalent
    #else
        // LLP64 model (Windows x64) or 32-bit: long long is 64-bit
        typedef long long i64;
        typedef unsigned long long u64;

        // size and uptr must match pointer width
        #if defined(_WIN64)
            // Windows x64 (LLP64): pointers are 64-bit, but long is 32-bit
            typedef unsigned long long size;
            typedef unsigned long long uptr;
        #elif defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
            // Other 64-bit platforms with LLP64-like model
            typedef unsigned long long size;
            typedef unsigned long long uptr;
        #else
            // 32-bit platforms: pointers are 32-bit
            typedef unsigned long size;
            typedef unsigned long uptr;
        #endif
    #endif
}

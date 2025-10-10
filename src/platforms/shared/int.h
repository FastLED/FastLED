#pragma once

#include <stdint.h>
#include <stddef.h>

namespace fl {
    // Default platform (desktop/generic): assume short 16-bit, int 32-bit (uint32_t is unsigned int)
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;             // 'int' is 32-bit on all desktop and most embedded targets
    typedef unsigned int u32;
    // 64-bit types: Linux/Unix x86_64 uses 'long', Windows x64 uses 'long long'
    #if defined(__LP64__) || defined(_LP64)
        // LP64 model (Linux/Unix x86_64): long and pointers are 64-bit
        typedef long i64;
        typedef unsigned long u64;
    #else
        // LLP64 model (Windows x64) or 32-bit: long long is 64-bit
        typedef long long i64;
        typedef unsigned long long u64;
    #endif
    // Use standard types directly to ensure exact compatibility
    typedef size_t size;
    typedef uintptr_t uptr;
}

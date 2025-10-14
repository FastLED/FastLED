#pragma once

// Generic/32-bit platform integer type definitions
// Used for 32-bit systems and unknown platforms

namespace fl {
    // 16-bit and 32-bit types (same across platforms)
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;

    // 64-bit types: use 'long long' (standard for 32-bit systems)
    typedef long long i64;
    typedef unsigned long long u64;

    // Pointer and size types for 32-bit systems
    // size_t and uintptr_t are typically 'unsigned long' on 32-bit
    typedef unsigned long size;
    typedef unsigned long uptr;
    typedef long iptr;
    typedef long ptrdiff;
}

#pragma once

// Windows LLP64 integer type definitions
// Windows x86_64 uses LLP64 model: long is 32-bit, long long is 64-bit

namespace fl {
    // 16-bit and 32-bit types (same across platforms)
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;

    // 64-bit types on Windows: use 'long long'
    // Windows keeps 'long' as 32-bit even on 64-bit systems (LLP64 model)
    typedef long long i64;
    typedef unsigned long long u64;

    // Pointer and size types
    // All pointer-sized types use 'long long' on Windows x64
    typedef unsigned long long size;
    typedef unsigned long long uptr;
    typedef long long iptr;
    typedef long long ptrdiff;
}

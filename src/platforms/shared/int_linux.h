#pragma once

// Linux LP64 integer type definitions
// Linux LP64: u64 is 'unsigned long' (differs from macOS)

namespace fl {
    // 16-bit and 32-bit types (same across platforms)
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;

    // 64-bit types on Linux LP64: use 'long' to match system u64
    typedef long i64;
    typedef unsigned long u64;

    // Pointer and size types
    // All pointer-sized types are 'unsigned long' on Linux LP64
    typedef unsigned long size;
    typedef unsigned long uptr;
    typedef long iptr;
    typedef long ptrdiff;
}

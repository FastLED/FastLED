#pragma once

// macOS LP64 integer type definitions
// macOS differs from Linux LP64: uint64_t is 'unsigned long long', not 'unsigned long'

namespace fl {
    // 16-bit and 32-bit types (same across platforms)
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;

    // 64-bit types on macOS: use 'long long' to match system uint64_t
    // macOS defines uint64_t as 'unsigned long long' even in 64-bit mode
    typedef long long i64;
    typedef unsigned long long u64;

    // Pointer and size types
    // size_t and uintptr_t are 'unsigned long' on macOS LP64
    typedef unsigned long size;
    typedef unsigned long uptr;
    typedef long iptr;
    typedef long ptrdiff;
}

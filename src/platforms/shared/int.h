#pragma once

namespace fl {
    // Default platform (desktop/generic): assume short 16-bit, int 32-bit
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;

    // LP64 model (Linux/Unix x86_64): long is 64-bit
    // Note: Use long (not long long) for i64/u64 to match system headers on LP64
    #if defined(__LP64__) || defined(_LP64)
        typedef long i64;
        typedef unsigned long u64;
        typedef unsigned long size;
        typedef unsigned long uptr;
        typedef long iptr;
        typedef long ptrdiff;
    // Windows x86_64 (LLP64 model): long long is 64-bit, long is 32-bit
    #elif defined(_WIN64) || defined(_M_AMD64)
        typedef long long i64;
        typedef unsigned long long u64;
        typedef unsigned long long size;
        typedef unsigned long long uptr;
        typedef long long iptr;
        typedef long long ptrdiff;
    // 32-bit platforms
    #else
        typedef long long i64;
        typedef unsigned long long u64;
        typedef unsigned long size;
        typedef unsigned long uptr;
        typedef long iptr;
        typedef long ptrdiff;
    #endif
}

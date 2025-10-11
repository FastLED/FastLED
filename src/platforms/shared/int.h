#pragma once

namespace fl {
    // Default platform (desktop/generic): assume short 16-bit, int 32-bit
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;

    // LP64 model (Linux/Unix x86_64): long is 64-bit
    #if defined(__LP64__) || defined(_LP64)
        typedef long i64;
        typedef unsigned long u64;
        typedef unsigned long size;
        typedef unsigned long uptr;
        typedef long ptrdiff;
    // LLP64 or x86_64: long long is 64-bit
    #elif defined(_WIN64) || defined(__x86_64__) || defined(__amd64__) || defined(__amd64) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
        typedef long long i64;
        typedef unsigned long long u64;
        typedef unsigned long long size;
        typedef unsigned long long uptr;
        typedef long long ptrdiff;
    // 32-bit platforms
    #else
        typedef long long i64;
        typedef unsigned long long u64;
        typedef unsigned long size;
        typedef unsigned long uptr;
        typedef long ptrdiff;
    #endif
}

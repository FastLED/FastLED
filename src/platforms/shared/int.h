#pragma once

namespace fl {
    // Default platform (desktop/generic): assume short 16-bit, int 32-bit
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;

    // LP64 model detection - but macOS differs from Linux!
    // macOS: uint64_t is 'unsigned long long' even in LP64 mode
    // Linux: uint64_t is 'unsigned long' in LP64 mode
    #if defined(__APPLE__) && (defined(__LP64__) || defined(_LP64))
        typedef long long i64;
        typedef unsigned long long u64;
        typedef unsigned long size;        // size_t is unsigned long on macOS LP64
        typedef unsigned long uptr;        // uintptr_t is unsigned long on macOS LP64
        typedef long iptr;
        typedef long ptrdiff;
    #elif defined(__LP64__) || defined(_LP64)
        // Linux/Unix LP64: long is 64-bit
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

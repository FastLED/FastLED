#pragma once

namespace fl {
    // On AVR: int is 16-bit, long is 32-bit — match stdint sizes manually
    typedef int i16;
    typedef unsigned int u16;
    typedef long i32;
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // AVR is 8-bit: pointers are 16-bit
    typedef unsigned int size;   // size_t equivalent (16-bit on AVR)
    typedef unsigned int uptr;   // uintptr_t equivalent (16-bit on AVR)
    typedef int iptr;            // intptr_t equivalent (16-bit on AVR)
    typedef int ptrdiff;         // ptrdiff_t equivalent (16-bit on AVR)
}

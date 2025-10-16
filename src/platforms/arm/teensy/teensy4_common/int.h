#pragma once

namespace fl {
    // ARM Teensy 4.0/4.1 (IMXRT1062 Cortex-M7): 32-bit platform
    // 
    // On Teensy 4.x, the Arduino core includes system headers that define
    // size_t, uintptr_t, and ptrdiff_t before FastLED headers are included.
    // This causes typedef conflicts in fl/stdint.h.
    //
    // To avoid conflicts while maintaining FastLED's integer type system:
    // - We define fl::size, fl::uptr, fl::ptrdiff using the underlying types
    // - fl/stdint.h will typedef these to size_t, uintptr_t, ptrdiff_t
    // - The underlying types match exactly what the system headers use
    
    typedef short i16;
    typedef unsigned short u16;
    typedef long i32;
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
    
    // Match Teensy 4.x system header types exactly:
    // - size_t is defined as unsigned int in stddef.h
    // - uintptr_t is defined as __uintptr_t (unsigned int) in sys/_stdint.h
    // - intptr_t is defined as __intptr_t (int) in sys/_stdint.h
    // - ptrdiff_t is defined as int in stddef.h
    typedef unsigned int size;    // size_t equivalent for Teensy 4.x
    typedef unsigned int uptr;    // uintptr_t equivalent for Teensy 4.x
    typedef int iptr;             // intptr_t equivalent for Teensy 4.x
    typedef int ptrdiff;          // ptrdiff_t equivalent for Teensy 4.x
}

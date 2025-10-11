#pragma once

namespace fl {
    // ARM Teensy 3.x family (MK20DX/MKL26Z Cortex-M4/M0+): 32-bit platform
    //
    // Supported platforms:
    // - Teensy 3.0 (MK20DX128 Cortex-M4 @ 48MHz)
    // - Teensy 3.1/3.2 (MK20DX256 Cortex-M4 @ 72MHz)
    // - Teensy LC (MKL26Z64 Cortex-M0+ @ 48MHz)
    //
    // On Teensy 3.x, the Arduino core includes system headers that define
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

    // Match Teensy 3.x system header types exactly:
    // - size_t is defined as unsigned int in stddef.h (line 209)
    // - uintptr_t is defined as __uintptr_t (unsigned int) in sys/_stdint.h (line 82)
    // - intptr_t is defined as __intptr_t (int) in sys/_stdint.h
    // - ptrdiff_t is defined as int in stddef.h (line 143)
    typedef unsigned int size;    // size_t equivalent for Teensy 3.x
    typedef unsigned int uptr;    // uintptr_t equivalent for Teensy 3.x
    typedef int iptr;             // intptr_t equivalent for Teensy 3.x
    typedef int ptrdiff;          // ptrdiff_t equivalent for Teensy 3.x
}

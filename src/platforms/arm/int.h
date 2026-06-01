#pragma once

// IWYU pragma: private

namespace fl {
#if defined(ARDUINO_ARCH_ZEPHYR)
    typedef __INT16_TYPE__ i16;
    typedef __UINT16_TYPE__ u16;
    typedef __INT32_TYPE__ i32;
    typedef __UINT32_TYPE__ u32;
    typedef __INT64_TYPE__ i64;
    typedef __UINT64_TYPE__ u64;
    typedef __SIZE_TYPE__ size;
    typedef __UINTPTR_TYPE__ uptr;
    typedef __INTPTR_TYPE__ iptr;
    typedef __PTRDIFF_TYPE__ ptrdiff;
#else
    // ARM platforms (32-bit): short is 16-bit, long is 32-bit
    // uint32_t resolves to 'unsigned long' on most ARM toolchains
    //
    // Supported platforms:
    // - Arduino Due (SAM3X8E Cortex-M3)
    // - Teensy 3.0 / 3.1 (MK20DX128 / MK20DX256)
    // - Teensy LC (MKL26Z64 Cortex-M0+)
    // - Teensy 4.0 / 4.1 (iMXRT1062 Cortex-M7)
    // - Arduino UNO R4 WiFi (Renesas RA4M1)
    // - STM32F1 (Maple Mini and similar)
    // - Arduino GIGA R1 (STM32H747)
    // - Nordic nRF52 family (nRF52832, nRF52840, etc.)
    typedef short i16;
    typedef unsigned short u16;
    typedef long i32;
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // ARM is 32-bit: pointers and size are 32-bit
    // Note: On ARM toolchains, size_t and uintptr_t are typically unsigned int (not unsigned long)
    // and ptrdiff_t is typically int (not long)
    typedef unsigned int size;    // size_t equivalent
    typedef unsigned int uptr;    // uintptr_t equivalent
    typedef int iptr;             // intptr_t equivalent
    typedef int ptrdiff;          // ptrdiff_t equivalent
#endif
} 

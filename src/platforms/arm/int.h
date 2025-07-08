#pragma once

#include <stdint.h>
#include <stddef.h>

namespace fl {
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
    // size_t is unsigned long on ARM (32-bit)
    typedef size_t size;
    // uintptr_t is unsigned long on ARM (32-bit pointers)
    typedef uintptr_t uptr;
} 

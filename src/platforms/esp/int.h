#pragma once


namespace fl {
    // ESP32: Like ARM platforms, uint32_t and int32_t are 'unsigned long' and 'long'
    // This matches the Xtensa toolchain behavior similar to ARM toolchains
    typedef short i16;
    typedef unsigned short u16;
    typedef long i32;
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // size_t is unsigned int on ESP32 (32-bit)
    typedef unsigned int sz;
    // uintptr_t is unsigned int on ESP32 (32-bit pointers)
    typedef unsigned int uptr;
}

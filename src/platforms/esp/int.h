#pragma once

#ifdef __cplusplus
namespace fl {
    // ESP platforms (ESP8266/ESP32): Xtensa and RISC-V toolchains
    // 32-bit platforms: short is 16-bit, long is 32-bit
    // Like ARM platforms, 32-bit types use 'long' to match uint32_t/int32_t
    typedef short i16;
    typedef unsigned short u16;
    typedef long i32;
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // ESP32 is 32-bit: pointers are 32-bit
    typedef unsigned long size;   // size_t equivalent (32-bit on ESP)
    typedef unsigned long uptr;   // uintptr_t equivalent (32-bit on ESP)
}
#else
// C language compatibility - define types in global namespace
typedef short i16;
typedef unsigned short u16;
typedef long i32;
typedef unsigned long u32;
typedef long long i64;
typedef unsigned long long u64;
typedef unsigned long size;
typedef unsigned long uptr;
#endif

#pragma once

#ifdef __cplusplus
namespace fl {
    // ESP platforms (ESP8266/ESP32): Xtensa and RISC-V toolchains
    // 32-bit platforms: short is 16-bit, int is 32-bit, long is 32-bit
    // Unlike ARM, ESP uses 'int' for 32-bit types (not 'long')
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;              // unsigned int is __uint32_t on ESP
    typedef unsigned int u32;     // signed int is __int32_t on ESP
    typedef long long i64;
    typedef unsigned long long u64;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // ESP32-P4 is 64-bit: pointers are 64-bit
    typedef unsigned long long size;   // size_t equivalent (64-bit on ESP32-P4)
    typedef unsigned long long uptr;   // uintptr_t equivalent (64-bit on ESP32-P4)
    typedef long long ptrdiff;         // ptrdiff_t equivalent (signed pointer diff, 64-bit)
#else
    // ESP32 is 32-bit: pointers are 32-bit
    typedef unsigned long size;   // size_t equivalent (32-bit on ESP)
    typedef unsigned long uptr;   // uintptr_t equivalent (32-bit on ESP)
    typedef long ptrdiff;          // ptrdiff_t equivalent (signed pointer diff, 32-bit)
#endif
}
#else
// C language compatibility - define types in global namespace
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long long i64;
typedef unsigned long long u64;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
typedef unsigned long long size;
typedef unsigned long long uptr;
typedef long long ptrdiff;
#else
typedef unsigned long size;
typedef unsigned long uptr;
typedef long ptrdiff;
#endif
#endif
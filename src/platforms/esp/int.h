#pragma once

#include <stdint.h>
#include <stddef.h>

namespace fl {
    // ESP platforms (ESP8266/ESP32): Like ARM platforms, uint32_t and int32_t are 'unsigned long' and 'long'
    // This matches the Xtensa toolchain behavior similar to ARM toolchains
    typedef short i16;
    typedef unsigned short u16;
    //typedef long i32;
    //typedef unsigned long u32;
    typedef int32_t i32;
    typedef uint32_t u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // Use standard types directly to ensure exact compatibility
    typedef size_t size;
    typedef uintptr_t uptr;
}

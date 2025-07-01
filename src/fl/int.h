#pragma once

#include <stdint.h>  // ok include

namespace fl {
    typedef int8_t i8;
    typedef int16_t i16;
    typedef int32_t i32;
    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    
    // Compile-time verification that types are exactly the expected size
    static_assert(sizeof(i8) == 1, "i8 must be exactly 1 byte");
    static_assert(sizeof(i16) == 2, "i16 must be exactly 2 bytes");
    static_assert(sizeof(i32) == 4, "i32 must be exactly 4 bytes");
    static_assert(sizeof(u8) == 1, "u8 must be exactly 1 byte");
    static_assert(sizeof(u16) == 2, "u16 must be exactly 2 bytes");
    static_assert(sizeof(u32) == 4, "u32 must be exactly 4 bytes");
}

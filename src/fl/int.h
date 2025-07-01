#pragma once

namespace fl {
    // 8-bit types - char is reliably 8 bits on all supported platforms
    typedef signed char i8;
    typedef unsigned char u8;
    
    // 16-bit and 32-bit types - platform-specific to match stdint.h exactly
    #ifdef __AVR__
        // On AVR: int is 16-bit, long is 32-bit
        // This matches how stdint.h defines these types on AVR
        typedef int i16;
        typedef unsigned int u16;
        typedef long i32;
        typedef unsigned long u32;
    #else
        // On most other platforms: short is 16-bit, int is 32-bit
        typedef short i16;
        typedef unsigned short u16;
        typedef int i32;
        typedef unsigned int u32;
    #endif
    
    // Compile-time verification that types are exactly the expected size
    static_assert(sizeof(i8) == 1, "i8 must be exactly 1 byte");
    static_assert(sizeof(i16) == 2, "i16 must be exactly 2 bytes");
    static_assert(sizeof(i32) == 4, "i32 must be exactly 4 bytes");
    static_assert(sizeof(u8) == 1, "u8 must be exactly 1 byte");
    static_assert(sizeof(u16) == 2, "u16 must be exactly 2 bytes");
    static_assert(sizeof(u32) == 4, "u32 must be exactly 4 bytes");
}

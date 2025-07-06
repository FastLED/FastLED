#pragma once

// Standard headers needed for integer types
#include <stdint.h>  // ok include
#include <stddef.h>  // ok include

namespace fl {
    // 8-bit types - char is reliably 8 bits on all supported platforms
    // These must be defined BEFORE platform includes so fractional types can use them
    typedef signed char i8;
    typedef unsigned char u8;
    
    // Pointer and size types - universal across platforms
    typedef uintptr_t uptr;  ///< Pointer-sized unsigned integer
    typedef size_t sz;       ///< Size type for containers and memory
    
    // Compile-time verification that types are exactly the expected size
    static_assert(sizeof(i8) == 1, "i8 must be exactly 1 byte");
    static_assert(sizeof(u8) == 1, "u8 must be exactly 1 byte");
    static_assert(sizeof(uptr) >= sizeof(void*), "uptr must be at least pointer size");
    static_assert(sizeof(sz) >= sizeof(void*), "sz must be at least pointer size for large memory operations");
}

// Platform-specific integer type definitions
// This includes platform-specific 16/32/64-bit types
#include "platforms/int.h"

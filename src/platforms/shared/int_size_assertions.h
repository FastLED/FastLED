#pragma once

// This file contains size assertions for platform-specific integer types
// It should be included AFTER the platform-specific typedefs are defined

namespace fl {
    // Compile-time verification that platform-specific types are exactly the expected size
    static_assert(sizeof(i16) == 2, "i16 must be exactly 2 bytes");
    static_assert(sizeof(i32) == 4, "i32 must be exactly 4 bytes");
    static_assert(sizeof(i64) == 8, "i64 must be exactly 8 bytes");
    static_assert(sizeof(u16) == 2, "u16 must be exactly 2 bytes");
    static_assert(sizeof(u32) == 4, "u32 must be exactly 4 bytes");
    static_assert(sizeof(u64) == 8, "u64 must be exactly 8 bytes");
}

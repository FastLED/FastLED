#pragma once

#include <stdint.h>  // ok include

namespace fl {
    typedef uint16_t u16;
    
    // Compile-time verification that u16 is exactly 2 bytes
    static_assert(sizeof(u16) == 2, "u16 must be exactly 2 bytes");
}

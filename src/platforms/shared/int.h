#pragma once

#include <stdint.h>
#include <stddef.h>

namespace fl {
    // Default platform (desktop/generic): assume short 16-bit, int 32-bit (uint32_t is unsigned int)
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;             // 'int' is 32-bit on all desktop and most embedded targets
    typedef unsigned int u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // Use standard types directly to ensure exact compatibility
    typedef size_t size;
    typedef uintptr_t uptr;
}

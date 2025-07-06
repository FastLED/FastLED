#pragma once

namespace fl {
    // Default platform (desktop/generic): assume short 16-bit, int 32-bit (uint32_t is unsigned int)
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;             // 'int' is 32-bit on all desktop and most embedded targets
    typedef unsigned int u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // size_t is unsigned long on most desktop platforms (32-bit or 64-bit)
    typedef unsigned long sz;
}

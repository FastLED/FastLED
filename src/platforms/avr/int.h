#pragma once

#include <stdint.h>
#include <stddef.h>

namespace fl {
    // On AVR: int is 16-bit, long is 32-bit — match stdint sizes manually
    typedef int i16;
    typedef unsigned int u16;
    typedef long i32;
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // Use standard types directly to ensure exact compatibility
    typedef size_t size;
    typedef uintptr_t uptr;
}

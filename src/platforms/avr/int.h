#pragma once

namespace fl {
    // On AVR: int is 16-bit, long is 32-bit — match stdint sizes manually
    typedef int i16;
    typedef unsigned int u16;
    typedef long i32;
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
}

// Include size assertions after platform-specific typedefs
#include "platforms/shared/int_size_assertions.h"

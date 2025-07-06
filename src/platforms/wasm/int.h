#pragma once

namespace fl {
    // WebAssembly / Emscripten: Default desktop-like type mapping
    // short is 16-bit, int is 32-bit (uint32_t is unsigned int)
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;
    typedef long long i64;
    typedef unsigned long long u64;
}

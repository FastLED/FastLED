#pragma once

#ifdef __cplusplus
namespace fl {
    // ESP8266: Xtensa LX106 toolchain
    // 32-bit platform: short is 16-bit, int is 32-bit
    // ESP8266 uses 'int' for all 32-bit types (not 'long')
    typedef short i16;
    typedef unsigned short u16;
    typedef int i32;
    typedef unsigned int u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // ESP8266: size_t and pointers are 32-bit using 'unsigned int'
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int ptrdiff;
}
#else
// C language compatibility - define types in global namespace
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long long i64;
typedef unsigned long long u64;
typedef unsigned int size;
typedef unsigned int uptr;
typedef int ptrdiff;
#endif

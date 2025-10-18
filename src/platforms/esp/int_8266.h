#pragma once

///////////////////////////////////////////////////////////////////////////////
// ESP8266 Platform Integer Type Definitions (Xtensa LX106 toolchain)
//
// This file defines types for both C and C++. The definitions are logically
// identical; only the namespace wrapper differs between the two languages.
//
// For C++: Types are defined in namespace fl { ... }
// For C: Types are defined in global scope
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// ESP8266 INTEGER TYPE DEFINITIONS (all variants identical)
// ============================================================================
// ESP8266: 32-bit platform, short is 16-bit, int is 32-bit
// Macros avoid duplication between C++ namespace and C global scope
#define DEFINE_ESP8266_INT_TYPES \
    typedef short i16; \
    typedef unsigned short u16; \
    typedef int i32; \
    typedef unsigned int u32; \
    typedef long long i64; \
    typedef unsigned long long u64;

// ============================================================================
// POINTER AND SIZE TYPE DEFINITIONS
// ============================================================================
// ESP8266: size_t and pointers are 32-bit using 'unsigned int'
#define DEFINE_ESP8266_POINTER_TYPES \
    typedef unsigned int size;     /* matches __SIZE_TYPE__ */ \
    typedef unsigned int uptr;     /* matches __uintptr_t */ \
    typedef int iptr;              /* matches __intptr_t */ \
    typedef int ptrdiff;           /* matches __PTRDIFF_TYPE__ */

///////////////////////////////////////////////////////////////////////////////
// C++ LANGUAGE SUPPORT
///////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
namespace fl {
    // ESP8266: Xtensa LX106 toolchain
    // 32-bit platform: short is 16-bit, int is 32-bit
    // ESP8266 uses 'int' for all 32-bit types (not 'long')
    DEFINE_ESP8266_INT_TYPES

    // ESP8266: size_t and pointers are 32-bit using 'unsigned int'
    DEFINE_ESP8266_POINTER_TYPES
}

///////////////////////////////////////////////////////////////////////////////
// C LANGUAGE SUPPORT
///////////////////////////////////////////////////////////////////////////////
#else
// C language compatibility - define types in global namespace
// Same type definitions as C++ namespace, but without namespace wrapper
DEFINE_ESP8266_INT_TYPES

// Pointer and size types
DEFINE_ESP8266_POINTER_TYPES

#endif  // __cplusplus

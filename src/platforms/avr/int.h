#pragma once

// AVR Platform Standard Integer Types
// Define standard stdint.h types BEFORE fl/stdint.h tries to define them
// AVR toolchain uses 'int' (not 'short') for 16-bit types
// Use guards to prevent redefinition conflicts

#ifndef __UINT8_T_DEFINED__
#define __UINT8_T_DEFINED__
typedef unsigned char uint8_t;
typedef signed char int8_t;
#endif

#ifndef __UINT16_T_DEFINED__
#define __UINT16_T_DEFINED__
typedef unsigned int uint16_t;   // AVR: int is 16-bit
typedef int int16_t;              // AVR: int is 16-bit
#endif

#ifndef __UINT32_T_DEFINED__
#define __UINT32_T_DEFINED__
typedef unsigned long uint32_t;   // AVR: long is 32-bit
typedef long int32_t;             // AVR: long is 32-bit
#endif

#ifndef __UINT64_T_DEFINED__
#define __UINT64_T_DEFINED__
typedef unsigned long long uint64_t;
typedef long long int64_t;
#endif

#ifndef __SIZE_T_DEFINED__
#define __SIZE_T_DEFINED__
typedef unsigned int size_t;      // AVR: 16-bit pointers
#endif

#ifndef __UINTPTR_T_DEFINED__
#define __UINTPTR_T_DEFINED__
typedef unsigned int uintptr_t;   // AVR: 16-bit pointers
#endif

#ifndef __PTRDIFF_T_DEFINED__
#define __PTRDIFF_T_DEFINED__
typedef int ptrdiff_t;            // AVR: 16-bit pointer difference
#endif

// stdint.h limit macros
#ifndef INT8_MIN
#define INT8_MIN   (-128)
#define INT16_MIN  (-32767-1)
#define INT32_MIN  (-2147483647-1)
#define INT64_MIN  (-9223372036854775807LL-1)
#define INT8_MAX   127
#define INT16_MAX  32767
#define INT32_MAX  2147483647
#define INT64_MAX  9223372036854775807LL
#define UINT8_MAX  0xFF
#define UINT16_MAX 0xFFFF
#define UINT32_MAX 0xFFFFFFFFUL
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

namespace fl {
    // On AVR: int is 16-bit, long is 32-bit — match stdint sizes manually
    typedef int i16;
    typedef unsigned int u16;
    typedef long i32;
    typedef unsigned long u32;
    typedef long long i64;
    typedef unsigned long long u64;
    // AVR is 8-bit: pointers are 16-bit
    typedef unsigned int size;   // size_t equivalent (16-bit on AVR)
    typedef unsigned int uptr;   // uintptr_t equivalent (16-bit on AVR)
    typedef int ptrdiff;         // ptrdiff_t equivalent (16-bit on AVR)
}

#pragma once

#ifdef __cplusplus
namespace fl {
    // ESP platforms: Xtensa and RISC-V toolchains
    // Each variant has separate definitions to allow independent tuning

    // Common 16-bit and 64-bit types (same across all ESP32 variants)
    typedef short i16;
    typedef unsigned short u16;
    typedef long long i64;
    typedef unsigned long long u64;

    // 32-bit integer types and pointer types vary by platform
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // ============================================================
    // ESP32-P4: 64-bit RISC-V platform
    // ============================================================
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned long long size;   // size_t equivalent (64-bit)
    typedef unsigned long long uptr;   // uintptr_t equivalent (64-bit)
    typedef long long iptr;            // intptr_t equivalent (64-bit)
    typedef long long ptrdiff;         // ptrdiff_t equivalent (64-bit)

#elif defined(CONFIG_IDF_TARGET_ESP32)
    // ============================================================
    // ESP32 (original): 32-bit Xtensa LX6 dual-core
    // ============================================================
    typedef long i32;              // long matches __int32_t
    typedef unsigned long u32;     // unsigned long matches __uint32_t
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    // ============================================================
    // ESP32-S2: 32-bit Xtensa LX7 single-core
    // ============================================================
    typedef long i32;              // long matches __int32_t
    typedef unsigned long u32;     // unsigned long matches __uint32_t
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // ============================================================
    // ESP32-S3: 32-bit Xtensa LX7 dual-core with vector extensions
    // ============================================================
    typedef long i32;              // long matches __int32_t
    typedef unsigned long u32;     // unsigned long matches __uint32_t
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    // ============================================================
    // ESP32-C3: 32-bit RISC-V single-core
    // ============================================================
    typedef long i32;              // long matches __int32_t
    typedef unsigned long u32;     // unsigned long matches __uint32_t
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    // ============================================================
    // ESP32-C6: 32-bit RISC-V with WiFi 6
    // ============================================================
    typedef long i32;              // long matches __int32_t
    typedef unsigned long u32;     // unsigned long matches __uint32_t
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    // ============================================================
    // ESP32-H2: 32-bit RISC-V with Bluetooth 5.2 LE
    // ============================================================
    typedef long i32;              // long matches __int32_t
    typedef unsigned long u32;     // unsigned long matches __uint32_t
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#else
    // ============================================================
    // ESP8266 or unknown ESP variant: 32-bit Xtensa (fallback)
    // ============================================================
    typedef long i32;              // long matches __int32_t
    typedef unsigned long u32;     // unsigned long matches __uint32_t
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__
#endif
}
#else
// C language compatibility - define types in global namespace

// Common types across all ESP32 variants
typedef short i16;
typedef unsigned short u16;
typedef long long i64;
typedef unsigned long long u64;

// Platform-specific 32-bit and pointer types
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // ESP32-P4: 64-bit RISC-V
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned long long size;
    typedef unsigned long long uptr;
    typedef long long iptr;
    typedef long long ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32 (original): 32-bit Xtensa LX6
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    // ESP32-S2: 32-bit Xtensa LX7
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S3: 32-bit Xtensa LX7
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-C3: 32-bit RISC-V
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32-C6: 32-bit RISC-V
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-H2: 32-bit RISC-V
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#else
    // ESP8266 or unknown ESP variant (fallback)
    typedef long i32;
    typedef unsigned long u32;
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;
#endif
#endif
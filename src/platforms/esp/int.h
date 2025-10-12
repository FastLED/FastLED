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

    // 32-bit types: Use compiler built-ins to match system stdint.h exactly
    // __INT32_TYPE__ and __UINT32_TYPE__ are defined by GCC and vary by toolchain/IDF version
    // This ensures our types match the system's __int32_t and __uint32_t
    //
    // Special handling for ESP32 IDF < 4.0 (including 3.3):
    // On these versions, Arduino.h includes system stdint.h which defines uint32_t/int32_t
    // as typedefs of __uint32_t/__int32_t. We must use the same base types to avoid
    // "conflicting declaration" errors when fl/stdint.h creates its typedefs.
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  #include "../esp/esp_version.h"
  #if !defined(ESP_IDF_VERSION) || !ESP_IDF_VERSION_4_OR_HIGHER
    // IDF 3.3 or unknown: Use system __int32_t/__uint32_t types directly
    // This matches what system stdint.h uses, preventing typedef conflicts
    typedef __int32_t i32;
    typedef __uint32_t u32;
  #elif defined(__INT32_TYPE__) && defined(__UINT32_TYPE__)
    typedef __INT32_TYPE__ i32;
    typedef __UINT32_TYPE__ u32;
  #else
    typedef int i32;
    typedef unsigned int u32;
  #endif
#elif defined(__INT32_TYPE__) && defined(__UINT32_TYPE__)
    typedef __INT32_TYPE__ i32;
    typedef __UINT32_TYPE__ u32;
#else
    // Fallback if compiler doesn't define these (shouldn't happen on modern GCC)
    typedef int i32;
    typedef unsigned int u32;
#endif

    // Pointer and size types vary by platform
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // ============================================================
    // ESP32-P4: 64-bit RISC-V platform
    // ============================================================
    typedef unsigned long long size;   // size_t equivalent (64-bit)
    typedef unsigned long long uptr;   // uintptr_t equivalent (64-bit)
    typedef long long iptr;            // intptr_t equivalent (64-bit)
    typedef long long ptrdiff;         // ptrdiff_t equivalent (64-bit)

#elif defined(CONFIG_IDF_TARGET_ESP32)
    // ============================================================
    // ESP32 (original): 32-bit Xtensa LX6 dual-core
    // ============================================================
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    // ============================================================
    // ESP32-S2: 32-bit Xtensa LX7 single-core
    // ============================================================
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // ============================================================
    // ESP32-S3: 32-bit Xtensa LX7 dual-core with vector extensions
    // ============================================================
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    // ============================================================
    // ESP32-C3: 32-bit RISC-V single-core
    // ============================================================
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    // ============================================================
    // ESP32-C6: 32-bit RISC-V with WiFi 6
    // ============================================================
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    // ============================================================
    // ESP32-H2: 32-bit RISC-V with Bluetooth 5.2 LE
    // ============================================================
    typedef unsigned int size;     // unsigned int matches __SIZE_TYPE__
    typedef unsigned int uptr;     // unsigned int matches __uintptr_t
    typedef int iptr;              // int matches __intptr_t
    typedef int ptrdiff;           // int matches __PTRDIFF_TYPE__

#else
    // ============================================================
    // ESP8266 or unknown ESP variant: 32-bit Xtensa (fallback)
    // ============================================================
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

// 32-bit types: Use compiler built-ins to match system stdint.h exactly
// Special handling for ESP32 IDF < 4.0 (same as C++ section above)
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  #include "../esp/esp_version.h"
  #if !defined(ESP_IDF_VERSION) || !ESP_IDF_VERSION_4_OR_HIGHER
    // IDF 3.3 or unknown: Use system __int32_t/__uint32_t types directly
    typedef __int32_t i32;
    typedef __uint32_t u32;
  #elif defined(__INT32_TYPE__) && defined(__UINT32_TYPE__)
    typedef __INT32_TYPE__ i32;
    typedef __UINT32_TYPE__ u32;
  #else
    typedef int i32;
    typedef unsigned int u32;
  #endif
#elif defined(__INT32_TYPE__) && defined(__UINT32_TYPE__)
    typedef __INT32_TYPE__ i32;
    typedef __UINT32_TYPE__ u32;
#else
    typedef int i32;
    typedef unsigned int u32;
#endif

// Platform-specific pointer and size types
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // ESP32-P4: 64-bit RISC-V
    typedef unsigned long long size;
    typedef unsigned long long uptr;
    typedef long long iptr;
    typedef long long ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32 (original): 32-bit Xtensa LX6
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    // ESP32-S2: 32-bit Xtensa LX7
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S3: 32-bit Xtensa LX7
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-C3: 32-bit RISC-V
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32-C6: 32-bit RISC-V
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-H2: 32-bit RISC-V
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;

#else
    // ESP8266 or unknown ESP variant (fallback)
    typedef unsigned int size;
    typedef unsigned int uptr;
    typedef int iptr;
    typedef int ptrdiff;
#endif
#endif
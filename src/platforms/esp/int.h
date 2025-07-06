#pragma once


namespace fl {
    // ESP32: short is 16-bit, long is 32-bit (same as Teensy but separate macro for clarity)
    // Note: On ESP-IDF 3.3 and older, uint32_t is 'unsigned int', not 'unsigned long'
    #if ESP_IDF_VERSION_MAJOR >= 4
        // ESP-IDF 4.0+ uses 'long' for 32-bit types
        typedef short i16;
        typedef unsigned short u16;
        typedef long i32;
        typedef long unsigned int u32;
        typedef long long i64;
        typedef unsigned long long u64;
    #else
        // ESP-IDF 3.x uses 'int' for 32-bit types
        typedef short i16;
        typedef unsigned short u16;
        typedef int i32;
        typedef unsigned int u32;
        typedef long long i64;
        typedef unsigned long long u64;
    #endif
}

// Include size assertions after platform-specific typedefs
#include "platforms/shared/int_size_assertions.h"

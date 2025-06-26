#pragma once

#include <stddef.h>
#include "fl/stdint.h"

// Teensy has usb_serial_putchar for direct USB output
extern "C" {
int usb_serial_putchar(uint8_t c);
int usb_serial_write(const void *buffer, uint32_t size);
}

namespace fl {

// Helper function to calculate string length without depending on strlen
// Only used for Teensy platforms
static size_t strlen_p(const char* str) {
    if (!str) return 0;
    size_t len = 0;
    while (*str++) len++;
    return len;
}

inline void print_teensy(const char* str) {
    if (!str) return;
    
    // Teensy: Use native USB serial first
    if (usb_serial_write(str, strlen_p(str)) >= 0) {
        // Success with native USB
    } else {
        // Fallback to Arduino Serial if available
        #ifdef ARDUINO_H
        if (Serial) {
            Serial.print(str);
        }
        #endif
    }
}

inline void println_teensy(const char* str) {
    if (!str) return;
    print_teensy(str);
    print_teensy("\n");
}

} // namespace fl

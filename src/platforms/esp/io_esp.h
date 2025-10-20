#pragma once

// ESP32 and ESP8266 use Arduino Serial library for I/O
#define IO_HAS_IMPL 1

namespace fl {

// Print functions using Arduino Serial
inline void print_esp(const char* str) {
    if (!str) return;

#if IO_HAS_IMPL
    // ESP32/ESP8266: Use Arduino Serial library
    // This is lightweight and avoids POSIX stdio linker issues
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.print(str);
    }
    #endif
#endif
}

inline void println_esp(const char* str) {
    if (!str) return;

#if IO_HAS_IMPL
    // ESP32/ESP8266: Use Arduino Serial library with newline
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.println(str);
    }
    #endif
#endif
}

// Input functions
inline int available_esp() {
    // ESP platforms - check Serial availability
    return 0;
}

inline int read_esp() {
    return -1;
}

} // namespace fl

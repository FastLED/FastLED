#pragma once

namespace fl {

// Print functions
inline void print_arduino(const char* str) {
    if (!str) return;
    
    // Generic Arduino platforms and final fallback
    // Only use Serial if Arduino.h has been included and Serial is available
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.print(str);
    }
    #endif
    // If no Serial available, output goes nowhere (silent)
    // This prevents crashes on platforms where Serial isn't initialized
}

inline void println_arduino(const char* str) {
    if (!str) return;
    print_arduino(str);
    print_arduino("\n");
}

// Input functions
inline int available_arduino() {
    // Generic Arduino platforms - check Serial availability
    #ifdef ARDUINO_H
    if (Serial) {
        return Serial.available();
    }
    #endif
    return 0;
}

inline int read_arduino() {
    // Generic Arduino platforms - read from Serial
    #ifdef ARDUINO_H
    if (Serial && Serial.available()) {
        return Serial.read();
    }
    #endif
    return -1;
}

} // namespace fl

#pragma once

namespace fl {

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

} // namespace fl

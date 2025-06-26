#pragma once

namespace fl {

// Low-level Teensy print functions that avoid printf dependencies
// These use direct UART/USB functions for maximum efficiency

inline void print_teensy(const char* str) {
    if (!str) return;
    
#if defined(__IMXRT1062__)
    // Teensy 4.x (4.0, 4.1) - Use direct USB serial functions
    // Only use Serial if Arduino.h has been included and Serial is available
    #ifdef ARDUINO_H
    if (Serial) {
        // Use write() instead of print() to avoid any printf dependencies
        Serial.write(str);
    }
    #endif
    
#elif defined(__MK66FX1M0__) || defined(__MK64FX512__)
    // Teensy 3.5/3.6 - Use direct USB serial functions
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.write(str);
    }
    #endif
    
#elif defined(__MK20DX256__) || defined(__MK20DX128__)
    // Teensy 3.0/3.1/3.2 - Use direct USB serial functions
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.write(str);
    }
    #endif
    
#elif defined(__MKL26Z64__)
    // Teensy LC - Use direct USB serial functions
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.write(str);
    }
    #endif
    
#else
    // Fallback for other Teensy platforms
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.write(str);
    }
    #endif
#endif
}

inline void println_teensy(const char* str) {
    if (!str) return;
    print_teensy(str);
    print_teensy("\n");
}

// Input functions using direct Teensy USB functions
inline int available_teensy() {
#if defined(__IMXRT1062__) || defined(__MK66FX1M0__) || defined(__MK64FX512__) || defined(__MK20DX256__) || defined(__MK20DX128__) || defined(__MKL26Z64__)
    // All Teensy platforms with USB serial
    #ifdef ARDUINO_H
    if (Serial) {
        return Serial.available();
    }
    #endif
#endif
    return 0;
}

inline int read_teensy() {
#if defined(__IMXRT1062__) || defined(__MK66FX1M0__) || defined(__MK64FX512__) || defined(__MK20DX256__) || defined(__MK20DX128__) || defined(__MKL26Z64__)
    // All Teensy platforms with USB serial
    #ifdef ARDUINO_H
    if (Serial && Serial.available()) {
        return Serial.read();
    }
    #endif
#endif
    return -1;
}

} // namespace fl

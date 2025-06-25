#include "io.h"

// Platform-specific includes for low-level output
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#elif defined(_WIN32)
#include <io.h>  // for _write
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__)
#include <unistd.h>  // for write
#endif

namespace fl {

void print(const char* str) {
    if (!str) return;

#ifdef __EMSCRIPTEN__
    // WASM: Use JavaScript console.log
    ::printf(str);
#elif defined(ARDUINO) || defined(__AVR__) || defined(ESP32) || defined(ESP8266) || defined(__IMXRT1062__)
    // Arduino/AVR/ESP32/ESP8266/Teensy: Use Serial if available
    // Only attempt to use Serial if Arduino.h has been included
    // and Serial is available. The typeof check helps ensure Serial is defined.
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.print(str);
    }
    #endif
    // If ARDUINO_H not included or Serial not available, do nothing

#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    // Native/Testing: Use direct system calls to stderr
    // Calculate length without strlen()
    size_t len = 0;
    const char* p = str;
    while (*p++) len++;
    
#ifdef _WIN32
    // Windows version
    _write(2, str, len);  // 2 = stderr
#else
    // Unix/Linux version
    ::write(2, str, len);  // 2 = stderr
#endif

#else
    // Fallback: Use minimal output (could be platform-specific UART)
    // Minimal fallback - could be extended for other platforms
    (void)str; // Suppress unused warning
#endif
}

void println(const char* str) {
    if (!str) return;
    print(str);
    print("\n");
}
}

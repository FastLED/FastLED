// ESP I/O implementation - Arduino Serial backend
// This file is included by io_esp.cpp when using Arduino backend

#include "Arduino.h"  // ok include

namespace fl {

// Print functions using Arduino Serial
void print_esp(const char* str) {
    if (!str) return;

    // ESP32/ESP8266: Use Arduino Serial library
    // This is lightweight and avoids POSIX stdio linker issues
    Serial.print(str);
}

void println_esp(const char* str) {
    if (!str) return;

    // ESP32/ESP8266: Use Arduino Serial library with newline
    Serial.println(str);
}

// Input functions
int available_esp() {
    // ESP platforms - check Serial availability
    return Serial.available();
}

int read_esp() {
    return Serial.read();
}

} // namespace fl

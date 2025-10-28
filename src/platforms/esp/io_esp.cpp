#include "Arduino.h"  // ok include
#include "io_esp.h"

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
    return 0;
}

int read_esp() {
    return -1;
}

} // namespace fl

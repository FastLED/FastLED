#pragma once

// arduino_before.h saves Serial macro as ArduinoSerial_Save before undefining it
#include "fl/stl/arduino_before.h"

namespace fl {
namespace platforms {

// Serial initialization
void begin(uint32_t baudRate) {
    ArduinoSerial_Save.begin(baudRate);
}

// Print functions
void print(const char* str) {
    ArduinoSerial_Save.print(str);
}

void println(const char* str) {
    ArduinoSerial_Save.println(str);
}

// Input functions
int available() {
    return ArduinoSerial_Save.available();
}

int peek() {
    return ArduinoSerial_Save.peek();
}

int read() {
    return ArduinoSerial_Save.read();
}

// Utility functions
bool flush(uint32_t timeoutMs) {
    ArduinoSerial_Save.flush();
    return true;
}

bool serial_ready() {
    return (bool)ArduinoSerial_Save;
}

// Binary write function
size_t write_bytes(const uint8_t* buffer, size_t size) {
    return ArduinoSerial_Save.write(buffer, size);
}

// Test/diagnostic helper: Arduino Serial is always "buffered" (not ROM UART)
bool serial_is_buffered() {
    return true;  // Arduino Serial is always buffered
}

} // namespace platforms
} // namespace fl

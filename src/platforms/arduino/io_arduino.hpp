#pragma once

// arduino_before.h saves Serial macro as ArduinoSerial_Save before undefining it
#include <Arduino.h>

namespace fl {
namespace platforms {

// Serial initialization
void begin(uint32_t baudRate) {
    Serial.begin(baudRate);
}

// Print functions
void print(const char* str) {
    Serial.print(str);
}

void println(const char* str) {
    Serial.println(str);
}

// Input functions
int available() {
    return Serial.available();
}

int peek() {
    return Serial.peek();
}

int read() {
    return Serial.read();
}

// Utility functions
bool flush(uint32_t timeoutMs) {
    Serial.flush();
    return true;
}

bool serial_ready() {
    return (bool)Serial;
}

// Binary write function
size_t write_bytes(const uint8_t* buffer, size_t size) {
    return Serial.write(buffer, size);
    //return 0;
}

// Test/diagnostic helper: Arduino Serial is always "buffered" (not ROM UART)
bool serial_is_buffered() {
    return true;  // Arduino Serial is always buffered
}

} // namespace platforms
} // namespace fl

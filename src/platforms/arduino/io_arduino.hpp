#pragma once

// IWYU pragma: private

// arduino_before.h saves Serial macro as ArduinoSerial_Save before undefining it
#include <Arduino.h>

namespace fl {
namespace platforms {

// Serial initialization
void begin(u32 baudRate) {
    Serial.begin(baudRate);
}

// Print functions
void print(const char* str) {
    if (!Serial) return;  // Non-blocking: skip if USB disconnected
    Serial.print(str);
}

void println(const char* str) {
    if (!Serial) return;  // Non-blocking: skip if USB disconnected
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

// High-level line reading using Arduino's Serial.readStringUntil()
// This handles USB CDC multi-packet transfers correctly via Stream::timedRead()
// which uses yield() (immediate context switch) instead of delay(1) (1ms sleep).
int readLineNative(char delimiter, char* out, int outLen) {
    String line = Serial.readStringUntil(delimiter);
    int len = line.length();
    if (len > outLen - 1) len = outLen - 1;
    memcpy(out, line.c_str(), len);
    out[len] = '\0';
    return len;
}

// Utility functions
bool flush(u32 timeoutMs) {
    Serial.flush();
    return true;
}

bool serial_ready() {
    return (bool)Serial;
}

// Binary write function
size_t write_bytes(const u8* buffer, size_t size) {
    return Serial.write(buffer, size);
    //return 0;
}

// Test/diagnostic helper: Arduino Serial is always "buffered" (not ROM UART)
bool serial_is_buffered() {
    return true;  // Arduino Serial is always buffered
}

} // namespace platforms
} // namespace fl

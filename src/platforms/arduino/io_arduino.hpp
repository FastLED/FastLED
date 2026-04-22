#pragma once

// IWYU pragma: private

// fl/system/arduino.h trampoline ensures Arduino.h + macro cleanup
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
#include "fl/stl/noexcept.h"
#include "platforms/avr/is_avr.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

// Serial initialization
void begin(u32 baudRate) FL_NOEXCEPT {
    Serial.begin(baudRate);
}

// Print functions
void print(const char* str) FL_NOEXCEPT {
    if (!Serial) return;  // Non-blocking: skip if USB disconnected
    Serial.print(str);
}

void println(const char* str) FL_NOEXCEPT {
    if (!Serial) return;  // Non-blocking: skip if USB disconnected
    Serial.println(str);
}

// Input functions
int available() FL_NOEXCEPT {
    return Serial.available();
}

int peek() FL_NOEXCEPT {
    return Serial.peek();
}

int read() FL_NOEXCEPT {
    return Serial.read();
}

// High-level line reading using Arduino's Serial.readStringUntil()
// This handles USB CDC multi-packet transfers correctly via Stream::timedRead()
// which uses yield() (immediate context switch) instead of delay(1) (1ms sleep).
int readLineNative(char delimiter, char* out, int outLen) FL_NOEXCEPT {
    if (outLen <= 0) {
        return 0;
    }
#if defined(FL_IS_AVR_ATTINY)
    // TinyDebugSerial lacks readStringUntil(), so keep Stream's blocking
    // line-read behavior without allocating Arduino String.
    const unsigned long timeoutMs = 1000UL;
    unsigned long start = millis();
    int len = 0;
    while (true) {
        if (!Serial.available()) {
            if (millis() - start >= timeoutMs) {
                break;
            }
            yield();
            continue;
        }
        const int value = Serial.read();
        if (value < 0) {
            break;
        }
        const char c = static_cast<char>(value);
        if (c == delimiter) {
            break;
        }
        if (len < outLen - 1) {
            out[len++] = c;
        }
        start = millis();
    }
    out[len] = '\0';
    return len;
#else
    String line = Serial.readStringUntil(delimiter);
    int len = line.length();
    if (len > outLen - 1) len = outLen - 1;
    memcpy(out, line.c_str(), len);
    out[len] = '\0';
    return len;
#endif
}

// Utility functions
bool flush(u32 timeoutMs) FL_NOEXCEPT {
    Serial.flush();
    return true;
}

bool serial_ready() FL_NOEXCEPT {
    return (bool)Serial;
}

// Binary write function
size_t write_bytes(const u8* buffer, size_t size) FL_NOEXCEPT {
    return Serial.write(buffer, size);
    //return 0;
}

// Test/diagnostic helper: Arduino Serial is always "buffered" (not ROM UART)
bool serial_is_buffered() FL_NOEXCEPT {
    return true;  // Arduino Serial is always buffered
}

} // namespace platforms
} // namespace fl

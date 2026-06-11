#pragma once

// IWYU pragma: private
#include "platforms/arm/lpc/is_lpc.h"

// NXP LPC845/LPC804 with Arduino framework - uses standard Serial API
// Most functions can use the generic Arduino implementation, but readLineNative
// needs custom handling since LPC's HardwareSerial lacks readStringUntil().
#if defined(FL_LPC845) || defined(FL_LPC804) || defined(FL_LPC11_USB) || defined(FL_LPC15) || defined(FL_LPC11_LEGACY)

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

// Serial initialization
void begin(u32 baudRate) FL_NOEXCEPT {
    Serial.begin(baudRate);
}

// Print functions
void print(const char* str) FL_NOEXCEPT {
    if (!Serial) return;
    Serial.print(str);
}

void println(const char* str) FL_NOEXCEPT {
    if (!Serial) return;
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

// LPC's HardwareSerial lacks readStringUntil(), so we implement line reading manually
int readLineNative(char delimiter, char* out, int outLen) FL_NOEXCEPT {
    if (outLen <= 0) {
        return 0;
    }
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
}

// Utility functions
bool flush(u32 timeoutMs) FL_NOEXCEPT {
    (void)timeoutMs;
    if (!Serial) return true;
    Serial.flush();
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NOEXCEPT {
    if (!Serial) return 0;
    return Serial.write(buffer, size);
}

bool serial_ready() FL_NOEXCEPT {
    return Serial ? true : false;
}

bool serial_is_buffered() FL_NOEXCEPT {
    return true;  // LPC Arduino uses buffered UART
}

} // namespace platforms
} // namespace fl

#endif

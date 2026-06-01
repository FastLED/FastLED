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
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    // ESP32 HWCDC/USBCDC: make writes drop instead of block when no host is reading.
    // Default is 100ms (HWCDC) / 250ms (USBCDC). Setting to 0 collapses worst-case
    // stall from ~2s to ~0 when host absent. See FastLED issue #2668 and
    // arduino-esp32 PR #7583. Safe to call on any Serial that exposes the method.
    Serial.setTxTimeoutMs(0);
#endif
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
    (void)timeoutMs;
    // Skip flush when host is absent. On HWCDC, Serial.flush() can spin
    // indefinitely without a host (arduino-esp32 issue #7554). Returning
    // true is correct semantics: there is no drained-to-host invariant
    // we can establish without a host. See FastLED issue #2668.
    if (!Serial) return true;
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

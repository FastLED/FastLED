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
void begin(u32 baudRate) FL_NO_EXCEPT {
    Serial.begin(baudRate);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    // ESP32 HWCDC/USBCDC: make writes drop instead of block when no host is reading.
    // Default is 100ms (HWCDC) / 250ms (USBCDC). Setting to 0 collapses worst-case
    // stall from ~2s to ~0 when host absent. See FastLED issue #2668 and
    // arduino-esp32 PR #7583. Safe to call on any Serial that exposes the method.
    Serial.setTxTimeoutMs(0);
#endif
}

// Ride out the Teensy 4 USB-CDC DTR debounce window before deciding
// "host absent". Teensy 4's `Serial.operator bool()` returns false
// for a hard-coded 15ms after every DTR transition (line discipline
// settle, see usb_serial.h:166-169 in Teensyduino core). Without this
// retry, the first messages emitted by the device after a host
// re-opens the serial port (e.g. autoresearch RPC reconnecting for
// the next test session) are silently dropped -- visible as "missing
// log lines between RPC sessions" or "first FL_WARN after reconnect
// never arrives". 30ms is 2× the debounce so we ride through the
// window with margin. Bounded so genuinely-absent-host pays at most
// 30ms per call instead of hanging.
//
// On non-Teensy Arduino platforms `Serial.operator bool()` either
// always returns true (real UART) or uses its own debounce (ESP32
// HWCDC: 100ms via setTxTimeoutMs). The 30ms bound is short enough
// to be invisible on real UARTs (instant true → no wait) and short
// enough on ESP32 to fall through to the existing setTxTimeoutMs(0)
// drop path applied by begin().
inline bool waitForSerialReady_DtrSettle() FL_NO_EXCEPT {
    if (Serial) return true;  // Common-case fast path: 1 check, no wait.
    constexpr u32 kDtrSettleDeadlineMs = 30;  // 2× Teensy 4 debounce
    const u32 t0 = millis();
    while (!Serial) {
        if ((u32)(millis() - t0) >= kDtrSettleDeadlineMs) {
            return false;  // Host truly absent -- caller drops the message.
        }
        yield();
    }
    return true;
}

// Print functions
void print(const char* str) FL_NO_EXCEPT {
    if (!waitForSerialReady_DtrSettle()) return;  // Non-blocking: skip if USB disconnected (after DTR settle wait)
    Serial.print(str);
}

void println(const char* str) FL_NO_EXCEPT {
    if (!waitForSerialReady_DtrSettle()) return;  // Non-blocking: skip if USB disconnected (after DTR settle wait)
    Serial.println(str);
}

// Input functions
int available() FL_NO_EXCEPT {
    return Serial.available();
}

int peek() FL_NO_EXCEPT {
    return Serial.peek();
}

int read() FL_NO_EXCEPT {
    return Serial.read();
}

// High-level line reading using Arduino's Serial.readStringUntil()
// This handles USB CDC multi-packet transfers correctly via Stream::timedRead()
// which uses yield() (immediate context switch) instead of delay(1) (1ms sleep).
int readLineNative(char delimiter, char* out, int outLen) FL_NO_EXCEPT {
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
bool flush(u32 timeoutMs) FL_NO_EXCEPT {
    (void)timeoutMs;
    // Skip flush when host is absent. On HWCDC, Serial.flush() can spin
    // indefinitely without a host (arduino-esp32 issue #7554). Returning
    // true is correct semantics: there is no drained-to-host invariant
    // we can establish without a host. See FastLED issue #2668.
    if (!Serial) return true;
    Serial.flush();
    return true;
}

bool serial_ready() FL_NO_EXCEPT {
    return (bool)Serial;
}

// Binary write function
size_t write_bytes(const u8* buffer, size_t size) FL_NO_EXCEPT {
    return Serial.write(buffer, size);
    //return 0;
}

// Test/diagnostic helper: Arduino Serial is always "buffered" (not ROM UART)
bool serial_is_buffered() FL_NO_EXCEPT {
    return true;  // Arduino Serial is always buffered
}

} // namespace platforms
} // namespace fl

#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"

#ifdef FL_IS_ARM_LPC

#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "platforms/arm/is_arm.h"
#include "platforms/io.h"

#if defined(ARDUINO)
// LPC Arduino core exposes the standard Serial API. LPC's HardwareSerial lacks
// readStringUntil(), so readLineNative() is implemented by hand below.
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
#include "fl/system/yield.h"
#include "fl/stl/chrono.h"
// IWYU pragma: end_keep
#endif

namespace fl {
namespace platforms {

#if defined(ARDUINO)

void begin(u32 baudRate) FL_NOEXCEPT {
    Serial.begin(baudRate);
}

void print(const char* str) FL_NOEXCEPT {
    if (!Serial) return;
    Serial.print(str);
}

void println(const char* str) FL_NOEXCEPT {
    if (!Serial) return;
    Serial.println(str);
}

int available() FL_NOEXCEPT {
    return Serial.available();
}

int peek() FL_NOEXCEPT {
    return Serial.peek();
}

int read() FL_NOEXCEPT {
    return Serial.read();
}

// LPC's HardwareSerial lacks readStringUntil(), so read the line manually.
int readLineNative(char delimiter, char* out, int outLen) FL_NOEXCEPT {
    if (outLen <= 0) {
        return 0;
    }
    const unsigned long timeoutMs = 1000UL;
    unsigned long start = fl::millis();
    int len = 0;
    while (true) {
        if (!Serial.available()) {
            if (fl::millis() - start >= timeoutMs) {
                break;
            }
            fl::yield();
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
        start = fl::millis();
    }
    out[len] = '\0';
    return len;
}

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

#else  // bare-metal: no Serial available yet

void begin(u32 baudRate) FL_NOEXCEPT {
    (void)baudRate;
}

void print(const char* str) FL_NOEXCEPT {
    (void)str;
}

void println(const char* str) FL_NOEXCEPT {
    (void)str;
}

int available() FL_NOEXCEPT {
    return 0;
}

int peek() FL_NOEXCEPT {
    return -1;
}

int read() FL_NOEXCEPT {
    return -1;
}

int readLineNative(char delimiter, char* out, int outLen) FL_NOEXCEPT {
    (void)delimiter;
    (void)out;
    (void)outLen;
    return -1;
}

bool flush(u32 timeoutMs) FL_NOEXCEPT {
    (void)timeoutMs;
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NOEXCEPT {
    (void)buffer;
    (void)size;
    return 0;
}

bool serial_ready() FL_NOEXCEPT {
    return false;
}

bool serial_is_buffered() FL_NOEXCEPT {
    return false;
}

#endif  // ARDUINO

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ARM_LPC

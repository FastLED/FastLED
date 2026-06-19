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

void begin(u32 baudRate) FL_NO_EXCEPT {
    Serial.begin(baudRate);
}

void print(const char* str) FL_NO_EXCEPT {
    if (!Serial) return;
    Serial.print(str);
}

void println(const char* str) FL_NO_EXCEPT {
    if (!Serial) return;
    Serial.println(str);
}

int available() FL_NO_EXCEPT {
    return Serial.available();
}

int peek() FL_NO_EXCEPT {
    return Serial.peek();
}

int read() FL_NO_EXCEPT {
    return Serial.read();
}

// LPC's HardwareSerial lacks readStringUntil(), so read the line manually.
int readLineNative(char delimiter, char* out, int outLen) FL_NO_EXCEPT {
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

bool flush(u32 timeoutMs) FL_NO_EXCEPT {
    (void)timeoutMs;
    if (!Serial) return true;
    Serial.flush();
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NO_EXCEPT {
    if (!Serial) return 0;
    return Serial.write(buffer, size);
}

bool serial_ready() FL_NO_EXCEPT {
    return Serial ? true : false;
}

bool serial_is_buffered() FL_NO_EXCEPT {
    return true;  // LPC Arduino uses buffered UART
}

#else  // bare-metal: no Serial available yet

void begin(u32 baudRate) FL_NO_EXCEPT {
    (void)baudRate;
}

void print(const char* str) FL_NO_EXCEPT {
    (void)str;
}

void println(const char* str) FL_NO_EXCEPT {
    (void)str;
}

int available() FL_NO_EXCEPT {
    return 0;
}

int peek() FL_NO_EXCEPT {
    return -1;
}

int read() FL_NO_EXCEPT {
    return -1;
}

int readLineNative(char delimiter, char* out, int outLen) FL_NO_EXCEPT {
    (void)delimiter;
    (void)out;
    (void)outLen;
    return -1;
}

bool flush(u32 timeoutMs) FL_NO_EXCEPT {
    (void)timeoutMs;
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NO_EXCEPT {
    (void)buffer;
    (void)size;
    return 0;
}

bool serial_ready() FL_NO_EXCEPT {
    return false;
}

bool serial_is_buffered() FL_NO_EXCEPT {
    return false;
}

#endif  // ARDUINO

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ARM_LPC

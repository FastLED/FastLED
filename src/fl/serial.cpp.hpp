// Arduino-compatible Serial API implementation

#pragma once

#include "serial.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/charconv.h"
#include "fl/stl/cstring.h"
#include "fl/stl/time.h"

namespace fl {

// ============================================================================
// SerialPort Implementation
// ============================================================================

void SerialPort::begin(u32 baudRate) {
    fl::serial_begin(baudRate);
}

void SerialPort::end() {
    // Most platforms don't need explicit end() call
    // Serial is always available for debugging
}

int SerialPort::available() {
    return fl::available();
}

int SerialPort::read() {
    return fl::read();
}

int SerialPort::peek() {
    return fl::peek();
}

size_t SerialPort::write(u8 byte) {
    char str[2] = {static_cast<char>(byte), '\0'};
    fl::print(str);
    return 1;
}

size_t SerialPort::write(const u8* buffer, size_t size) {
    return fl::write_bytes(buffer, size);
}

size_t SerialPort::print(const char* str) {
    if (!str) {
        return 0;
    }

    fl::print(str);
    return fl::strlen(str);
}

size_t SerialPort::print(int value) {
    char buffer[12];  // Enough for "-2147483648" + null
    fl::itoa(value, buffer, 10);  // Base 10
    return print(buffer);
}

size_t SerialPort::print(unsigned int value) {
    char buffer[11];  // Enough for "4294967295" + null
    fl::utoa32(value, buffer, 10);  // Base 10
    return print(buffer);
}

size_t SerialPort::print(long value) {
    char buffer[21];  // Enough for 64-bit values
    fl::itoa64(value, buffer, 10);  // Base 10
    return print(buffer);
}

size_t SerialPort::print(unsigned long value) {
    char buffer[21];  // Enough for 64-bit values
    fl::utoa64(value, buffer, 10);  // Base 10
    return print(buffer);
}

size_t SerialPort::println(const char* str) {
    if (!str) {
        return println();
    }

    fl::println(str);
    return fl::strlen(str) + 1;  // +1 for newline
}

size_t SerialPort::println() {
    fl::println("");
    return 1;  // Just the newline
}

size_t SerialPort::println(int value) {
    char buffer[12];
    fl::itoa(value, buffer, 10);
    return println(buffer);
}

size_t SerialPort::println(unsigned int value) {
    char buffer[11];
    fl::utoa32(value, buffer, 10);
    return println(buffer);
}

size_t SerialPort::println(long value) {
    char buffer[21];
    fl::itoa64(value, buffer, 10);
    return println(buffer);
}

size_t SerialPort::println(unsigned long value) {
    char buffer[21];
    fl::utoa64(value, buffer, 10);
    return println(buffer);
}

// Note: printf() is now implemented as an inline template in serial.h

bool SerialPort::flush(u32 timeoutMs) {
    return fl::flush(timeoutMs);
}

SerialPort::operator bool() const {
    return fl::serial_ready();
}

void SerialPort::setTimeout(u32 timeoutMs) {
    mTimeoutMs = timeoutMs;
}

fl::string SerialPort::readString() {
    fl::string result;
    u32 startTime = fl::millis();

    while (fl::millis() - startTime < mTimeoutMs) {
        if (available() > 0) {
            int c = read();
            if (c != -1) {
                result += static_cast<char>(c);
                startTime = fl::millis();  // Reset timeout on successful read
            }
        }
    }

    return result;
}

fl::string SerialPort::readStringUntil(char delimiter) {
    fl::string result;
    u32 startTime = fl::millis();

    while (fl::millis() - startTime < mTimeoutMs) {
        if (available() > 0) {
            int c = read();
            if (c == -1) {
                continue;  // No data
            }
            if (c == delimiter) {
                break;  // Found delimiter, stop reading
            }
            result += static_cast<char>(c);
            startTime = fl::millis();  // Reset timeout on successful read
        }
    }

    return result;
}

size_t SerialPort::readBytes(u8* buffer, size_t length) {
    if (!buffer || length == 0) {
        return 0;
    }

    size_t count = 0;
    u32 startTime = fl::millis();

    while (count < length && (fl::millis() - startTime < mTimeoutMs)) {
        if (available() > 0) {
            int c = read();
            if (c != -1) {
                buffer[count++] = static_cast<u8>(c);
                startTime = fl::millis();  // Reset timeout on successful read
            }
        }
    }

    return count;
}

size_t SerialPort::readBytesUntil(char delimiter, u8* buffer, size_t length) {
    if (!buffer || length == 0) {
        return 0;
    }

    size_t count = 0;
    u32 startTime = fl::millis();

    while (count < length && (fl::millis() - startTime < mTimeoutMs)) {
        if (available() > 0) {
            int c = read();
            if (c == -1) {
                continue;  // No data
            }
            if (c == delimiter) {
                break;  // Found delimiter, stop reading
            }
            buffer[count++] = static_cast<u8>(c);
            startTime = fl::millis();  // Reset timeout on successful read
        }
    }

    return count;
}

long SerialPort::parseInt() {
    bool negative = false;
    long value = 0;
    bool foundDigit = false;
    u32 startTime = fl::millis();

    // Skip non-numeric characters
    while (fl::millis() - startTime < mTimeoutMs) {
        if (available() > 0) {
            int c = peek();
            if (c == -1) {
                continue;
            }

            // Check for sign
            if (c == '-') {
                negative = true;
                read();  // Consume the sign
                break;
            } else if (c == '+') {
                read();  // Consume the sign
                break;
            } else if (c >= '0' && c <= '9') {
                break;  // Found a digit
            } else {
                read();  // Skip non-numeric character
                startTime = fl::millis();  // Reset timeout
            }
        }
    }

    // Parse digits
    startTime = fl::millis();
    while (fl::millis() - startTime < mTimeoutMs) {
        if (available() > 0) {
            int c = peek();
            if (c == -1) {
                continue;
            }

            if (c >= '0' && c <= '9') {
                value = value * 10 + (c - '0');
                read();  // Consume the digit
                foundDigit = true;
                startTime = fl::millis();  // Reset timeout
            } else {
                break;  // Non-digit, stop parsing
            }
        }
    }

    return foundDigit ? (negative ? -value : value) : 0;
}

float SerialPort::parseFloat() {
    bool negative = false;
    long intPart = 0;
    long fracPart = 0;
    int fracDigits = 0;
    bool foundDigit = false;
    bool inFraction = false;
    u32 startTime = fl::millis();

    // Skip non-numeric characters
    while (fl::millis() - startTime < mTimeoutMs) {
        if (available() > 0) {
            int c = peek();
            if (c == -1) {
                continue;
            }

            // Check for sign
            if (c == '-') {
                negative = true;
                read();  // Consume the sign
                break;
            } else if (c == '+') {
                read();  // Consume the sign
                break;
            } else if (c >= '0' && c <= '9') {
                break;  // Found a digit
            } else if (c == '.') {
                break;  // Found decimal point
            } else {
                read();  // Skip non-numeric character
                startTime = fl::millis();  // Reset timeout
            }
        }
    }

    // Parse digits (integer and fractional parts)
    startTime = fl::millis();
    while (fl::millis() - startTime < mTimeoutMs) {
        if (available() > 0) {
            int c = peek();
            if (c == -1) {
                continue;
            }

            if (c == '.' && !inFraction) {
                inFraction = true;
                read();  // Consume decimal point
                foundDigit = true;  // Decimal point counts as finding a number
                startTime = fl::millis();  // Reset timeout
            } else if (c >= '0' && c <= '9') {
                if (inFraction) {
                    fracPart = fracPart * 10 + (c - '0');
                    fracDigits++;
                } else {
                    intPart = intPart * 10 + (c - '0');
                }
                read();  // Consume the digit
                foundDigit = true;
                startTime = fl::millis();  // Reset timeout
            } else {
                break;  // Non-digit, stop parsing
            }
        }
    }

    if (!foundDigit) {
        return 0.0f;
    }

    // Calculate float value
    float value = static_cast<float>(intPart);
    if (fracDigits > 0) {
        float divisor = 1.0f;
        for (int i = 0; i < fracDigits; i++) {
            divisor *= 10.0f;
        }
        value += static_cast<float>(fracPart) / divisor;
    }

    return negative ? -value : value;
}

// ============================================================================
// Global Serial object
// ============================================================================

// Note: FL_MAYBE_UNUSED on extern declaration (in header) is enough
// Don't use it on definition or linker won't export the symbol
SerialPort fl_serial;

} // namespace fl

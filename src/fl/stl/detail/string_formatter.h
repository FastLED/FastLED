#pragma once

#include "fl/int.h"
#include "fl/stl/stdint.h"  // For int32_t, uint32_t, uint64_t

namespace fl {

// Forward declaration
template <size N> class StrN;

// Forward declarations for charconv functions (avoid circular dependency with charconv.h)
// Note: Default arguments must only be specified in charconv.h, not here
int itoa(int32_t value, char *buffer, int radix);
int utoa32(uint32_t value, char *buffer, int radix);
int utoa64(uint64_t value, char *buffer, int radix);
void ftoa(float value, char *buffer, int precision);

// StringFormatter: Static utility class for formatting values to strings
// All methods are templates that work with any StrN<SIZE>
class StringFormatter {
  public:
    // Generic template methods that work with any StrN<SIZE>
    // These are the primary implementations that all code should use

    // Decimal formatting (base 10)
    template<size SIZE>
    static void append(i32 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void append(u32 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void append(int64_t val, StrN<SIZE> *dst);
    template<size SIZE>
    static void append(uint64_t val, StrN<SIZE> *dst);
    template<size SIZE>
    static void append(i16 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void append(u16 val, StrN<SIZE> *dst);

    // Hexadecimal formatting (base 16)
    template<size SIZE>
    static void appendHex(i32 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendHex(u32 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendHex(int64_t val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendHex(uint64_t val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendHex(i16 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendHex(u16 val, StrN<SIZE> *dst);

    // Octal formatting (base 8)
    template<size SIZE>
    static void appendOct(i32 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendOct(u32 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendOct(int64_t val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendOct(uint64_t val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendOct(i16 val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendOct(u16 val, StrN<SIZE> *dst);

    // Utility methods
    static bool isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }
    static float parseFloat(const char *str, size len);
    static int parseInt(const char *str, size len);
    static int parseInt(const char *str);
    static bool isDigit(char c) { return c >= '0' && c <= '9'; }

    template<size SIZE>
    static void appendFloat(const float &val, StrN<SIZE> *dst);
    template<size SIZE>
    static void appendFloat(const float &val, StrN<SIZE> *dst, int precision);
};

// Inline template implementations for StringFormatter
// These must be defined in the header to avoid template instantiation order issues

// Float formatting
template<size SIZE>
inline void StringFormatter::appendFloat(const float &val, StrN<SIZE> *dst) {
    char buf[64] = {0};
    ftoa(val, buf, 2);  // Default precision = 2
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendFloat(const float &val, StrN<SIZE> *dst, int precision) {
    char buf[64] = {0};
    ftoa(val, buf, precision);
    dst->write(buf, strlen(buf));
}

// Decimal (base 10) formatting
template<size SIZE>
inline void StringFormatter::append(i32 val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    itoa(val, buf, 10);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::append(u32 val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    utoa32(val, buf, 10);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::append(int64_t val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    // For signed 64-bit, handle negative numbers manually
    if (val < 0) {
        dst->write("-", 1);
        utoa64(static_cast<uint64_t>(-val), buf, 10);
    } else {
        utoa64(static_cast<uint64_t>(val), buf, 10);
    }
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::append(uint64_t val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    utoa64(val, buf, 10);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::append(i16 val, StrN<SIZE> *dst) {
    append(static_cast<i32>(val), dst);
}

template<size SIZE>
inline void StringFormatter::append(u16 val, StrN<SIZE> *dst) {
    append(static_cast<u32>(val), dst);
}

// Hexadecimal (base 16) formatting
template<size SIZE>
inline void StringFormatter::appendHex(i32 val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    itoa(val, buf, 16);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendHex(u32 val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    utoa32(val, buf, 16);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendHex(int64_t val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    // For hex, treat signed as bit pattern (cast to unsigned)
    utoa64(static_cast<uint64_t>(val), buf, 16);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendHex(uint64_t val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    utoa64(val, buf, 16);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendHex(i16 val, StrN<SIZE> *dst) {
    appendHex(static_cast<i32>(val), dst);
}

template<size SIZE>
inline void StringFormatter::appendHex(u16 val, StrN<SIZE> *dst) {
    appendHex(static_cast<u32>(val), dst);
}

// Octal (base 8) formatting
template<size SIZE>
inline void StringFormatter::appendOct(i32 val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    itoa(val, buf, 8);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendOct(u32 val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    utoa32(val, buf, 8);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendOct(int64_t val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    // For octal, treat signed as bit pattern (cast to unsigned)
    utoa64(static_cast<uint64_t>(val), buf, 8);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendOct(uint64_t val, StrN<SIZE> *dst) {
    char buf[63] = {0};
    utoa64(val, buf, 8);
    dst->write(buf, strlen(buf));
}

template<size SIZE>
inline void StringFormatter::appendOct(i16 val, StrN<SIZE> *dst) {
    appendOct(static_cast<i32>(val), dst);
}

template<size SIZE>
inline void StringFormatter::appendOct(u16 val, StrN<SIZE> *dst) {
    appendOct(static_cast<u32>(val), dst);
}

} // namespace fl

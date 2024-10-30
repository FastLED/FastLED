// Arduino.h emulation for the WebAssembly platform.
// This allows us to compile sketches as is for the WebAssembly platform.

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <random>
#include <algorithm>
#include <string>
#include "str.h"

#include "namespace.h"
FASTLED_NAMESPACE_BEGIN
class Str;
FASTLED_NAMESPACE_END

FASTLED_USING_NAMESPACE

using std::min;
using std::max;

inline long random(long min, long max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    // Arduino random is exclusive of the max value, but std::uniform_int_distribution is inclusive.
    // So we subtract 1 from the max value.
    std::uniform_int_distribution<> dis(min, max-1);
    return dis(gen);
}

inline long random(long max) {
    return random(0, max);
}

template<typename T>
struct PrintHelper {};

#define DEFINE_PRINT_HELPER(type, format) \
    template<> \
    struct PrintHelper<type> { \
        static void print(type val) { printf(format, val); } \
        static void println(type val) { printf(format, val); printf("\n"); } \
    }

#define DEFINE_PRINT_HELPER_EXT(type, format, val_opp) \
    template<> \
    struct PrintHelper<type> { \
        static void print(type val) { printf(format, val_opp); } \
        static void println(type val) { printf(format, val_opp); printf("\n"); } \
    }

// gcc push options
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"

DEFINE_PRINT_HELPER(double, "%f");
DEFINE_PRINT_HELPER(float, "%f");
DEFINE_PRINT_HELPER(const char *, "%s");
DEFINE_PRINT_HELPER(uint64_t, "%lu");
DEFINE_PRINT_HELPER(uint32_t, "%u");
DEFINE_PRINT_HELPER(uint16_t, "%u");
DEFINE_PRINT_HELPER(uint8_t, "%u");
DEFINE_PRINT_HELPER(int64_t, "%ld");
DEFINE_PRINT_HELPER(int32_t, "%d");
DEFINE_PRINT_HELPER(int16_t, "%d");
DEFINE_PRINT_HELPER(int8_t, "%d");
DEFINE_PRINT_HELPER_EXT(std::string, "%s", val.c_str());
DEFINE_PRINT_HELPER_EXT(Str, "%s", val.c_str());

// gcc pop options
#pragma GCC diagnostic pop

struct SerialEmulation {
    void begin(int) {}
    template<typename T>
    void print(T val) { PrintHelper<T>::print(val); }
    template<typename T>
    void println(T val) { PrintHelper<T>::println(val); }
    int available() { return 0; }
    int read() { return 0; }
    void write(uint8_t) {}
    void write(const char *s) { printf("%s", s); }
    void write(const uint8_t *s, size_t n) { fwrite(s, 1, n, stdout); }
    void write(const char *s, size_t n) { fwrite(s, 1, n, stdout); }
    void flush() {}
    void end() {}
};

#define LED_BUILTIN 13
#define HIGH 1
#define LOW 0
void digitalWrite(int, int) {}
void analogWrite(int, int) {}
int digitalRead(int) { return LOW; }

void pinMode(int, int) {}

// avr flash memory macro is disabled.
#ifdef F
#undef F
#endif

#define F(x) x

// Found in the wild for scintillating example
#ifdef FL_PGM_READ_PTR_NEAR
#undef FL_PGM_READ_PTR_NEAR
#endif

#define FL_PGM_READ_PTR_NEAR(addr) (*(addr))
typedef unsigned char byte;


SerialEmulation Serial;
SerialEmulation Serial1;
SerialEmulation Serial2;
SerialEmulation Serial3;
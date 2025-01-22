// Arduino.h emulation for the WebAssembly platform.
// This allows us to compile sketches as is for the WebAssembly platform.

#pragma once

#include "fl/str.h"
#include <algorithm>
#include <random>
#include <stdint.h>
#include <stdio.h>
#include <string>

#include "fl/namespace.h"

FASTLED_USING_NAMESPACE

using std::max;
using std::min;


namespace fl {
class Str;
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    const long run = in_max - in_min;
    if (run == 0) {
        return 0; // AVR returns -1, SAM returns 0
    }
    const long rise = out_max - out_min;
    const long delta = x - in_min;
    return (delta * rise) / run + out_min;
}

// constrain
template <typename T> T constrain(T x, T a, T b) {
    return x < a ? a : (x > b ? b : x);
}
} // namespace fl


using fl::map;
using fl::constrain;


inline long random(long min, long max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    // Arduino random is exclusive of the max value, but
    // std::uniform_int_distribution is inclusive. So we subtract 1 from the max
    // value.
    std::uniform_int_distribution<> dis(min, max - 1);
    return dis(gen);
}


inline int analogRead(int) { return random(0, 1023); }

inline long random(long max) { return random(0, max); }

template <typename T> struct PrintHelper {};

#define DEFINE_PRINT_HELPER(type, format)                                      \
    template <> struct PrintHelper<type> {                                     \
        static void print(type val) { printf(format, val); }                   \
        static void println(type val) {                                        \
            printf(format, val);                                               \
            printf("\n");                                                      \
        }                                                                      \
    }

#define DEFINE_PRINT_HELPER_EXT(type, format, val_opp)                         \
    template <> struct PrintHelper<type> {                                     \
        static void print(type val) { printf(format, val_opp); }               \
        static void println(type val) {                                        \
            printf(format, val_opp);                                           \
            printf("\n");                                                      \
        }                                                                      \
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
DEFINE_PRINT_HELPER(bool, "%d");
DEFINE_PRINT_HELPER_EXT(std::string, "%s", val.c_str());
DEFINE_PRINT_HELPER_EXT(fl::Str, "%s", val.c_str());

#ifdef __EMSCRIPTEN__
DEFINE_PRINT_HELPER(unsigned long, "%lu");  // Not sure why this is needed in emscripten
#endif

#define A0 0
#define A1 1
#define A2 2
#define A3 3
#define A4 4
#define A5 5


// gcc pop options
#pragma GCC diagnostic pop

struct SerialEmulation {
    void begin(int) {}
    template <typename T> void print(T val) { PrintHelper<T>::print(val); }
    template <typename T> void println(T val) { PrintHelper<T>::println(val); }
    void println() { printf("\n"); }
    int available() { return 0; }
    int read() { return 0; }
    void write(uint8_t) {}
    void write(const char *s) { printf("%s", s); }
    void write(const uint8_t *s, size_t n) { fwrite(s, 1, n, stdout); }
    void write(const char *s, size_t n) { fwrite(s, 1, n, stdout); }
    void flush() {}
    void end() {}
    uint8_t peek() { return 0; }
};

#define LED_BUILTIN 13
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2


inline void digitalWrite(int, int) {}
inline void analogWrite(int, int) {}
inline int digitalRead(int) { return LOW; }
inline void pinMode(int, int) {}

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

extern SerialEmulation Serial;
extern SerialEmulation Serial1;
extern SerialEmulation Serial2;
extern SerialEmulation Serial3;
typedef SerialEmulation HardwareSerial;
typedef SerialEmulation SoftwareSerial;
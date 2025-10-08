// Arduino.h emulation for the WebAssembly platform.
// This allows us to compile sketches as is for the WebAssembly platform.

#pragma once

// Standard Arduino define - indicates Arduino environment and version
// Using a modern Arduino IDE version number (1.8.19 = 10819)
#ifndef ARDUINO
#define ARDUINO 1
#endif

// FastLED headers only - NO stdlib headers
#include "fl/str.h"
#include "fl/stdint.h"
#include "fl/ostream.h"
#include "fl/namespace.h"
#include "time_stub.h"
#include "fl/math_macros.h"
#include "fl/math.h"

FASTLED_USING_NAMESPACE

// Custom min/max to avoid <algorithm> include
template <typename T>
T min(T a, T b) {
    return a < b ? a : b;
}
template <typename T>
T max(T a, T b) {
    return a > b ? a : b;
}




namespace fl {

long map(long x, long in_min, long in_max, long out_min, long out_max);

// constrain
template <typename T> T constrain(T x, T a, T b) {
    return x < a ? a : (x > b ? b : x);
}
} // namespace fl

using fl::constrain;
using fl::map;

long random(long min, long max);
long random(long max);
long random();
int analogRead(int);



#ifndef A0
#define A0 0
#endif

#ifndef A1
#define A1 1
#endif

#ifndef A2
#define A2 2
#endif

#ifndef A3
#define A3 3
#endif

#ifndef A4
#define A4 4
#endif

#ifndef A5
#define A5 5
#endif

#ifndef A6
#define A6 6
#endif

#ifndef A7
#define A7 7
#endif

#ifndef A8
#define A8 8
#endif

#ifndef A9
#define A9 9
#endif

#ifndef A10
#define A10 10
#endif

#ifndef A11
#define A11 11
#endif



struct SerialEmulation {
    void begin(int);

    // Template methods must stay in header
    template <typename T> void print(T val) {
        fl::cout << val;
    }
    template <typename T> void println(T val) {
        fl::cout << val << fl::endl;
    }

    // Two-argument print overloads for formatting - moved to .cpp
    void print(float val, int digits);
    void print(double val, int digits);
    void print(int val, int base);
    void print(unsigned int val, int base);

    void println();
    int available();
    int read();
    void write(uint8_t);
    void write(const char *s);
    void write(const uint8_t *s, size_t n);
    void write(const char *s, size_t n);
    void flush();
    void end();
    uint8_t peek();
};

#define LED_BUILTIN 13
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

void digitalWrite(int, int);
void analogWrite(int, int);
int digitalRead(int);
void pinMode(int, int);



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

// Define Serial instances for stub compilation
extern SerialEmulation Serial;
extern SerialEmulation Serial1;
extern SerialEmulation Serial2;
extern SerialEmulation Serial3;

typedef SerialEmulation HardwareSerial;
typedef SerialEmulation SoftwareSerial;

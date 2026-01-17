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
#include "fl/stddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/ostream.h"
#include "fl/stl/stdio.h"
#include "time_stub.h"
#include "fl/math_macros.h"
#include "fl/stl/math.h"

// Math functions from fl:: namespace
// On stub platform, standard library <cmath> provides math functions in global namespace
// through included headers like <mutex>. No need to re-export fl:: math functions here.
// Arduino sketches can use either the stdlib versions or explicitly use fl:: prefix.

#ifndef FASTLED_NO_ARDUINO_STUBS
// Custom min/max to avoid <algorithm> include
// Excluded when FASTLED_NO_ARDUINO_STUBS is defined (for compatibility with ArduinoFake, etc.)
template <typename T>
T min(T a, T b) {
    return a < b ? a : b;
}
template <typename T>
T max(T a, T b) {
    return a > b ? a : b;
}
#endif // FASTLED_NO_ARDUINO_STUBS




namespace fl {

long map(long x, long in_min, long in_max, long out_min, long out_max);

// constrain
template <typename T> T constrain(T x, T a, T b) {
    return x < a ? a : (x > b ? b : x);
}
} // namespace fl

#ifndef FASTLED_NO_ARDUINO_STUBS
// Arduino stub functions - excluded when FASTLED_NO_ARDUINO_STUBS is defined
using fl::constrain;
using fl::map;

long random(long min, long max);
long random(long max);
int analogRead(int);
void init();  // Arduino hardware initialization (stub: does nothing)

// Test helper functions for analog value injection (stub platform only)
void setAnalogValue(int pin, int value);  // Set analog value for specific pin
int getAnalogValue(int pin);              // Get current analog value for pin
void clearAnalogValues();                 // Reset all analog values to default (random)
#endif // FASTLED_NO_ARDUINO_STUBS



#ifndef FASTLED_NO_ARDUINO_STUBS
// Analog pin definitions - excluded when FASTLED_NO_ARDUINO_STUBS is defined
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
#endif // FASTLED_NO_ARDUINO_STUBS



#ifndef FASTLED_NO_ARDUINO_STUBS
// Serial emulation and digital I/O - excluded when FASTLED_NO_ARDUINO_STUBS is defined
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

    // Printf-style formatted output (variadic template implementation)
    template<typename... Args>
    void printf(const char* format, Args... args) {
        fl::printf(format, args...);
    }

    int available();
    int read();
    fl::string readStringUntil(char terminator);
    void write(uint8_t);
    void write(const char *s);
    void write(const uint8_t *s, size_t n);
    void write(const char *s, size_t n);
    void flush();
    void end();
    uint8_t peek();

    // Support for Serial boolean checks (always returns true for stub platform)
    explicit operator bool() const { return true; }
    bool operator!() const { return false; }
};

#define LED_BUILTIN 13
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// Bit manipulation macros
#define bit(b) (1UL << (b))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))

void digitalWrite(int, int);
void analogWrite(int, int);
void analogReference(int);
int digitalRead(int);
void pinMode(int, int);
#endif // FASTLED_NO_ARDUINO_STUBS



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
typedef bool boolean;
typedef unsigned int word;

// Arduino String class compatibility - use fl::string as String
typedef fl::string String;

#ifndef FASTLED_NO_ARDUINO_STUBS
// Define Serial instances for stub compilation
extern SerialEmulation Serial;
extern SerialEmulation Serial1;
extern SerialEmulation Serial2;
extern SerialEmulation Serial3;

typedef SerialEmulation HardwareSerial;
typedef SerialEmulation SoftwareSerial;
#endif // FASTLED_NO_ARDUINO_STUBS

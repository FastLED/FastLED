// Arduino.h emulation for the WebAssembly platform.
// This allows us to compile sketches as is for the WebAssembly platform.
// ok no namespace fl

#pragma once

// IWYU pragma: private

// Standard Arduino define - indicates Arduino environment and version
// Using a modern Arduino IDE version number (1.8.19 = 10819)
#ifndef ARDUINO
#define ARDUINO 1
#endif

// FastLED headers only - NO stdlib headers
#include "fl/stl/string.h"
#include "fl/stl/stddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/ostream.h"
#include "fl/stl/stdio.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/singleton.h"
#include "platforms/stub/time_stub.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

// Math functions from fl:: namespace
// On stub platform, min/max/abs/radians/degrees are provided by
// FastLED.h (from fl/math/math.h) which undefs Arduino macros and brings fl:: functions
// into global namespace via using declarations.
// Note: fl::map refers to the map container (red-black tree), not the Arduino map function
// Note: fl/math/math.h is NOT included here - it's included by FastLED.h after macro undefs
// Note: fl/map_range.h provides fl::map_range() which the global map() function delegates to

#ifndef FASTLED_NO_ARDUINO_STUBS
// Arduino stub functions - excluded when FASTLED_NO_ARDUINO_STUBS is defined

// Arduino map() function - defined in global namespace (NOT in fl::)
// fl::map refers to the map container
// This delegates to fl::map_range() for consistent behavior and overflow protection
long map(long x, long in_min, long in_max, long out_min, long out_max) FL_NOEXCEPT;

long random(long min, long max) FL_NOEXCEPT;
long random(long max) FL_NOEXCEPT;
int analogRead(int) FL_NOEXCEPT;
void init() FL_NOEXCEPT;  // Arduino hardware initialization (stub: does nothing)

// Test helper functions for analog value injection (stub platform only)
void setAnalogValue(int pin, int value) FL_NOEXCEPT;  // Set analog value for specific pin
int getAnalogValue(int pin) FL_NOEXCEPT;              // Get current analog value for pin
void clearAnalogValues() FL_NOEXCEPT;                 // Reset all analog values to default (random)
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
    void begin(int) FL_NOEXCEPT;

    // Single-argument print/println
    template <typename T> void print(T val) FL_NOEXCEPT {
        fl::cout << val;
    }
    template <typename T> void println(T val) FL_NOEXCEPT {
        fl::cout << val << fl::endl;
    }

    // Two-argument floating point: print(float/double, digits)
    template <typename T>
    typename fl::enable_if<fl::is_floating_point<T>::value>::type
    print(T val, int digits) FL_NOEXCEPT {
        digits = digits < 0 ? 0 : (digits > 9 ? 9 : digits);
        double d = static_cast<double>(val);
        switch(digits) {
            case 0: fl::printf("%.0f", d); break;
            case 1: fl::printf("%.1f", d); break;
            case 2: fl::printf("%.2f", d); break;
            case 3: fl::printf("%.3f", d); break;
            case 4: fl::printf("%.4f", d); break;
            case 5: fl::printf("%.5f", d); break;
            case 6: fl::printf("%.6f", d); break;
            case 7: fl::printf("%.7f", d); break;
            case 8: fl::printf("%.8f", d); break;
            case 9: fl::printf("%.9f", d); break;
        }
    }

    template <typename T>
    typename fl::enable_if<fl::is_floating_point<T>::value>::type
    println(T val, int digits) FL_NOEXCEPT {
        print(val, digits);
        fl::printf("\n");
    }

    // Two-argument integer: print(integer, base)
    // Covers all signed/unsigned integer types including char/u8
    // (char/u8 are printed as numeric values, not characters)
    template <typename T>
    typename fl::enable_if<fl::is_integral<T>::value &&
                           !fl::is_same<typename fl::remove_cv<T>::type, bool>::value>::type
    print(T val, int base) FL_NOEXCEPT {
        if (fl::is_signed<T>::value) {
            long long v = static_cast<long long>(val);
            if (base == 16) fl::printf("%llx", v);
            else if (base == 8) fl::printf("%llo", v);
            else if (base == 2) {
                int bits = static_cast<int>(sizeof(T) * 8);
                for (int i = bits - 1; i >= 0; i--)
                    fl::printf("%d", (int)((v >> i) & 1));
            }
            else fl::printf("%lld", v);
        } else {
            unsigned long long v = static_cast<unsigned long long>(val);
            if (base == 16) fl::printf("%llx", v);
            else if (base == 8) fl::printf("%llo", v);
            else if (base == 2) {
                int bits = static_cast<int>(sizeof(T) * 8);
                for (int i = bits - 1; i >= 0; i--)
                    fl::printf("%d", (int)((v >> i) & 1));
            }
            else fl::printf("%llu", v);
        }
    }

    template <typename T>
    typename fl::enable_if<fl::is_integral<T>::value &&
                           !fl::is_same<typename fl::remove_cv<T>::type, bool>::value>::type
    println(T val, int base) FL_NOEXCEPT {
        print(val, base);
        fl::printf("\n");
    }

    void println() FL_NOEXCEPT;

    // Printf-style formatted output (variadic template implementation)
    template<typename... Args>
    void printf(const char* format, Args... args) FL_NOEXCEPT {  // ok snprintf — method routes through fl::printf
        fl::printf(format, args...);
    }

    int available() FL_NOEXCEPT;
    int read() FL_NOEXCEPT;
    fl::string readStringUntil(char terminator) FL_NOEXCEPT;
    void write(fl::u8) FL_NOEXCEPT;
    void write(const char *s) FL_NOEXCEPT;
    void write(const fl::u8 *s, size_t n) FL_NOEXCEPT;
    void write(const char *s, size_t n) FL_NOEXCEPT;
    void flush() FL_NOEXCEPT;
    void end() FL_NOEXCEPT;
    fl::u8 peek() FL_NOEXCEPT;

    // Support for Serial boolean checks (always returns true for stub platform)
    explicit operator bool() const FL_NOEXCEPT { return true; }
    bool operator!() const FL_NOEXCEPT { return false; }
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
#define lowByte(w) ((fl::u8) ((w) & 0xff))
#define highByte(w) ((fl::u8) ((w) >> 8))

// Arduino math macro
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

void digitalWrite(int, int) FL_NOEXCEPT;
void analogWrite(int, int) FL_NOEXCEPT;
void analogReference(int) FL_NOEXCEPT;
int digitalRead(int) FL_NOEXCEPT;
void pinMode(int, int) FL_NOEXCEPT;
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

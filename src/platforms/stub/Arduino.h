// Arduino.h emulation for the WebAssembly platform.
// This allows us to compile sketches as is for the WebAssembly platform.

#pragma once

// Standard Arduino define - indicates Arduino environment and version
// Using a modern Arduino IDE version number (1.8.19 = 10819)
#ifndef ARDUINO
#define ARDUINO 1
#endif

#include "fl/str.h"
#include <algorithm>
#include <random>
#include "fl/stdint.h"
#include <stdio.h>
#include <string>
#include "fl/ostream.h"

#include "fl/namespace.h"
// Arduino timing functions - provided by time_stub.h
#include "time_stub.h"
#include "fl/math_macros.h"
#include "fl/math.h"

FASTLED_USING_NAMESPACE

template <typename T>
T min(T a, T b) {
    return a < b ? a : b;
}
template <typename T>
T max(T a, T b) {
    return a > b ? a : b;
}




namespace fl {

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

using fl::constrain;
using fl::map;

inline long random(long min, long max) {
    if (min == max) {
        return min;
    }
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

// Arduino-compatible random() with no parameters - returns full range random long
inline long random() { 
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<long> dis;
    return dis(gen);
}



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
    void begin(int) {}
    template <typename T> void print(T val) {
        fl::cout << val;
    }
    template <typename T> void println(T val) {
        fl::cout << val << fl::endl;
    }
    
    // Two-argument print overloads for formatting
    void print(float _val, int digits) { 
        // Clamp digits to reasonable range
        digits = digits < 0 ? 0 : (digits > 9 ? 9 : digits);
        double val = static_cast<double>(_val);
        
        // Use literal format strings to avoid linter warnings
        switch(digits) {
            case 0: printf("%.0f", val); break;
            case 1: printf("%.1f", val); break;
            case 2: printf("%.2f", val); break;
            case 3: printf("%.3f", val); break;
            case 4: printf("%.4f", val); break;
            case 5: printf("%.5f", val); break;
            case 6: printf("%.6f", val); break;
            case 7: printf("%.7f", val); break;
            case 8: printf("%.8f", val); break;
            case 9: printf("%.9f", val); break;
        }
    }
    void print(double val, int digits) { 
        // Clamp digits to reasonable range
        digits = digits < 0 ? 0 : (digits > 9 ? 9 : digits);
        
        // Use literal format strings to avoid linter warnings
        switch(digits) {
            case 0: printf("%.0f", val); break;
            case 1: printf("%.1f", val); break;
            case 2: printf("%.2f", val); break;
            case 3: printf("%.3f", val); break;
            case 4: printf("%.4f", val); break;
            case 5: printf("%.5f", val); break;
            case 6: printf("%.6f", val); break;
            case 7: printf("%.7f", val); break;
            case 8: printf("%.8f", val); break;
            case 9: printf("%.9f", val); break;
        }
    }
    void print(int val, int base) {
        if (base == 16) printf("%x", val);
        else if (base == 8) printf("%o", val);
        else if (base == 2) {
            // Binary output
            for (int i = 31; i >= 0; i--) {
                printf("%d", (val >> i) & 1);
            }
        }
        else printf("%d", val);
    }
    void print(unsigned int val, int base) {
        if (base == 16) printf("%x", val);
        else if (base == 8) printf("%o", val);
        else if (base == 2) {
            // Binary output
            for (int i = 31; i >= 0; i--) {
                printf("%d", (val >> i) & 1);
            }
        }
        else printf("%u", val);
    }
    
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

// Define Serial instances for stub compilation
extern SerialEmulation Serial;
extern SerialEmulation Serial1;
extern SerialEmulation Serial2;
extern SerialEmulation Serial3;

typedef SerialEmulation HardwareSerial;
typedef SerialEmulation SoftwareSerial;

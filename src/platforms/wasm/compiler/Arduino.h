// Arduino.h emulation for the WebAssembly platform.
// This allows us to compile sketches as is for the WebAssembly platform.

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <random>
#include <algorithm>

using std::min;
using std::max;

inline long random(long min, long max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

inline long random(long max) {
    return random(0, max);
}

struct SerialEmulation {
    void print(const char *s) { printf("%s", s); }
    void println(const char *s) { printf("%s\n", s); }
    void print(uint8_t n) { printf("%d", n); }
    void println(uint8_t n) { printf("%d\n", n); }
    void print(int n) { printf("%d", n); }
    void println(int n) { printf("%d\n", n); }
    void print(uint16_t n) { printf("%d", n); }
    void println(uint16_t n) { printf("%d\n", n); }
    void begin(int32_t) {}
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
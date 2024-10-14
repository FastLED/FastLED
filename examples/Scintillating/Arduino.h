

#pragma once

#include <stdint.h>
#include <stdio.h>

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
int digitalRead(int) { return LOW; }



SerialEmulation Serial;
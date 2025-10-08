
#if defined(FASTLED_USE_STUB_ARDUINO) || defined(__EMSCRIPTEN__)
// STUB platform implementation - excluded for WASM builds which provide their own Arduino.cpp

#include "./Arduino.h"  // ok include

// Stdlib headers - kept out of Arduino.h for faster compilation
#include <random>

#include "fl/printf.h"

// fl namespace functions
namespace fl {

long map(long x, long in_min, long in_max, long out_min, long out_max) {
    const long run = in_max - in_min;
    if (run == 0) {
        return 0; // AVR returns -1, SAM returns 0
    }
    const long rise = out_max - out_min;
    const long delta = x - in_min;
    return (delta * rise) / run + out_min;
}

} // namespace fl

// Random number generation functions
long random(long min, long max) {
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

long random(long max) {
    return random(0, max);
}

long random() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<long> dis;
    return dis(gen);
}

int analogRead(int) {
    return random(0, 1023);
}

// Digital I/O functions
void digitalWrite(int, int) {}
void analogWrite(int, int) {}
int digitalRead(int) { return LOW; }
void pinMode(int, int) {}

// SerialEmulation member functions
void SerialEmulation::begin(int) {}

void SerialEmulation::print(float _val, int digits) {
    // Clamp digits to reasonable range
    digits = digits < 0 ? 0 : (digits > 9 ? 9 : digits);
    double val = static_cast<double>(_val);

    // Use literal format strings to avoid linter warnings
    switch(digits) {
        case 0: fl::printf("%.0f", val); break;
        case 1: fl::printf("%.1f", val); break;
        case 2: fl::printf("%.2f", val); break;
        case 3: fl::printf("%.3f", val); break;
        case 4: fl::printf("%.4f", val); break;
        case 5: fl::printf("%.5f", val); break;
        case 6: fl::printf("%.6f", val); break;
        case 7: fl::printf("%.7f", val); break;
        case 8: fl::printf("%.8f", val); break;
        case 9: fl::printf("%.9f", val); break;
    }
}

void SerialEmulation::print(double val, int digits) {
    // Clamp digits to reasonable range
    digits = digits < 0 ? 0 : (digits > 9 ? 9 : digits);

    // Use literal format strings to avoid linter warnings
    switch(digits) {
        case 0: fl::printf("%.0f", val); break;
        case 1: fl::printf("%.1f", val); break;
        case 2: fl::printf("%.2f", val); break;
        case 3: fl::printf("%.3f", val); break;
        case 4: fl::printf("%.4f", val); break;
        case 5: fl::printf("%.5f", val); break;
        case 6: fl::printf("%.6f", val); break;
        case 7: fl::printf("%.7f", val); break;
        case 8: fl::printf("%.8f", val); break;
        case 9: fl::printf("%.9f", val); break;
    }
}

void SerialEmulation::print(int val, int base) {
    if (base == 16) fl::printf("%x", val);
    else if (base == 8) fl::printf("%o", val);
    else if (base == 2) {
        // Binary output
        for (int i = 31; i >= 0; i--) {
            fl::printf("%d", (val >> i) & 1);
        }
    }
    else fl::printf("%d", val);
}

void SerialEmulation::print(unsigned int val, int base) {
    if (base == 16) fl::printf("%x", val);
    else if (base == 8) fl::printf("%o", val);
    else if (base == 2) {
        // Binary output
        for (int i = 31; i >= 0; i--) {
            fl::printf("%d", (val >> i) & 1);
        }
    }
    else fl::printf("%u", val);
}

void SerialEmulation::println() {
    fl::printf("\n");
}

int SerialEmulation::available() {
    return 0;
}

int SerialEmulation::read() {
    return 0;
}

void SerialEmulation::write(uint8_t) {}

void SerialEmulation::write(const char *s) {
    fl::printf("%s", s);
}

void SerialEmulation::write(const uint8_t *s, size_t n) {
    fwrite(s, 1, n, stdout);
}

void SerialEmulation::write(const char *s, size_t n) {
    fwrite(s, 1, n, stdout);
}

void SerialEmulation::flush() {}

void SerialEmulation::end() {}

uint8_t SerialEmulation::peek() {
    return 0;
}

// Serial instances
SerialEmulation Serial;
SerialEmulation Serial1;
SerialEmulation Serial2;
SerialEmulation Serial3;

#endif

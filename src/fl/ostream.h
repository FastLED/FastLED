#pragma once

// Forward declaration to avoid pulling in fl/io.h and causing fl/io.cpp to be compiled
namespace fl {
    void print(const char* str);
}

#include "fl/str.h"
#include "crgb.h"

#ifndef FASTLED_STRSTREAM_USES_SIZE_T
#if defined(__AVR__) || defined(ESP8266) || defined(ESP32)
#define FASTLED_STRSTREAM_USES_SIZE_T 0
#else
#define FASTLED_STRSTREAM_USES_SIZE_T 1
#endif
#endif

namespace fl {

class ostream {
public:
    ostream() = default;

    // Stream output operators that immediately print
    ostream& operator<<(const char* str) {
        if (str) {
            print(str);
        }
        return *this;
    }

    ostream& operator<<(const string& str) {
        print(str.c_str());
        return *this;
    }

    ostream& operator<<(char c) {
        char str[2] = {c, '\0'};
        print(str);
        return *this;
    }

    ostream& operator<<(int8_t n) {
        string temp;
        temp.append(int16_t(n));
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(uint8_t n) {
        string temp;
        temp.append(uint16_t(n));
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(int16_t n) {
        string temp;
        temp.append(n);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(uint16_t n) {
        string temp;
        temp.append(n);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(int32_t n) {
        string temp;
        temp.append(n);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(uint32_t n) {
        string temp;
        temp.append(n);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(float f) {
        string temp;
        temp.append(f);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(double d) {
        string temp;
        temp.append(d);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(const CRGB& rgb) {
        string temp;
        temp.append(rgb);
        print(temp.c_str());
        return *this;
    }

#if FASTLED_STRSTREAM_USES_SIZE_T
    ostream& operator<<(size_t n) {
        string temp;
        temp.append(uint32_t(n));
        print(temp.c_str());
        return *this;
    }
#endif

    // Generic template for other types that have string append support
    template<typename T>
    ostream& operator<<(const T& value) {
        string temp;
        temp.append(value);
        print(temp.c_str());
        return *this;
    }
};

// Global cout instance for immediate output
extern ostream cout;

// Line ending manipulator
struct endl_t {};
extern const endl_t endl;

// endl manipulator implementation
inline ostream& operator<<(ostream& os, const endl_t&) {
    os << "\n";
    return os;
}

} // namespace fl

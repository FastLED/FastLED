#pragma once

#include "crgb.h"
#include "str.h"


#ifndef FASTLED_STRSTREAM_USES_SIZE_T
#if defined(__AVR__) || defined(ESP8266) || defined(ESP32)
#define FASTLED_STRSTREAM_USES_SIZE_T 0
#else
#define FASTLED_STRSTREAM_USES_SIZE_T 1
#endif
#endif

namespace fl {

template <typename T> struct StrStreamHelper {
    static void append(Str &str, const T& n) { str.append(n); }
};

template <> struct StrStreamHelper<int> {
    static void append(Str &str, const int& n) { str.append(int32_t(n)); }
};

template <> struct StrStreamHelper<uint8_t> {
    static void append(Str &str, const uint8_t& n) { str.append(uint16_t(n)); }
};

template <> struct StrStreamHelper<char> {
    static void append(Str &str, const char& n) { str.append(uint16_t(n)); }
};

template <> struct StrStreamHelper<unsigned int> {
    static void append(Str &str, const unsigned int& n) { str.append(uint32_t(n)); }
};

class StrStream {
  public:
    StrStream() = default;
    StrStream(const Str &str) : mStr(str) {}

    void setTreatCharAsInt(bool treatCharAsInt) {
        mTreatCharAsInt = treatCharAsInt;
    }

    const Str &str() const { return mStr; }
    const char *c_str() const { return mStr.c_str(); }

    StrStream &operator<<(const CRGB &rgb) {
        mStr.append("CRGB(");
        mStr.append(rgb.r);
        mStr.append(",");
        mStr.append(rgb.g);
        mStr.append(",");
        mStr.append(rgb.b);
        mStr.append(")");
        return *this;
    }

    StrStream &operator=(uint16_t n) {
        mStr.clear();
        (*this) << n;
        return *this;
    }

    StrStream &operator=(uint8_t n) {
        mStr.clear();
        (*this) << n;
        return *this;
    }

    StrStream &operator=(char c) {
        mStr.clear();
        (*this) << c;
        return *this;
    }

    // << operator section
    StrStream &operator<<(const Str &str) {
        mStr.append(str);
        return *this;
    }

    StrStream &operator<<(const char *str) {
        mStr.append(str);
        return *this;
    }

    StrStream &operator<<(char c) {
        if (mTreatCharAsInt) {
            StrStreamHelper<int>::append(mStr, c);
        } else {
            StrStreamHelper<char>::append(mStr, c);
        }
        return *this;
    }

    #if FASTLED_STRSTREAM_USES_SIZE_T
    StrStream &operator<<(size_t n) {
        mStr.append(uint32_t(n));
        return *this;
    }
    #endif

    template<typename T>
    StrStream &operator<<(T n) {
        StrStreamHelper<T>::append(mStr, n);
        return *this;
    }

    StrStream &operator<<(uint8_t n) {
        if (mTreatCharAsInt) {
            mStr.append(uint16_t(n));
        } else {
            mStr.append(n);
        }
        return *this;
    }

    StrStream &operator<<(uint16_t n) {
        mStr.append(n);
        return *this;
    }

    StrStream &operator<<(int16_t n) {
        mStr.append(n);
        return *this;
    }

    StrStream &operator<<(uint32_t n) {
        mStr.append(uint32_t(n));
        return *this;
    }

    StrStream &operator<<(int32_t n) {
        mStr.append(n);
        return *this;
    }

    // assignment operator completely replaces the current string
    StrStream &operator=(const Str &str) {
        mStr = str;
        return *this;
    }

    StrStream &operator=(const char *str) {
        mStr = str;
        return *this;
    }

    // crgb
    StrStream &operator=(const CRGB &rgb) {
        mStr.clear();
        (*this) << rgb;
        return *this;
    }


  private:
    Str mStr;
    bool mTreatCharAsInt = true;
};

class FakeStrStream {
  public:
    template<typename T>
    FakeStrStream& operator<<(const T&) { return *this; }
    
    FakeStrStream& operator<<(const char*) { return *this; }
    
    template<typename T> 
    FakeStrStream& operator=(const T&) { return *this; }

    FakeStrStream& operator<<(const CRGB&) { return *this; }
    FakeStrStream& operator<<(const Str&) { return *this; }
    FakeStrStream& operator<<(char) { return *this; }
    
    #if FASTLED_STRSTREAM_USES_SIZE_T
    FakeStrStream& operator<<(size_t) { return *this; }
    #endif
    
    FakeStrStream& operator<<(uint8_t) { return *this; }
    FakeStrStream& operator<<(uint16_t) { return *this; }
    FakeStrStream& operator<<(int16_t) { return *this; }
    FakeStrStream& operator<<(uint32_t) { return *this; }
    FakeStrStream& operator<<(int32_t) { return *this; }
    
    FakeStrStream& operator=(const Str&) { return *this; }
    FakeStrStream& operator=(const CRGB&) { return *this; }
    FakeStrStream& operator=(uint16_t) { return *this; }
    FakeStrStream& operator=(uint8_t) { return *this; }
    FakeStrStream& operator=(char) { return *this; }

};

} // namespace fl

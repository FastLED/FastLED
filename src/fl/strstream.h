#pragma once

#include "str.h"
#include "crgb.h"

namespace fl {

class StrStream {
  public:
    StrStream() = default;
    StrStream(const Str &str) : mStr(str) {}

    void setTreatCharAsInt(bool treatCharAsInt) { mTreatCharAsInt = treatCharAsInt; }

    const Str &str() const { return mStr; }
    const char *c_str() const { return mStr.c_str(); }

    StrStream& operator<<(const CRGB& rgb) {
        mStr.append("CRGB(");
        mStr.append(rgb.r);
        mStr.append(",");
        mStr.append(rgb.g);
        mStr.append(",");
        mStr.append(rgb.b);
        mStr.append(")");
        return *this;
    }

    StrStream &operator<<(const Str &str) {
        mStr.append(str);
        return *this;
    }

    StrStream &operator<<(const char *str) {
        mStr.append(str);
        return *this;
    }

    StrStream &operator<<(int n) {
        mStr.append(n);
        return *this;
    }

    StrStream &operator<<(unsigned int n) {
        mStr.append(uint32_t(n));
        return *this;
    }

    StrStream &operator<<(uint16_t n) {
        mStr.append(n);
        return *this;
    }

    StrStream &operator<<(uint8_t n) {
        if (mTreatCharAsInt) {
            mStr.append(int(n));
        } else {
            mStr.append(n);
        }
        return *this;
    }

    StrStream &operator<<(char c) {
        if (mTreatCharAsInt) {
            mStr.append(int(c));
        } else {
            mStr.append(c);
        }
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

    StrStream &operator=(int n) {
        mStr.clear();
        (*this) << n;
        return *this;
    }

    StrStream &operator=(unsigned int n) {
        mStr.clear();
        (*this) << n;
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

    // crgb 
    StrStream &operator=(const CRGB &rgb) {
        mStr.clear();
        mStr.append("CRGB(");
        mStr.append(rgb.r);
        mStr.append(", ");
        mStr.append(rgb.g);
        mStr.append(", ");
        mStr.append(rgb.b);
        mStr.append(")");
        return *this;
    }

  private:
    Str mStr;
    bool mTreatCharAsInt = true;
};

} // namespace fl

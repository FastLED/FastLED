#pragma once

#include "fl/int.h"
#include "fl/str.h"
#include "crgb.h"
#include "str.h"

namespace fl {

class Tile2x2_u8;
template <typename T> struct vec2;  // Forward declaration from fl/geometry.h
template <typename T, typename Alloc> class HeapVector;  // Forward declaration from fl/vector.h
struct FFTBins;  // Forward declaration from fl/fft.h

template <typename T> struct StrStreamHelper {
    static void append(string &str, const T &n) { str.append(n); }
};

template <> struct StrStreamHelper<int> {
    static void append(string &str, const int &n) { str.append(fl::i32(n)); }
};

template <> struct StrStreamHelper<fl::u8> {
    static void append(string &str, const fl::u8 &n) { str.append(fl::u16(n)); }
};

template <> struct StrStreamHelper<char> {
    static void append(string &str, const char &n) { str.append(fl::u16(n)); }
};

template <> struct StrStreamHelper<unsigned int> {
    static void append(string &str, const unsigned int &n) {
        str.append(fl::u32(n));
    }
};

class StrStream {
  public:
    StrStream() = default;
    StrStream(const string &str) : mStr(str) {}

    void setTreatCharAsInt(bool treatCharAsInt) {
        mTreatCharAsInt = treatCharAsInt;
    }

    const string &str() const { return mStr; }
    const char *c_str() const { return mStr.c_str(); }

    StrStream &operator<<(const CRGB &rgb) {
        mStr.append(rgb);
        return *this;
    }
    StrStream &operator<<(const StrStream &strStream) {
        mStr.append(strStream.str());
        return *this;
    }

    StrStream &operator<<(const Tile2x2_u8 &subpixel);

    // FFTBins support - implemented in strstream.cpp.hpp
    StrStream &operator<<(const FFTBins &bins);

    // vec2<T> support - format as (x,y)
    template<typename T>
    StrStream &operator<<(const vec2<T> &v) {
        mStr.append("(");
        mStr.append(v.x);
        mStr.append(",");
        mStr.append(v.y);
        mStr.append(")");
        return *this;
    }

    // HeapVector<T, Alloc> support - format as [item1, item2, ...]
    template<typename T, typename Alloc>
    StrStream &operator<<(const HeapVector<T, Alloc> &vec) {
        mStr.append("[");
        for (fl::sz i = 0; i < vec.size(); ++i) {
            if (i > 0) {
                mStr.append(", ");
            }
            mStr.append(vec[i]);
        }
        mStr.append("]");
        return *this;
    }

    StrStream &operator=(const fl::u16 &n) {
        mStr.clear();
        (*this) << n;
        return *this;
    }

    StrStream &operator=(const fl::u8 &n) {
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
    StrStream &operator<<(const string &str) {
        mStr.append(str);
        return *this;
    }

    StrStream &operator<<(const char *str) {
        mStr.append(str);
        return *this;
    }

    StrStream &operator<<(const float &f) {
        // multiply by 100 and round to get 2 decimal places
        mStr.append(f);
        return *this;
    }

    StrStream &operator<<(const double &f) {
        // multiply by 100 and round to get 2 decimal places
        mStr.append(f);
        return *this;
    }

    StrStream &operator<<(const char &c) {
        if (mTreatCharAsInt) {
            StrStreamHelper<int>::append(mStr, c);
        } else {
            StrStreamHelper<char>::append(mStr, c);
        }
        return *this;
    }

    
    StrStream &operator<<(const fl::u8 &n) {
        if (mTreatCharAsInt) {
            mStr.append(fl::u16(n));
        } else {
            mStr.append(n);
        }
        return *this;
    }

    StrStream &operator<<(const fl::i16 &n) {
        mStr.append(n);
        return *this;
    }

    StrStream &operator<<(const fl::i32 &n) {
        mStr.append(n);
        return *this;
    }

    StrStream &operator<<(const fl::u32 &n) {
        mStr.append(n);
        return *this;
    }

    // Unified handler for fl:: namespace size-like unsigned integer types and int
    // This handles fl::sz, fl::u16 from the fl:: namespace, and int type
    template<typename T>
    typename fl::enable_if<
        fl::is_same<T, fl::sz>::value ||
        fl::is_same<T, fl::u16>::value ||
        fl::is_same<T, int>::value,
        StrStream&
    >::type operator<<(T n) {
        if (fl::is_same<T, int>::value) {
            mStr.append(fl::i32(n));
        } else {
            mStr.append(fl::u32(n));
        }
        return *this;
    }

    // assignment operator completely replaces the current string
    StrStream &operator=(const string &str) {
        mStr = str;
        return *this;
    }

    StrStream &operator=(const char *str) {
        mStr.clear();
        mStr.append(str);
        return *this;
    }

    // crgb
    StrStream &operator=(const CRGB &rgb) {
        mStr.clear();
        (*this) << rgb;
        return *this;
    }

    void clear() { mStr.clear(); }

  private:
    string mStr;
    bool mTreatCharAsInt = true;
};

class FakeStrStream {
  public:
    template <typename T> FakeStrStream &operator<<(const T &) { return *this; }

    FakeStrStream &operator<<(const char *) { return *this; }

    template <typename T> FakeStrStream &operator=(const T &) { return *this; }

    FakeStrStream &operator<<(const CRGB &) { return *this; }
    FakeStrStream &operator<<(const string &) { return *this; }
    FakeStrStream &operator<<(char) { return *this; }

    // Unified template for fl:: namespace types and int to avoid conflicts on AVR
    template<typename T>
    typename fl::enable_if<
        fl::is_same<T, fl::sz>::value ||
        fl::is_same<T, fl::u16>::value ||
        fl::is_same<T, int>::value,
        FakeStrStream&
    >::type operator<<(T) { return *this; }

    FakeStrStream &operator<<(fl::u8) { return *this; }
    FakeStrStream &operator<<(fl::i16) { return *this; }
    FakeStrStream &operator<<(fl::i32) { return *this; }
    FakeStrStream &operator<<(fl::u32) { return *this; }

    FakeStrStream &operator=(const string &) { return *this; }
    FakeStrStream &operator=(const CRGB &) { return *this; }
    FakeStrStream &operator=(fl::u16) { return *this; }
    FakeStrStream &operator=(fl::u8) { return *this; }
    FakeStrStream &operator=(char) { return *this; }
};

} // namespace fl

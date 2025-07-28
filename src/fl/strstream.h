#pragma once

#include "fl/int.h"
#include "fl/str.h"
#include "crgb.h"
#include "str.h"


namespace fl {

class Tile2x2_u8;
class Tile2x2_u8_wrap;  // Forward declaration to support operator<< overload
template <typename T> struct vec2;  // Forward declaration from fl/geometry.h
template <typename T, typename Alloc> class HeapVector;  // Forward declaration from fl/vector.h
struct FFTBins;  // Forward declaration from fl/fft.h
template <fl::u32 N> class BitsetFixed;
class bitset_dynamic;
template <fl::u32 N> class BitsetInlined;

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
    static void append(string &str, const char &n) { str.append(n); }  // Append as character, not number
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
    StrStream &operator<<(const Tile2x2_u8_wrap &tile);  // New overload for wrapped tiles

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
        for (fl::size i = 0; i < vec.size(); ++i) {
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

    template<fl::size N>
    StrStream &operator<<(const char (&str)[N]) {
        mStr.append(str);
        return *this;
    }

    template<fl::u32 N>
    StrStream &operator<<(const BitsetFixed<N> &bs) {
        // mStr.append(bs);
        bs.to_string(&mStr);
        return *this;
    }

    StrStream &operator<<(const bitset_dynamic &bs) {
        bs.to_string(&mStr);
        return *this;
    }

    template<fl::u32 N>
    StrStream &operator<<(const BitsetInlined<N> &bs) {
        bs.to_string(&mStr);
        return *this;
    }

    // bool support - output as "true"/"false" for readability
    StrStream &operator<<(bool b) {
        mStr.append(b ? "true" : "false");
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

    StrStream &operator<<(const fl::u64 &n) {
        mStr.append(n);
        return *this;
    }

    StrStream &operator<<(const long long &n) {
        mStr.append(static_cast<fl::i64>(n));
        return *this;
    }


    // Unified handler for fl:: namespace size-like unsigned integer types and int
    // This handles fl::size, fl::u16 from the fl:: namespace, and int type
    template<typename T>
    typename fl::enable_if<
        fl::is_same<T, fl::size>::value ||
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
    bool mTreatCharAsInt = false;  // Default to ASCII mode for readable text output
};

class FakeStrStream {
  public:
    template <typename T> FakeStrStream &operator<<(const T &) { return *this; }

    FakeStrStream &operator<<(const char *) { return *this; }

    template<fl::size N>
    FakeStrStream &operator<<(const char (&)[N]) { return *this; }

    template <typename T> FakeStrStream &operator=(const T &) { return *this; }

    FakeStrStream &operator<<(const CRGB &) { return *this; }
    FakeStrStream &operator<<(const string &) { return *this; }
    FakeStrStream &operator<<(char) { return *this; }

    // bool support to match StrStream interface
    FakeStrStream &operator<<(bool) { return *this; }

    // Unified template for fl:: namespace types and int to avoid conflicts on AVR
    template<typename T>
    typename fl::enable_if<
        fl::is_same<T, fl::size>::value ||
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

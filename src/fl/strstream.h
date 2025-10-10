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

//-----------------------------------------------------------------------------
// Helper to determine target type for integer conversion based on size/signedness
//-----------------------------------------------------------------------------
namespace strstream_detail {
    // Determines which fl:: type to cast to for display
    template<typename T, fl::size Size = sizeof(T), bool IsSigned = fl::is_signed<T>::value>
    struct cast_target;

    // 1-byte unsigned → u16 (ergonomic: displays uint8_t as number, not char)
    template<typename T> struct cast_target<T, 1, false> { using type = fl::u16; };

    // 1-byte signed → i8
    template<typename T> struct cast_target<T, 1, true> { using type = fl::i8; };

    // 2-byte unsigned → u16
    template<typename T> struct cast_target<T, 2, false> { using type = fl::u16; };

    // 2-byte signed → i16
    template<typename T> struct cast_target<T, 2, true> { using type = fl::i16; };

    // 4-byte unsigned → u32
    template<typename T> struct cast_target<T, 4, false> { using type = fl::u32; };

    // 4-byte signed → i32
    template<typename T> struct cast_target<T, 4, true> { using type = fl::i32; };

    // 8-byte unsigned → u64
    template<typename T> struct cast_target<T, 8, false> { using type = fl::u64; };

    // 8-byte signed → i64
    template<typename T> struct cast_target<T, 8, true> { using type = fl::i64; };
}

//-----------------------------------------------------------------------------
// Macro for generating fundamental integer type overloads with SFINAE collision prevention
//-----------------------------------------------------------------------------
// Each fundamental type checks it's distinct from 'char' and all previously-defined types
// This prevents ambiguous overloads on platforms where types alias (e.g., AVR: int==short)
// Note: We use variadic macros to handle commas in template arguments
#define STRSTREAM_INT_OVERLOAD(TYPE, ...) \
    template<typename T> \
    typename fl::enable_if< \
        fl::is_same<T, TYPE>::value && \
        !fl::is_same<TYPE, char>::value && \
        !fl::is_same<T, char>::value && \
        (__VA_ARGS__), \
        StrStream& \
    >::type operator<<(T n) { \
        using target_t = typename strstream_detail::cast_target<TYPE>::type; \
        mStr.append(static_cast<target_t>(n)); \
        return *this; \
    }

#define FAKESTRSTREAM_INT_OVERLOAD(TYPE, ...) \
    template<typename T> \
    typename fl::enable_if< \
        fl::is_same<T, TYPE>::value && \
        !fl::is_same<TYPE, char>::value && \
        !fl::is_same<T, char>::value && \
        (__VA_ARGS__), \
        FakeStrStream& \
    >::type operator<<(T) { \
        return *this; \
    }

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

    // Non-template overload for char - takes by value to ensure priority over templates
    StrStream &operator<<(char c) {
        if (mTreatCharAsInt) {
            mStr.append(fl::i32(c));
        } else {
            mStr.append(c);
        }
        return *this;
    }

    // Non-template overloads for signed/unsigned char to avoid template resolution issues
    StrStream &operator<<(signed char n) {
        using target_t = typename strstream_detail::cast_target<signed char>::type;
        mStr.append(static_cast<target_t>(n));
        return *this;
    }

    StrStream &operator<<(unsigned char n) {
        using target_t = typename strstream_detail::cast_target<unsigned char>::type;
        mStr.append(static_cast<target_t>(n));
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

    //-------------------------------------------------------------------------
    // Fundamental integer type overloads with SFINAE collision prevention
    //-------------------------------------------------------------------------
    // Each type checks it's distinct from 'char' and all previously-defined types
    // Ordering matters: define in sequence so SFINAE excludes platform-specific aliases
    //
    // NOTE: We deliberately skip signed char and unsigned char because they may alias
    // with 'char' on some platforms, and we want the explicit char overload to handle
    // char types. Users passing signed/unsigned char will get them cast to larger types
    // via the 1-byte cast_target logic.

    // short - first integer type overload
    STRSTREAM_INT_OVERLOAD(short,
        !fl::is_same<short, signed char>::value &&
        !fl::is_same<short, unsigned char>::value)

    // unsigned short - check not same as previous types
    STRSTREAM_INT_OVERLOAD(unsigned short,
        !fl::is_same<unsigned short, signed char>::value &&
        !fl::is_same<unsigned short, unsigned char>::value &&
        !fl::is_same<unsigned short, short>::value)

    // int - check not same as any previous types (on AVR: int==short)
    STRSTREAM_INT_OVERLOAD(int,
        !fl::is_same<int, signed char>::value &&
        !fl::is_same<int, unsigned char>::value &&
        !fl::is_same<int, short>::value &&
        !fl::is_same<int, unsigned short>::value)

    // unsigned int - check not same as previous types
    STRSTREAM_INT_OVERLOAD(unsigned int,
        !fl::is_same<unsigned int, signed char>::value &&
        !fl::is_same<unsigned int, unsigned char>::value &&
        !fl::is_same<unsigned int, short>::value &&
        !fl::is_same<unsigned int, unsigned short>::value &&
        !fl::is_same<unsigned int, int>::value)

    // long - check not same as previous types (may alias to int or long long)
    STRSTREAM_INT_OVERLOAD(long,
        !fl::is_same<long, signed char>::value &&
        !fl::is_same<long, unsigned char>::value &&
        !fl::is_same<long, short>::value &&
        !fl::is_same<long, unsigned short>::value &&
        !fl::is_same<long, int>::value &&
        !fl::is_same<long, unsigned int>::value)

    // unsigned long - check not same as previous types
    STRSTREAM_INT_OVERLOAD(unsigned long,
        !fl::is_same<unsigned long, signed char>::value &&
        !fl::is_same<unsigned long, unsigned char>::value &&
        !fl::is_same<unsigned long, short>::value &&
        !fl::is_same<unsigned long, unsigned short>::value &&
        !fl::is_same<unsigned long, int>::value &&
        !fl::is_same<unsigned long, unsigned int>::value &&
        !fl::is_same<unsigned long, long>::value)

    // long long - check not same as previous types (may alias to long on some platforms)
    STRSTREAM_INT_OVERLOAD(long long,
        !fl::is_same<long long, signed char>::value &&
        !fl::is_same<long long, unsigned char>::value &&
        !fl::is_same<long long, short>::value &&
        !fl::is_same<long long, unsigned short>::value &&
        !fl::is_same<long long, int>::value &&
        !fl::is_same<long long, unsigned int>::value &&
        !fl::is_same<long long, long>::value &&
        !fl::is_same<long long, unsigned long>::value)

    // unsigned long long - check not same as previous types
    STRSTREAM_INT_OVERLOAD(unsigned long long,
        !fl::is_same<unsigned long long, signed char>::value &&
        !fl::is_same<unsigned long long, unsigned char>::value &&
        !fl::is_same<unsigned long long, short>::value &&
        !fl::is_same<unsigned long long, unsigned short>::value &&
        !fl::is_same<unsigned long long, int>::value &&
        !fl::is_same<unsigned long long, unsigned int>::value &&
        !fl::is_same<unsigned long long, long>::value &&
        !fl::is_same<unsigned long long, unsigned long>::value &&
        !fl::is_same<unsigned long long, long long>::value)

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
    FakeStrStream &operator<<(const char *) { return *this; }

    template<fl::size N>
    FakeStrStream &operator<<(const char (&)[N]) { return *this; }

    template <typename T> FakeStrStream &operator=(const T &) { return *this; }

    FakeStrStream &operator<<(const CRGB &) { return *this; }
    FakeStrStream &operator<<(const string &) { return *this; }
    FakeStrStream &operator<<(char) { return *this; }
    FakeStrStream &operator<<(signed char) { return *this; }
    FakeStrStream &operator<<(unsigned char) { return *this; }

    // bool support to match StrStream interface
    FakeStrStream &operator<<(bool) { return *this; }

    FakeStrStream &operator<<(float) { return *this; }
    FakeStrStream &operator<<(double) { return *this; }

    //-------------------------------------------------------------------------
    // Fundamental integer type overloads with SFINAE collision prevention
    //-------------------------------------------------------------------------
    // Mirror StrStream's integer handling for API compatibility
    // NOTE: Skip signed/unsigned char - handled by explicit non-template overloads above

    FAKESTRSTREAM_INT_OVERLOAD(short,
        !fl::is_same<short, signed char>::value &&
        !fl::is_same<short, unsigned char>::value)

    FAKESTRSTREAM_INT_OVERLOAD(unsigned short,
        !fl::is_same<unsigned short, signed char>::value &&
        !fl::is_same<unsigned short, unsigned char>::value &&
        !fl::is_same<unsigned short, short>::value)

    FAKESTRSTREAM_INT_OVERLOAD(int,
        !fl::is_same<int, signed char>::value &&
        !fl::is_same<int, unsigned char>::value &&
        !fl::is_same<int, short>::value &&
        !fl::is_same<int, unsigned short>::value)

    FAKESTRSTREAM_INT_OVERLOAD(unsigned int,
        !fl::is_same<unsigned int, signed char>::value &&
        !fl::is_same<unsigned int, unsigned char>::value &&
        !fl::is_same<unsigned int, short>::value &&
        !fl::is_same<unsigned int, unsigned short>::value &&
        !fl::is_same<unsigned int, int>::value)

    FAKESTRSTREAM_INT_OVERLOAD(long,
        !fl::is_same<long, signed char>::value &&
        !fl::is_same<long, unsigned char>::value &&
        !fl::is_same<long, short>::value &&
        !fl::is_same<long, unsigned short>::value &&
        !fl::is_same<long, int>::value &&
        !fl::is_same<long, unsigned int>::value)

    FAKESTRSTREAM_INT_OVERLOAD(unsigned long,
        !fl::is_same<unsigned long, signed char>::value &&
        !fl::is_same<unsigned long, unsigned char>::value &&
        !fl::is_same<unsigned long, short>::value &&
        !fl::is_same<unsigned long, unsigned short>::value &&
        !fl::is_same<unsigned long, int>::value &&
        !fl::is_same<unsigned long, unsigned int>::value &&
        !fl::is_same<unsigned long, long>::value)

    FAKESTRSTREAM_INT_OVERLOAD(long long,
        !fl::is_same<long long, signed char>::value &&
        !fl::is_same<long long, unsigned char>::value &&
        !fl::is_same<long long, short>::value &&
        !fl::is_same<long long, unsigned short>::value &&
        !fl::is_same<long long, int>::value &&
        !fl::is_same<long long, unsigned int>::value &&
        !fl::is_same<long long, long>::value &&
        !fl::is_same<long long, unsigned long>::value)

    FAKESTRSTREAM_INT_OVERLOAD(unsigned long long,
        !fl::is_same<unsigned long long, signed char>::value &&
        !fl::is_same<unsigned long long, unsigned char>::value &&
        !fl::is_same<unsigned long long, short>::value &&
        !fl::is_same<unsigned long long, unsigned short>::value &&
        !fl::is_same<unsigned long long, int>::value &&
        !fl::is_same<unsigned long long, unsigned int>::value &&
        !fl::is_same<unsigned long long, long>::value &&
        !fl::is_same<unsigned long long, unsigned long>::value &&
        !fl::is_same<unsigned long long, long long>::value)

    FakeStrStream &operator=(const string &) { return *this; }
    FakeStrStream &operator=(const CRGB &) { return *this; }
    FakeStrStream &operator=(char) { return *this; }
};


} // namespace fl

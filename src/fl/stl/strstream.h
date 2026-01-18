#pragma once

#include "fl/stl/cstdint.h"
#include "fl/str.h"
#include "crgb.h"
#include "fl/stl/ios.h"
#include "fl/stl/type_traits.h"


namespace fl {

class Tile2x2_u8;
class Tile2x2_u8_wrap;  // Forward declaration to support operator<< overload
template <typename T> struct vec2;  // Forward declaration from fl/geometry.h
template <typename T> struct rect;  // Forward declaration from fl/geometry.h
template <typename T, typename Alloc> class vector;  // Forward declaration from fl/vector.h
template <typename T> class Optional;  // Forward declaration from fl/stl/optional.h
template <typename Key, typename Hash, typename KeyEqual> class unordered_set;  // Forward declaration from fl/stl/unordered_set.h
template <typename Key, typename T, typename Hash, typename KeyEqual, int INLINED_COUNT> class unordered_map;  // Forward declaration from fl/stl/unordered_map.h
template <typename Key, typename Value, fl::size N> class FixedMap;  // Forward declaration from fl/stl/map.h
template <typename Key, typename Value, typename Less, typename Allocator> class SortedHeapMap;  // Forward declaration from fl/stl/map.h
template <typename T, fl::size Extent> class span;  // Forward declaration from fl/slice.h (no default arg to avoid redefinition)
template <typename T1, typename T2> struct pair;  // Forward declaration from fl/stl/pair.h
struct FFTBins;  // Forward declaration from fl/fft.h
template <fl::u32 N> class BitsetFixed;
class bitset_dynamic;
template <fl::u32 N> class BitsetInlined;

// Note: int_cast_detail::cast_target is now defined in fl/stl/type_traits.h
// and shared between StrStream and StrN for consistent integer formatting


class StrStream {
  public:
    StrStream() = default;
    StrStream(const string &str) : mStr(str) {}

    void setTreatCharAsInt(bool treatCharAsInt) {
        mTreatCharAsInt = treatCharAsInt;
    }

    string str() const { return mStr; }
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

    // rect<T> support - format as rect((minx,miny), (maxx,maxy))
    template<typename T>
    StrStream &operator<<(const rect<T> &r) {
        mStr.append("rect(");
        (*this) << r.mMin;
        mStr.append(", ");
        (*this) << r.mMax;
        mStr.append(")");
        return *this;
    }

    // Optional<T> support - format as nullopt or optional(value)
    template<typename T>
    StrStream &operator<<(const Optional<T> &opt) {
        if (!opt.has_value()) {
            mStr.append("nullopt");
        } else {
            mStr.append("optional(");
            (*this) << *opt;
            mStr.append(")");
        }
        return *this;
    }

    // vector<T, Alloc> support - format as [item1, item2, ...]
    template<typename T, typename Alloc>
    StrStream &operator<<(const fl::vector<T, Alloc> &vec) {
        mStr.append("[");
        for (fl::size i = 0; i < vec.size(); ++i) {
            if (i > 0) {
                mStr.append(", ");
            }
            (*this) << vec[i];
        }
        mStr.append("]");
        return *this;
    }

    // unordered_set<Key, Hash, KeyEqual> support - format as {item1, item2, ...}
    template<typename Key, typename Hash, typename KeyEqual>
    StrStream &operator<<(const fl::unordered_set<Key, Hash, KeyEqual> &set) {
        mStr.append("{");
        bool first = true;
        for (auto it = set.begin(); it != set.end(); ++it) {
            if (!first) {
                mStr.append(", ");
            }
            first = false;
            (*this) << *it;
        }
        mStr.append("}");
        return *this;
    }

    // unordered_map<Key, T, Hash, KeyEqual, INLINED_COUNT> support - format as {key1: value1, key2: value2, ...}
    template<typename Key, typename T, typename Hash, typename KeyEqual, int INLINED_COUNT>
    StrStream &operator<<(const fl::unordered_map<Key, T, Hash, KeyEqual, INLINED_COUNT> &map) {
        mStr.append("{");
        bool first = true;
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (!first) {
                mStr.append(", ");
            }
            first = false;
            (*this) << it->first;
            mStr.append(": ");
            (*this) << it->second;
        }
        mStr.append("}");
        return *this;
    }

    // FixedMap<Key, Value, N> support - format as {key1: value1, key2: value2, ...}
    template<typename Key, typename Value, fl::size N>
    StrStream &operator<<(const fl::FixedMap<Key, Value, N> &map) {
        mStr.append("{");
        bool first = true;
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (!first) {
                mStr.append(", ");
            }
            first = false;
            (*this) << it->first;
            mStr.append(": ");
            (*this) << it->second;
        }
        mStr.append("}");
        return *this;
    }

    // SortedHeapMap<Key, Value, Less, Allocator> support - format as {key1: value1, key2: value2, ...}
    template<typename Key, typename Value, typename Less, typename Allocator>
    StrStream &operator<<(const fl::SortedHeapMap<Key, Value, Less, Allocator> &map) {
        mStr.append("{");
        bool first = true;
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (!first) {
                mStr.append(", ");
            }
            first = false;
            (*this) << it->first;
            mStr.append(": ");
            (*this) << it->second;
        }
        mStr.append("}");
        return *this;
    }

    // span<T, Extent> support - format as span[item1, item2, ...]
    // Uses same format as vector but with "span" prefix for clarity
    template<typename T, fl::size Extent>
    StrStream &operator<<(const fl::span<T, Extent> &s) {
        mStr.append("span[");
        for (fl::size i = 0; i < s.size(); ++i) {
            if (i > 0) {
                mStr.append(", ");
            }
            (*this) << s[i];
        }
        mStr.append("]");
        return *this;
    }

    // pair<T1, T2> support - format as (first, second)
    template<typename T1, typename T2>
    StrStream &operator<<(const fl::pair<T1, T2> &p) {
        mStr.append("(");
        (*this) << p.first;
        mStr.append(", ");
        (*this) << p.second;
        mStr.append(")");
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
        using target_t = typename int_cast_detail::cast_target<signed char>::type;
        appendFormatted(static_cast<target_t>(n));
        return *this;
    }

    StrStream &operator<<(unsigned char n) {
        using target_t = typename int_cast_detail::cast_target<unsigned char>::type;
        appendFormatted(static_cast<target_t>(n));
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
    // Enum support - converts enum class and regular enums to their underlying integer type
    //-------------------------------------------------------------------------
    template<typename T>
    typename fl::enable_if<fl::is_enum<T>::value, StrStream&>::type
    operator<<(T e) {
        using underlying_t = typename fl::underlying_type<T>::type;
        return (*this) << static_cast<underlying_t>(e);
    }

    //-------------------------------------------------------------------------
    // Generic integer type overload using SFINAE
    //-------------------------------------------------------------------------
    // Handles all multi-byte integer types (excludes char types which are handled separately)
    // Uses is_multi_byte_integer trait to select only non-char integer types
    template<typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value, StrStream&>::type
    operator<<(T val) {
        using target_t = typename int_cast_detail::cast_target<T>::type;
        appendFormatted(static_cast<target_t>(val));
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

    // Get current formatting base (10=decimal, 16=hex, 8=octal)
    int getBase() const { return mBase; }

    // Friend operators for manipulators
    friend StrStream& operator<<(StrStream&, const hex_t&);
    friend StrStream& operator<<(StrStream&, const dec_t&);
    friend StrStream& operator<<(StrStream&, const oct_t&);

  private:
    string mStr;
    bool mTreatCharAsInt = false;  // Default to ASCII mode for readable text output
    int mBase = 10;  // Default to decimal

    // Helper methods to format integers based on current base
    void appendFormatted(fl::i8 val);
    void appendFormatted(fl::i16 val);
    void appendFormatted(fl::i32 val);
    void appendFormatted(fl::i64 val);
    void appendFormatted(fl::u16 val);
    void appendFormatted(fl::u32 val);
    void appendFormatted(fl::u64 val);
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

    // Enum support to match StrStream interface
    template<typename T>
    typename fl::enable_if<fl::is_enum<T>::value, FakeStrStream&>::type
    operator<<(T) { return *this; }

    FakeStrStream &operator<<(float) { return *this; }
    FakeStrStream &operator<<(double) { return *this; }

    //-------------------------------------------------------------------------
    // Generic integer type overload using SFINAE
    //-------------------------------------------------------------------------
    // Mirror StrStream's integer handling for API compatibility
    // Handles all multi-byte integer types (excludes char types which are handled separately)
    template<typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value, FakeStrStream&>::type
    operator<<(T) { return *this; }

    // Generic pointer and other type support for compatibility
    FakeStrStream &operator<<(const void*) { return *this; }

    // Support for strings/characters in arrays
    template<fl::size N>
    FakeStrStream &operator<<(const StrStream (&)[N]) { return *this; }

    FakeStrStream &operator<<(const StrStream &) { return *this; }
    FakeStrStream &operator<<(const Tile2x2_u8 &) { return *this; }
    FakeStrStream &operator<<(const Tile2x2_u8_wrap &) { return *this; }
    FakeStrStream &operator<<(const FFTBins &) { return *this; }

    // Vector support
    template<typename T>
    FakeStrStream &operator<<(const vec2<T> &) { return *this; }

    // rect support
    template<typename T>
    FakeStrStream &operator<<(const rect<T> &) { return *this; }

    // Optional support
    template<typename T>
    FakeStrStream &operator<<(const Optional<T> &) { return *this; }

    // vector support
    template<typename T, typename Alloc>
    FakeStrStream &operator<<(const fl::vector<T, Alloc> &) { return *this; }

    // unordered_set support
    template<typename Key, typename Hash, typename KeyEqual>
    FakeStrStream &operator<<(const fl::unordered_set<Key, Hash, KeyEqual> &) { return *this; }

    // unordered_map support
    template<typename Key, typename T, typename Hash, typename KeyEqual, int INLINED_COUNT>
    FakeStrStream &operator<<(const fl::unordered_map<Key, T, Hash, KeyEqual, INLINED_COUNT> &) { return *this; }

    // FixedMap support
    template<typename Key, typename Value, fl::size N>
    FakeStrStream &operator<<(const fl::FixedMap<Key, Value, N> &) { return *this; }

    // SortedHeapMap support
    template<typename Key, typename Value, typename Less, typename Allocator>
    FakeStrStream &operator<<(const fl::SortedHeapMap<Key, Value, Less, Allocator> &) { return *this; }

    // span support
    template<typename T, fl::size Extent>
    FakeStrStream &operator<<(const fl::span<T, Extent> &) { return *this; }

    // pair support
    template<typename T1, typename T2>
    FakeStrStream &operator<<(const fl::pair<T1, T2> &) { return *this; }

    // Bitset support
    template<fl::u32 N>
    FakeStrStream &operator<<(const BitsetFixed<N> &) { return *this; }

    FakeStrStream &operator<<(const bitset_dynamic &) { return *this; }

    template<fl::u32 N>
    FakeStrStream &operator<<(const BitsetInlined<N> &) { return *this; }

    // Support for hex and dec formatters
    FakeStrStream &operator<<(const hex_t &) { return *this; }
    FakeStrStream &operator<<(const dec_t &) { return *this; }
    FakeStrStream &operator<<(const oct_t &) { return *this; }

    FakeStrStream &operator=(const string &) { return *this; }
    FakeStrStream &operator=(const CRGB &) { return *this; }
    FakeStrStream &operator=(char) { return *this; }
};


} // namespace fl

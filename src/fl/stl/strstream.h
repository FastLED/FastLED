#pragma once

#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "crgb.h"  // IWYU pragma: keep
#include "fl/stl/ios.h"  // IWYU pragma: keep
#include "fl/stl/type_traits.h"
#include "fl/stl/noexcept.h"


namespace fl {

class Tile2x2_u8;
class Tile2x2_u8_wrap;  // Forward declaration to support operator<< overload
template <typename T> struct vec2;  // Forward declaration from fl/math/geometry.h
template <typename T> struct rect;  // Forward declaration from fl/math/geometry.h
template <typename T> class vector;  // Forward declaration from fl/vector.h  // IWYU pragma: keep
template <typename T> class Optional;  // Forward declaration from fl/stl/optional.h
template <typename Key, typename Hash, typename KeyEqual> class unordered_set;  // Forward declaration from fl/stl/unordered_set.h
template <typename Key, typename T, typename Hash, typename KeyEqual, int INLINED_COUNT> class unordered_map;  // Forward declaration from fl/stl/unordered_map.h
template <typename Key, typename Value, fl::size N> class unsorted_map_fixed;  // Forward declaration from fl/stl/map.h
template <typename Key, typename Value, typename Less> class flat_map;  // Forward declaration from fl/stl/flat_map.h
template <typename T, fl::size Extent> class span;  // Forward declaration from fl/stl/span.h (no default arg to avoid redefinition)  // IWYU pragma: keep
template <typename T1, typename T2> struct pair;  // Forward declaration from fl/stl/pair.h
namespace audio { namespace fft { class Bins; } }  // Forward declaration
template <fl::u32 N> class bitset_fixed;
class bitset_dynamic;  // IWYU pragma: keep
template <fl::u32 N> class bitset_inlined;  // IWYU pragma: keep

// Note: int_cast_detail::cast_target is now defined in fl/stl/type_traits.h
// and shared between sstream and basic_string for consistent integer formatting


class sstream {
  public:
    sstream() FL_NOEXCEPT = default;
    sstream(const string &str) FL_NOEXCEPT : mStr(str) {}

    void setTreatCharAsInt(bool treatCharAsInt) FL_NOEXCEPT {
        mTreatCharAsInt = treatCharAsInt;
    }

    string str() const FL_NOEXCEPT { return mStr; }
    const char *c_str() const FL_NOEXCEPT { return mStr.c_str(); }

    sstream &operator<<(const CRGB &rgb) FL_NOEXCEPT {
        mStr.append(rgb);
        return *this;
    }
    sstream &operator<<(const sstream &strStream) FL_NOEXCEPT {
        mStr.append(strStream.str());
        return *this;
    }

    sstream &operator<<(const Tile2x2_u8 &subpixel) FL_NOEXCEPT;
    sstream &operator<<(const Tile2x2_u8_wrap &tile) FL_NOEXCEPT;  // New overload for wrapped tiles

    // Bins support - implemented in strstream.cpp.hpp
    sstream &operator<<(const audio::fft::Bins &bins) FL_NOEXCEPT;

    // vec2<T> support - format as (x,y)
    template<typename T>
    sstream &operator<<(const vec2<T> &v) FL_NOEXCEPT {
        mStr.append("(");
        mStr.append(v.x);
        mStr.append(",");
        mStr.append(v.y);
        mStr.append(")");
        return *this;
    }

    // rect<T> support - format as rect((minx,miny), (maxx,maxy))
    template<typename T>
    sstream &operator<<(const rect<T> &r) FL_NOEXCEPT {
        mStr.append("rect(");
        (*this) << r.mMin;
        mStr.append(", ");
        (*this) << r.mMax;
        mStr.append(")");
        return *this;
    }

    // Optional<T> support - format as nullopt or optional(value)
    template<typename T>
    sstream &operator<<(const Optional<T> &opt) FL_NOEXCEPT {
        if (!opt.has_value()) {
            mStr.append("nullopt");
        } else {
            mStr.append("optional(");
            (*this) << *opt;
            mStr.append(")");
        }
        return *this;
    }

    // vector<T> support - format as [item1, item2, ...]
    template<typename T>
    sstream &operator<<(const fl::vector<T> &vec) FL_NOEXCEPT {
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
    sstream &operator<<(const fl::unordered_set<Key, Hash, KeyEqual> &set) FL_NOEXCEPT {
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
    sstream &operator<<(const fl::unordered_map<Key, T, Hash, KeyEqual, INLINED_COUNT> &map) FL_NOEXCEPT {
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

    // unsorted_map_fixed<Key, Value, N> support - format as {key1: value1, key2: value2, ...}
    template<typename Key, typename Value, fl::size N>
    sstream &operator<<(const fl::unsorted_map_fixed<Key, Value, N> &map) FL_NOEXCEPT {
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

    // flat_map<Key, Value, Less> support - format as {key1: value1, key2: value2, ...}
    template<typename Key, typename Value, typename Less>
    sstream &operator<<(const fl::flat_map<Key, Value, Less> &map) FL_NOEXCEPT {
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
    sstream &operator<<(const fl::span<T, Extent> &s) FL_NOEXCEPT {
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
    sstream &operator<<(const fl::pair<T1, T2> &p) FL_NOEXCEPT {
        mStr.append("(");
        (*this) << p.first;
        mStr.append(", ");
        (*this) << p.second;
        mStr.append(")");
        return *this;
    }

    sstream &operator=(const fl::u16 &n) FL_NOEXCEPT {
        mStr.clear();
        (*this) << n;
        return *this;
    }

    sstream &operator=(const fl::u8 &n) FL_NOEXCEPT {
        mStr.clear();
        (*this) << n;
        return *this;
    }

    sstream &operator=(char c) FL_NOEXCEPT {
        mStr.clear();
        (*this) << c;
        return *this;
    }

    // << operator section
    sstream &operator<<(const string &str) FL_NOEXCEPT {
        mStr.append(str);
        return *this;
    }

    sstream &operator<<(const char *str) FL_NOEXCEPT {
        if (str) {
            mStr.append(str);
        }
        return *this;
    }

    sstream &operator<<(const float &f) FL_NOEXCEPT {
        // multiply by 100 and round to get 2 decimal places
        mStr.append(f);
        return *this;
    }

    sstream &operator<<(const double &f) FL_NOEXCEPT {
        // multiply by 100 and round to get 2 decimal places
        mStr.append(f);
        return *this;
    }

    // Non-template overload for char - takes by value to ensure priority over templates
    sstream &operator<<(char c) FL_NOEXCEPT {
        if (mTreatCharAsInt) {
            mStr.append(fl::i32(c));
        } else {
            mStr.append(c);
        }
        return *this;
    }

    // Non-template overloads for signed/unsigned char to avoid template resolution issues
    sstream &operator<<(signed char n) FL_NOEXCEPT {
        using target_t = typename int_cast_detail::cast_target<signed char>::type;
        appendFormatted(static_cast<target_t>(n));
        return *this;
    }

    sstream &operator<<(unsigned char n) FL_NOEXCEPT {
        using target_t = typename int_cast_detail::cast_target<unsigned char>::type;
        appendFormatted(static_cast<target_t>(n));
        return *this;
    }

    template<fl::size N>
    sstream &operator<<(const char (&str)[N]) FL_NOEXCEPT {
        mStr.append(str);
        return *this;
    }

    template<fl::u32 N>
    sstream &operator<<(const bitset_fixed<N> &bs) FL_NOEXCEPT {
        // mStr.append(bs);
        bs.to_string(&mStr);
        return *this;
    }

    sstream &operator<<(const bitset_dynamic &bs) FL_NOEXCEPT {
        bs.to_string(&mStr);
        return *this;
    }

    template<fl::u32 N>
    sstream &operator<<(const bitset_inlined<N> &bs) FL_NOEXCEPT {
        bs.to_string(&mStr);
        return *this;
    }

    // bool support - output as "true"/"false" for readability
    sstream &operator<<(bool b) FL_NOEXCEPT {
        mStr.append(b ? "true" : "false");
        return *this;
    }

    //-------------------------------------------------------------------------
    // Enum support - converts enum class and regular enums to their underlying integer type
    //-------------------------------------------------------------------------
    template<typename T>
    typename fl::enable_if<fl::is_enum<T>::value, sstream&>::type
    operator<<(T e) FL_NOEXCEPT {
        using underlying_t = typename fl::underlying_type<T>::type;
        return (*this) << static_cast<underlying_t>(e);
    }

    //-------------------------------------------------------------------------
    // Generic integer type overload using SFINAE
    //-------------------------------------------------------------------------
    // Handles all multi-byte integer types (excludes char types which are handled separately)
    // Uses is_multi_byte_integer trait to select only non-char integer types
    template<typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value, sstream&>::type
    operator<<(T val) FL_NOEXCEPT {
        using target_t = typename int_cast_detail::cast_target<T>::type;
        appendFormatted(static_cast<target_t>(val));
        return *this;
    }

    //-------------------------------------------------------------------------
    // Types with to_float() method (e.g., fixed_point types)
    //-------------------------------------------------------------------------
    template<typename T>
    typename fl::enable_if<
        fl::is_same<decltype(static_cast<const T*>(nullptr)->to_float()) FL_NOEXCEPT , float>::value
        && !fl::is_floating_point<T>::value,
        sstream&>::type
    operator<<(const T &val) {
        mStr.append(val.to_float());
        return *this;
    }

    // assignment operator completely replaces the current string
    sstream &operator=(const string &str) FL_NOEXCEPT {
        mStr = str;
        return *this;
    }

    sstream &operator=(const char *str) FL_NOEXCEPT {
        mStr.clear();
        mStr.append(str);
        return *this;
    }
    

    // crgb
    sstream &operator=(const CRGB &rgb) FL_NOEXCEPT {
        mStr.clear();
        (*this) << rgb;
        return *this;
    }

    void clear() FL_NOEXCEPT { mStr.clear(); }

    // Get current formatting base (10=decimal, 16=hex, 8=octal)
    int getBase() const FL_NOEXCEPT { return mBase; }

    // Friend operators for manipulators
    friend sstream& operator<<(sstream&, const hex_t&) FL_NOEXCEPT;
    friend sstream& operator<<(sstream&, const dec_t&) FL_NOEXCEPT;
    friend sstream& operator<<(sstream&, const oct_t&) FL_NOEXCEPT;

  private:
    string mStr;
    bool mTreatCharAsInt = false;  // Default to ASCII mode for readable text output
    int mBase = 10;  // Default to decimal

    // Helper methods to format integers based on current base
    void appendFormatted(fl::i8 val) FL_NOEXCEPT;
    void appendFormatted(fl::i16 val) FL_NOEXCEPT;
    void appendFormatted(fl::i32 val) FL_NOEXCEPT;
    void appendFormatted(fl::i64 val) FL_NOEXCEPT;
    void appendFormatted(fl::u16 val) FL_NOEXCEPT;
    void appendFormatted(fl::u32 val) FL_NOEXCEPT;
    void appendFormatted(fl::u64 val) FL_NOEXCEPT;
};

class sstream_noop {
  public:
    sstream_noop &operator<<(const char *) FL_NOEXCEPT { return *this; }

    template<fl::size N>
    sstream_noop &operator<<(const char (&)[N]) FL_NOEXCEPT { return *this; }

    template <typename T> sstream_noop &operator=(const T &) FL_NOEXCEPT { return *this; }

    sstream_noop &operator<<(const CRGB &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(const string &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(char) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(signed char) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(unsigned char) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(short) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(unsigned short) FL_NOEXCEPT { return *this; }

    // bool support to match sstream interface
    sstream_noop &operator<<(bool) FL_NOEXCEPT { return *this; }

    // Enum support to match sstream interface
    template<typename T>
    typename fl::enable_if<fl::is_enum<T>::value, sstream_noop&>::type
    operator<<(T) FL_NOEXCEPT { return *this; }

    sstream_noop &operator<<(float) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(double) FL_NOEXCEPT { return *this; }

    //-------------------------------------------------------------------------
    // Generic integer type overload using SFINAE
    //-------------------------------------------------------------------------
    // Mirror sstream's integer handling for API compatibility
    // Handles all multi-byte integer types (excludes char types which are handled separately)
    template<typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value, sstream_noop&>::type
    operator<<(T) FL_NOEXCEPT { return *this; }

    // Generic pointer and other type support for compatibility
    sstream_noop &operator<<(const void*) FL_NOEXCEPT { return *this; }

    // Support for strings/characters in arrays
    template<fl::size N>
    sstream_noop &operator<<(const sstream (&)[N]) FL_NOEXCEPT { return *this; }

    sstream_noop &operator<<(const sstream &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(const Tile2x2_u8 &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(const Tile2x2_u8_wrap &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(const audio::fft::Bins &) FL_NOEXCEPT { return *this; }

    // Vector support
    template<typename T>
    sstream_noop &operator<<(const vec2<T> &) FL_NOEXCEPT { return *this; }

    // rect support
    template<typename T>
    sstream_noop &operator<<(const rect<T> &) FL_NOEXCEPT { return *this; }

    // Optional support
    template<typename T>
    sstream_noop &operator<<(const Optional<T> &) FL_NOEXCEPT { return *this; }

    // vector support
    template<typename T>
    sstream_noop &operator<<(const fl::vector<T> &) FL_NOEXCEPT { return *this; }

    // unordered_set support
    template<typename Key, typename Hash, typename KeyEqual>
    sstream_noop &operator<<(const fl::unordered_set<Key, Hash, KeyEqual> &) FL_NOEXCEPT { return *this; }

    // unordered_map support
    template<typename Key, typename T, typename Hash, typename KeyEqual, int INLINED_COUNT>
    sstream_noop &operator<<(const fl::unordered_map<Key, T, Hash, KeyEqual, INLINED_COUNT> &) FL_NOEXCEPT { return *this; }

    // unsorted_map_fixed support
    template<typename Key, typename Value, fl::size N>
    sstream_noop &operator<<(const fl::unsorted_map_fixed<Key, Value, N> &) FL_NOEXCEPT { return *this; }

    // flat_map support
    template<typename Key, typename Value, typename Less>
    sstream_noop &operator<<(const fl::flat_map<Key, Value, Less> &) FL_NOEXCEPT { return *this; }

    // span support
    template<typename T, fl::size Extent>
    sstream_noop &operator<<(const fl::span<T, Extent> &) FL_NOEXCEPT { return *this; }

    // pair support
    template<typename T1, typename T2>
    sstream_noop &operator<<(const fl::pair<T1, T2> &) FL_NOEXCEPT { return *this; }

    // Bitset support
    template<fl::u32 N>
    sstream_noop &operator<<(const bitset_fixed<N> &) FL_NOEXCEPT { return *this; }

    sstream_noop &operator<<(const bitset_dynamic &) FL_NOEXCEPT { return *this; }

    template<fl::u32 N>
    sstream_noop &operator<<(const bitset_inlined<N> &) FL_NOEXCEPT { return *this; }

    // Support for hex and dec formatters
    sstream_noop &operator<<(const hex_t &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(const dec_t &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator<<(const oct_t &) FL_NOEXCEPT { return *this; }

    sstream_noop &operator=(const string &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator=(const CRGB &) FL_NOEXCEPT { return *this; }
    sstream_noop &operator=(char) FL_NOEXCEPT { return *this; }
};


} // namespace fl

#pragma once

/// std::string compatible string class.
// fl::string has inlined memory and copy on write semantics.


#include "fl/stl/basic_string.h"
#include "fl/stl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"  // IWYU pragma: keep (for FL_IS_WASM)

#ifdef FL_IS_WASM
// IWYU pragma: begin_keep
#include <string>
// IWYU pragma: end_keep
#endif

#include "fl/stl/shared_ptr.h"
#include "fl/stl/optional.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"  // IWYU pragma: keep
#include "fl/stl/variant.h"
#include "fl/stl/string_view.h"
#include "fl/stl/bitset_dynamic.h"
#include "fl/stl/bitset.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"

#ifndef FASTLED_STR_INLINED_SIZE
#define FASTLED_STR_INLINED_SIZE 64
#endif


// Test for Arduino String class (must be after includes)
#if defined(String_class_h)
#define FL_STRING_NEEDS_ARDUINO_CONVERSION 1
#else
#define FL_STRING_NEEDS_ARDUINO_CONVERSION 0
#endif

#if FL_STRING_NEEDS_ARDUINO_CONVERSION
class String;  // Arduino String class
#endif

namespace fl {

class Tile2x2_u8_wrap;
class JsonUiInternal;

struct json_value;
class json;

template <typename T> struct rect;
template <typename T> struct vec2;
template <typename T> struct vec3;

template <typename T> class WeakPtr;

template <typename Key, typename Hash, typename KeyEqual> class HashSet;

class XYMap;

namespace audio { namespace fft { class Bins; } }
struct CRGB;

} // namespace fl

namespace fl {

// Thin template wrapper around basic_string. Provides the inline buffer storage.
// All logic is in basic_string (compiled once). StrN<N> just holds char[N].
template <fl::size SIZE = FASTLED_STR_INLINED_SIZE> class StrN : public basic_string {
  protected:
    char mInlineBuffer[SIZE] = {0};

  public:
    using basic_string::npos;
    using basic_string::copy;
    using basic_string::assign;

    // ======= STATIC FACTORY METHODS =======
    static StrN from_literal(const char* literal) FL_NOEXCEPT {
        StrN result;
        result.setLiteral(literal);
        return result;
    }

    static StrN from_view(const char* data, fl::size len) FL_NOEXCEPT {
        StrN result;
        result.setView(data, len);
        return result;
    }

    static StrN from_view(const string_view& sv) FL_NOEXCEPT {
        return from_view(sv.data(), sv.size());
    }

    // ======= CONSTRUCTORS =======
    StrN() FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        // mInlineBuffer already zero-initialized by = {0}
    }

    StrN(const char* str) FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        if (str) copy(str);
    }

    StrN(const StrN& other) FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        copy(static_cast<const basic_string&>(other));
    }

    template <fl::size M>
    StrN(const StrN<M>& other) FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        copy(static_cast<const basic_string&>(other));
    }

    StrN(StrN&& other) FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        moveFrom(fl::move(other));
    }

    StrN(const string_view& sv) FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        if (!sv.empty()) {
            copy(sv.data(), sv.size());
        }
    }

    StrN(const fl::shared_ptr<StringHolder>& holder) FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        setSharedHolder(holder);
    }

    template <typename InputIt>
    StrN(InputIt first, InputIt last) FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        assign(first, last);
    }

    template <int N> StrN(const char (&str)[N]) FL_NOEXCEPT : basic_string(mInlineBuffer, SIZE) {
        copy(str, N - 1);
    }

    // ======= ASSIGNMENT =======
    StrN& operator=(const StrN& other) FL_NOEXCEPT {
        copy(static_cast<const basic_string&>(other));
        return *this;
    }

    template <fl::size M>
    StrN& operator=(const StrN<M>& other) FL_NOEXCEPT {
        copy(static_cast<const basic_string&>(other));
        return *this;
    }

    StrN& operator=(StrN&& other) FL_NOEXCEPT {
        moveAssign(fl::move(other));
        return *this;
    }

    template <int N> StrN& operator=(const char (&str)[N]) FL_NOEXCEPT {
        assign(str, N - 1);
        return *this;
    }

    // ======= SUBSTRING (returns StrN, so must be here) =======
    StrN substring(fl::size start, fl::size end) const FL_NOEXCEPT {
        if (start == 0 && end == size()) return *this;
        if (start >= size()) return StrN();
        if (end > size()) end = size();
        if (start >= end) return StrN();
        StrN out;
        out.copy(c_str() + start, end - start);
        return out;
    }

    StrN substr(fl::size start, fl::size length) const FL_NOEXCEPT {
        fl::size end = start + length;
        if (end > size()) end = size();
        return substring(start, end);
    }

    StrN substr(fl::size start) const FL_NOEXCEPT {
        return substring(start, size());
    }

    StrN trim() const FL_NOEXCEPT {
        fl::size start = 0;
        fl::size end_pos = size();
        while (start < size() && fl::isspace(c_str()[start])) start++;
        while (end_pos > start && fl::isspace(c_str()[end_pos - 1])) end_pos--;
        return substring(start, end_pos);
    }

    // ======= DESTRUCTOR =======
    ~StrN() FL_NOEXCEPT {}
};

class string : public StrN<FASTLED_STR_INLINED_SIZE> {
  public:
    static constexpr fl::size npos = static_cast<fl::size>(-1);

    using basic_string::copy;
    using basic_string::assign;
    using basic_string::append;
    using basic_string::appendHex;
    using basic_string::appendOct;

    // ======= STATIC FACTORY METHODS =======
    static string from_literal(const char* literal) FL_NOEXCEPT {
        string result;
        result.setLiteral(literal);
        return result;
    }

    static string from_view(const char* data, fl::size len) FL_NOEXCEPT {
        string result;
        result.setView(data, len);
        return result;
    }

    static string from_view(const string_view& sv) FL_NOEXCEPT {
        return from_view(sv.data(), sv.size());
    }

    static string interned(const char* str, fl::size len) FL_NOEXCEPT {
        string result;
        if (!str || len == 0) return result;
        auto holder = fl::make_shared<StringHolder>(str, len);
        result.setSharedHolder(holder);
        return result;
    }

    static string interned(const char* str) FL_NOEXCEPT {
        if (!str) return string();
        return interned(str, fl::strlen(str));
    }

    static string interned(const string_view& sv) FL_NOEXCEPT {
        return interned(sv.data(), sv.size());
    }

    static string copy_no_view(const string& str) FL_NOEXCEPT {
        if (str.is_referencing()) {
            string result;
            result.copy(str.c_str(), str.size());
            return result;
        }
        return str;
    }

    static int strcmp(const string& a, const string& b);

    // ======= CONSTRUCTORS =======
    string() FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>() {}
    string(const char* str) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>(str) {}
    string(const char* str, fl::size len) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>() {
        copy(str, len);
    }
    string(fl::size len, char c) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>() {
        resize(len, c);
    }
    string(const string& other) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>(other) {}
    string(string&& other) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>(fl::move(other)) {}
    template <fl::size M>
    string(const StrN<M>& other) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>(other) {}
    string(const basic_string& other) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>() {
        copy(other);
    }
    string(const string_view& sv) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>(sv) {}
    string(const fl::span<const char>& s) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>() {
        copy(s.data(), s.size());
    }
    string(const fl::span<char>& s) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>() {
        copy(s.data(), s.size());
    }
    string(const fl::shared_ptr<StringHolder>& holder) FL_NOEXCEPT : StrN<FASTLED_STR_INLINED_SIZE>(holder) {}

    // ======= ASSIGNMENT =======
    string& operator=(const string& other) FL_NOEXCEPT {
        copy(static_cast<const basic_string&>(other));
        return *this;
    }
    string& operator=(string&& other) FL_NOEXCEPT {
        moveAssign(fl::move(other));
        return *this;
    }
    string& operator=(const char* str) FL_NOEXCEPT {
        copy(str, fl::strlen(str));
        return *this;
    }

    // Bring base class assign methods into scope
    using StrN<FASTLED_STR_INLINED_SIZE>::assign;

    string& assign(string_view sv) FL_NOEXCEPT {
        if (sv.empty()) {
            clear();
        } else {
            copy(sv.data(), sv.size());
        }
        return *this;
    }

#ifdef FL_IS_WASM
    string(const std::string& str) : StrN<FASTLED_STR_INLINED_SIZE>() {  // okay std namespace
        copy(str.c_str(), str.size());
    }
    string& operator=(const std::string& str) FL_NOEXCEPT {  // okay std namespace
        copy(str.c_str(), str.size());
        return *this;
    }
    string& append(const std::string& str) {  // okay std namespace
        write(str.c_str(), str.size());
        return *this;
    }
#endif

#if FL_STRING_NEEDS_ARDUINO_CONVERSION
    string(const ::String& str);
    string& operator=(const ::String& str) FL_NOEXCEPT;
    string& append(const ::String& str);
#endif



    bool operator>(const string &other) const FL_NOEXCEPT {
        return fl::strcmp(c_str(), other.c_str()) > 0;
    }

    bool operator>=(const string &other) const FL_NOEXCEPT {
        return fl::strcmp(c_str(), other.c_str()) >= 0;
    }

    bool operator<(const string &other) const FL_NOEXCEPT {
        return fl::strcmp(c_str(), other.c_str()) < 0;
    }

    bool operator<=(const string &other) const FL_NOEXCEPT {
        return fl::strcmp(c_str(), other.c_str()) <= 0;
    }

    bool operator==(const string &other) const FL_NOEXCEPT {
        if (size() != other.size()) {
            return false;
        }
        return fl::memcmp(c_str(), other.c_str(), size()) == 0;
    }

    bool operator!=(const string &other) const FL_NOEXCEPT {
        return !(*this == other);
    }

    // ======= CONCATENATION =======
    string& operator+=(const string& other) FL_NOEXCEPT {
        append(other.c_str(), other.size());
        return *this;
    }

    template <typename T> string& operator+=(const T& val) FL_NOEXCEPT {
        append(val);
        return *this;
    }

    // ======= APPEND OVERLOADS =======
    template <fl::u32 N>
    string& append(const bitset_fixed<N>& bs) FL_NOEXCEPT {
        bs.to_string(this);
        return *this;
    }

    string& append(const bitset_dynamic& bs) FL_NOEXCEPT {
        bs.to_string(this);
        return *this;
    }

    template <fl::u32 N>
    string& append(const bitset_inlined<N>& bs) FL_NOEXCEPT {
        bs.to_string(this);
        return *this;
    }

    template <typename T> string& append(const fl::span<T>& slice) FL_NOEXCEPT {
        append("[");
        for (fl::size i = 0; i < slice.size(); ++i) {
            if (i > 0) append(", ");
            append(slice[i]);
        }
        append("]");
        return *this;
    }

    template <typename T> string& append(const fl::vector<T>& vec) FL_NOEXCEPT {
        fl::span<const T> slice(vec.data(), vec.size());
        append(slice);
        return *this;
    }

    template <typename T, fl::size N>
    string& append(const fl::InlinedVector<T, N>& vec) FL_NOEXCEPT {
        fl::span<const T> slice(vec.data(), vec.size());
        append(slice);
        return *this;
    }

    template <typename T>
    string& append(const rect<T>& rect) FL_NOEXCEPT {
        append("rect(");
        append(rect.mMin.x);
        append(",");
        append(rect.mMin.y);
        append(",");
        append(rect.mMax.x);
        append(",");
        append(rect.mMax.y);
        append(")");
        return *this;
    }

    template <typename T>
    string& append(const vec2<T>& pt) FL_NOEXCEPT {
        append("vec2(");
        append(pt.x);
        append(",");
        append(pt.y);
        append(")");
        return *this;
    }

    template <typename T>
    string& append(const vec3<T>& pt) FL_NOEXCEPT {
        append("vec3(");
        append(pt.x);
        append(",");
        append(pt.y);
        append(",");
        append(pt.z);
        append(")");
        return *this;
    }

    template <typename T>
    string& append(const WeakPtr<T>& val) FL_NOEXCEPT {
        write("WeakPtr(use_count=", 18);
        write(static_cast<fl::u32>(val.use_count()));
        write(")", 1);
        return *this;
    }

    template <typename T>
    string& append(const fl::shared_ptr<T>& val) FL_NOEXCEPT {
        if (!val) {
            write("nullptr", 7);
        } else {
            write("shared_ptr(use_count=", 21);
            write(static_cast<fl::u32>(val.use_count()));
            write(")", 1);
        }
        return *this;
    }

    string& append(const JsonUiInternal& val);
    string& append(const json_value& val);
    string& append(const json& val);

    template <typename T, fl::size N>
    string& append(const fl::FixedVector<T, N>& vec) FL_NOEXCEPT {
        append("[");
        for (fl::size i = 0; i < vec.size(); ++i) {
            if (i > 0) append(",");
            append(vec[i]);
        }
        append("]");
        return *this;
    }

    string& append(const CRGB& c);
    string& appendCRGB(const CRGB& c);

    // Any type with a to_float() method (e.g., fixed_point types)
    template <typename T>
    typename fl::enable_if<
        fl::is_same<decltype(static_cast<const T*>(nullptr)->to_float()), float>::value
        && !fl::is_floating_point<T>::value,
        string&>::type
    append(const T& val) {
        basic_string::append(val.to_float());
        return *this;
    }

    string& append(const audio::fft::Bins& str);
    string& append(const XYMap& map);
    string& append(const Tile2x2_u8_wrap& tile);

    template <typename Key, typename Hash, typename KeyEqual>
    string& append(const HashSet<Key, Hash, KeyEqual>& set) FL_NOEXCEPT {
        append("{");
        fl::size i = 0;
        for (auto it = set.begin(); it != set.end(); ++it) {
            if (i > 0) append(",");
            append(*it);
            ++i;
        }
        append("}");
        return *this;
    }

    template <typename T>
    string& append(const fl::optional<T>& opt) FL_NOEXCEPT {
        if (opt.has_value()) {
            append(opt.value());
        } else {
            write("nullopt", 7);
        }
        return *this;
    }

    void swap(string& other);

    string& intern();

  private:
    enum {
        kStrInlineSize = FASTLED_STR_INLINED_SIZE,
    };

    static void compileTimeAssertions();
};

// ======= FREE FUNCTIONS =======

template<typename T>
inline string to_string(T value) FL_NOEXCEPT {
    string result;
    result.append(value);
    return result;
}

inline string to_string(float value, int precision) FL_NOEXCEPT {
    string result;
    result.append(value, precision);
    return result;
}

inline string operator+(const char* lhs, const string& rhs) FL_NOEXCEPT {
    string result(lhs);
    result += rhs;
    return result;
}

inline string operator+(const string& lhs, const char* rhs) FL_NOEXCEPT {
    string result(lhs);
    result += rhs;
    return result;
}

inline string operator+(const string& lhs, const string& rhs) FL_NOEXCEPT {
    string result(lhs);
    result += rhs;
    return result;
}

template<typename T>
inline typename fl::enable_if<!fl::is_arithmetic<T>::value, string>::type
operator+(const char* lhs, const T& rhs) {
    string result(lhs);
    result += rhs;
    return result;
}

template<typename T>
inline typename fl::enable_if<!fl::is_arithmetic<T>::value, string>::type
operator+(const T& lhs, const char* rhs) {
    string result;
    result.append(lhs);
    result += rhs;
    return result;
}

template<typename T>
inline string operator+(const string& lhs, const T& rhs) FL_NOEXCEPT {
    string result(lhs);
    result += rhs;
    return result;
}

template<typename T>
inline string operator+(const T& lhs, const string& rhs) FL_NOEXCEPT {
    string result;
    result.append(lhs);
    result += rhs;
    return result;
}

///////////////////////////////////////////////////////////////////////////////
// Template Implementation for charconv.h
///////////////////////////////////////////////////////////////////////////////

template<typename T>
fl::string to_hex(T value, bool uppercase, bool pad_to_width) FL_NOEXCEPT {
    constexpr auto width = detail::get_hex_int_width<sizeof(T)>();
    bool is_negative = false;
    u64 unsigned_value;
    if (static_cast<i64>(value) < 0 && sizeof(T) <= 8) {
        is_negative = true;
        unsigned_value = static_cast<u64>(-static_cast<i64>(value));
    } else {
        unsigned_value = static_cast<u64>(value);
    }
    return detail::hex(unsigned_value, width, is_negative, uppercase, pad_to_width);
}

// Comparator that orders strings by size first, then by content.
// Not lexicographic — use only for associative containers (flat_map, etc.)
// where you need fast lookup and don't care about alphabetical order.
struct StringFastLess {
    bool operator()(const string &a, const string &b) const {
        fl::size al = a.size(), bl = b.size();
        if (al != bl) {
            return al < bl;
        }
        return fl::memcmp(a.c_str(), b.c_str(), al) < 0;
    }
};

} // namespace fl

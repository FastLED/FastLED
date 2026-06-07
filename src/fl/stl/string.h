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

// `fl::string_n<N>` is the templated inline-buffer storage policy
// over `fl::basic_string`. It co-locates a `char[N]` buffer with
// the basic_string state and passes that buffer to the base ctor.
// Every operation (write, append, find, replace, …) lives in
// `basic_string` and is compiled exactly once; per-N instantiations
// of `string_n<N>` are thin constructor stubs that only differ in
// the literal `N` they hand to the base. The compiler / linker
// folds identical instantiations under COMDAT, and the non-shared
// bytes are tiny (each ctor is `: basic_string(mBuf, N) {}` plus a
// short body that calls `copy()`/`setLiteral()`/etc.).
//
// Convenience aliases:
//   - `fl::string_small`  — 32-byte inline buffer (constrained MCUs)
//   - `fl::string`        — `FASTLED_STR_INLINED_SIZE` (default 64)
//   - `fl::string_large`  — 256-byte inline buffer (text-heavy paths)
//
// All three are layout-compatible; copy/move between sizes works
// because the underlying `basic_string` storage policy is uniform.
//
// See `agents/docs/string-architecture.md`.
template <fl::size N>
class string_n : public basic_string {
  protected:
    char mInlineBuffer[N] = {0};

  public:
    // Default + content-population constructors. All delegate to
    // `basic_string(mInlineBuffer, N)` and then call the
    // non-template population helpers on the base.
    string_n() FL_NOEXCEPT : basic_string(mInlineBuffer, N) {}

    string_n(const char* str) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        if (str) copy(str);
    }

    string_n(const char* str, fl::size len) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        copy(str, len);
    }

    string_n(fl::size len, char c) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        resize(len, c);
    }

    string_n(const string_n& other) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        copy(static_cast<const basic_string&>(other));
    }

    template <fl::size M>
    string_n(const string_n<M>& other) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        copy(static_cast<const basic_string&>(other));
    }

    string_n(string_n&& other) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        moveFrom(fl::move(other));
    }

    string_n(const basic_string& other) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        copy(other);
    }

    string_n(const string_view& sv) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        if (!sv.empty()) {
            copy(sv.data(), sv.size());
        }
    }

    string_n(const fl::span<const char>& s) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        copy(s.data(), s.size());
    }

    string_n(const fl::span<char>& s) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        copy(s.data(), s.size());
    }

    string_n(const fl::shared_ptr<StringHolder>& holder) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        setSharedHolder(holder);
    }

    template <typename InputIt>
    string_n(InputIt first, InputIt last) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        assign(first, last);
    }

    template <int M>
    string_n(const char (&str)[M]) FL_NOEXCEPT : basic_string(mInlineBuffer, N) {
        copy(str, M - 1);
    }

    string_n& operator=(const string_n& other) FL_NOEXCEPT {
        copy(static_cast<const basic_string&>(other));
        return *this;
    }

    template <fl::size M>
    string_n& operator=(const string_n<M>& other) FL_NOEXCEPT {
        copy(static_cast<const basic_string&>(other));
        return *this;
    }

    string_n& operator=(string_n&& other) FL_NOEXCEPT {
        moveAssign(fl::move(other));
        return *this;
    }
};

// Convenience aliases. `fl::string` is the default; `string_small`
// trades inline capacity for object size on constrained targets,
// `string_large` trades the other way for text-heavy paths.
using string_small = string_n<32>;
using string_large = string_n<256>;

// `fl::string` extends `string_n<FASTLED_STR_INLINED_SIZE>` with
// the composite-type formatter overloads (CRGB, vec2, span,
// vector, optional, …), the `substring()` / `trim()` family
// (returning `string` so callers chain naturally), and the static
// factory + comparison methods. The trampoline pattern is:
//
//   fl::string -> fl::string_n<N> -> fl::basic_string
//
// where only `basic_string` carries the real bytes; the upper
// layers are thin wrappers that get folded out by the optimizer.
class string : public string_n<FASTLED_STR_INLINED_SIZE> {
  public:
    static constexpr fl::size npos = static_cast<fl::size>(-1);

    using basic_string::copy;
    using basic_string::assign;
    using basic_string::append;
    using basic_string::appendHex;
    using basic_string::appendOct;

    // ======= STATIC FACTORY METHODS =======
    static string from_literal(const char* literal) FL_NOEXCEPT;
    static string from_view(const char* data, fl::size len) FL_NOEXCEPT;
    static string from_view(const string_view& sv) FL_NOEXCEPT;
    static string interned(const char* str, fl::size len) FL_NOEXCEPT;
    static string interned(const char* str) FL_NOEXCEPT;
    static string interned(const string_view& sv) FL_NOEXCEPT;
    static string copy_no_view(const string& str) FL_NOEXCEPT;
    static int strcmp(const string& a, const string& b) FL_NOEXCEPT;

    // ======= CONSTRUCTORS =======
    // All non-template ctors delegate to string_n<N>'s ctors —
    // defined in string.cpp.hpp as one-line forwarders for header
    // hygiene. Template ctors stay inline (they're parameterised
    // on caller types and can't be split out).
    string() FL_NOEXCEPT;
    string(const char* str) FL_NOEXCEPT;
    string(const char* str, fl::size len) FL_NOEXCEPT;
    string(fl::size len, char c) FL_NOEXCEPT;
    string(const string& other) FL_NOEXCEPT;
    string(string&& other) FL_NOEXCEPT;
    string(const basic_string& other) FL_NOEXCEPT;
    string(const string_view& sv) FL_NOEXCEPT;
    string(const fl::span<const char>& s) FL_NOEXCEPT;
    string(const fl::span<char>& s) FL_NOEXCEPT;
    string(const fl::shared_ptr<StringHolder>& holder) FL_NOEXCEPT;
    template <typename InputIt>
    string(InputIt first, InputIt last) FL_NOEXCEPT
        : string_n<FASTLED_STR_INLINED_SIZE>(first, last) {}
    template <int N>
    string(const char (&str)[N]) FL_NOEXCEPT
        : string_n<FASTLED_STR_INLINED_SIZE>(str) {}

    // ======= ASSIGNMENT =======
    string& operator=(const string& other) FL_NOEXCEPT;
    string& operator=(string&& other) FL_NOEXCEPT;
    string& operator=(const char* str) FL_NOEXCEPT;
    template <int N> string& operator=(const char (&str)[N]) FL_NOEXCEPT {
        assign(str, N - 1);
        return *this;
    }

    string& assign(string_view sv) FL_NOEXCEPT;

    // ======= SUBSTRING (returns string, so must live here) =======
    string substring(fl::size start, fl::size end) const FL_NOEXCEPT;
    string substr(fl::size start, fl::size length) const FL_NOEXCEPT;
    string substr(fl::size start) const FL_NOEXCEPT;
    string trim() const FL_NOEXCEPT;

#ifdef FL_IS_WASM
    string(const std::string& str);  // okay std namespace; defined in string.cpp.hpp
    string& operator=(const std::string& str) FL_NOEXCEPT;  // okay std namespace
    string& append(const std::string& str);  // okay std namespace
#endif

#if FL_STRING_NEEDS_ARDUINO_CONVERSION
    string(const ::String& str);
    string& operator=(const ::String& str) FL_NOEXCEPT;
    string& append(const ::String& str);
#endif



    bool operator>(const string &other) const FL_NOEXCEPT;
    bool operator>=(const string &other) const FL_NOEXCEPT;
    bool operator<(const string &other) const FL_NOEXCEPT;
    bool operator<=(const string &other) const FL_NOEXCEPT;
    bool operator==(const string &other) const FL_NOEXCEPT;
    bool operator!=(const string &other) const FL_NOEXCEPT;

    // ======= CONCATENATION =======
    string& operator+=(const string& other) FL_NOEXCEPT;

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

    string& append(const JsonUiInternal& val) FL_NOEXCEPT;
    string& append(const json_value& val) FL_NOEXCEPT;
    string& append(const json& val) FL_NOEXCEPT;

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

    string& append(const CRGB& c) FL_NOEXCEPT;
    string& appendCRGB(const CRGB& c) FL_NOEXCEPT;

    // Any type with a to_float() method (e.g., fixed_point types)
    template <typename T>
    typename fl::enable_if<
        fl::is_same<decltype(static_cast<const T*>(nullptr)->to_float()) FL_NOEXCEPT , float>::value
        && !fl::is_floating_point<T>::value,
        string&>::type
    append(const T& val) {
        basic_string::append(val.to_float());
        return *this;
    }

    string& append(const audio::fft::Bins& str) FL_NOEXCEPT;
    string& append(const XYMap& map) FL_NOEXCEPT;
    string& append(const Tile2x2_u8_wrap& tile) FL_NOEXCEPT;

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

    void swap(string& other) FL_NOEXCEPT;

    string& intern() FL_NOEXCEPT;

  private:
    enum {
        kStrInlineSize = FASTLED_STR_INLINED_SIZE,
    };

    static void compileTimeAssertions() FL_NOEXCEPT;
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
operator+(const char* lhs, const T& rhs) FL_NOEXCEPT {
    string result(lhs);
    result += rhs;
    return result;
}

template<typename T>
inline typename fl::enable_if<!fl::is_arithmetic<T>::value, string>::type
operator+(const T& lhs, const char* rhs) FL_NOEXCEPT {
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
    bool operator()(const string &a, const string &b) const FL_NOEXCEPT {
        fl::size al = a.size(), bl = b.size();
        if (al != bl) {
            return al < bl;
        }
        return fl::memcmp(a.c_str(), b.c_str(), al) < 0;
    }
};

} // namespace fl

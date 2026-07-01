#pragma once

#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"
#include "fl/gfx/crgb.h"  // IWYU pragma: keep
#include "fl/stl/move.h"  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "fl/stl/ios.h"  // IWYU pragma: keep

// Include cstdio for print function
#include "fl/stl/cstdio.h"
#include "fl/stl/noexcept.h"

namespace fl {

class ostream {
public:
    ostream() = default;

    // Stream output operators that immediately print
    ostream& operator<<(const char* str) FL_NO_EXCEPT {
        if (str) {
            print(str);
        }
        return *this;
    }

    ostream& operator<<(const string& str) FL_NO_EXCEPT {
        print(str.c_str());
        return *this;
    }

    ostream& operator<<(char c) FL_NO_EXCEPT {
        char str[2] = {c, '\0'};
        print(str);
        return *this;
    }

    ostream& operator<<(fl::i8 n) FL_NO_EXCEPT;
    ostream& operator<<(fl::u8 n) FL_NO_EXCEPT;
    ostream& operator<<(fl::i16 n) FL_NO_EXCEPT;
    ostream& operator<<(fl::i32 n) FL_NO_EXCEPT;
    ostream& operator<<(fl::u32 n) FL_NO_EXCEPT;

    ostream& operator<<(float f) FL_NO_EXCEPT {
        string temp;
        temp.append(f);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(double d) FL_NO_EXCEPT {
        string temp;
        temp.append(d);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(const CRGB& rgb) FL_NO_EXCEPT {
        string temp;
        temp.append(rgb);
        print(temp.c_str());
        return *this;
    }

    // Generic integer handler using SFINAE - handles all multi-byte integer types
    // (including unsigned long on Windows) by casting to the appropriate fl:: type.
    // Mirrors the pattern used by sstream.
    template<typename T>
    typename fl::enable_if<fl::is_multi_byte_integer<T>::value, ostream&>::type
    operator<<(T val) FL_NO_EXCEPT {
        using target_t = typename int_cast_detail::cast_target<T>::type;
        string temp;
        temp.append(static_cast<target_t>(val));
        print(temp.c_str());
        return *this;
    }

    // Get current formatting base (1=decimal, 16=hex, 8=octal)
    int getBase() const FL_NO_EXCEPT { return mBase; }

    // Friend operators for manipulators
    friend ostream& operator<<(ostream&, const hex_t&) FL_NO_EXCEPT;
    friend ostream& operator<<(ostream&, const dec_t&) FL_NO_EXCEPT;
    friend ostream& operator<<(ostream&, const oct_t&) FL_NO_EXCEPT;

private:
    int mBase = 10;  // Default to decimal
};

// Global cout instance for immediate output.
// `cout` is a public API global — users write `fl::cout << x;` by name.
// Migration to `Singleton<ostream>::instance()` would break source
// compat (`cout()` at call sites). Kept as a global; allowlist marker
// on the definition site in ostream.cpp.hpp.
extern ostream cout;

// Line ending manipulator — empty marker struct, public API surface
// (`cout << endl`), same source-compat constraint as the other
// manipulators. Definition in ostream.cpp.hpp is allowlisted.
struct endl_t {};
extern const endl_t endl;

// endl manipulator implementation
inline ostream& operator<<(ostream& os, const endl_t&) FL_NO_EXCEPT {
    os << "\n";
    return os;
}

// hex, dec, oct manipulator implementations
// (declared as friend functions in ostream class, implemented in ostream.cpp)
ostream& operator<<(ostream& os, const hex_t&) FL_NO_EXCEPT;
ostream& operator<<(ostream& os, const dec_t&) FL_NO_EXCEPT;
ostream& operator<<(ostream& os, const oct_t&) FL_NO_EXCEPT;

} // namespace fl

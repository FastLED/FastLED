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
    ostream& operator<<(const char* str) FL_NOEXCEPT {
        if (str) {
            print(str);
        }
        return *this;
    }

    ostream& operator<<(const string& str) FL_NOEXCEPT {
        print(str.c_str());
        return *this;
    }

    ostream& operator<<(char c) FL_NOEXCEPT {
        char str[2] = {c, '\0'};
        print(str);
        return *this;
    }

    ostream& operator<<(fl::i8 n) FL_NOEXCEPT;
    ostream& operator<<(fl::u8 n) FL_NOEXCEPT;
    ostream& operator<<(fl::i16 n) FL_NOEXCEPT;
    ostream& operator<<(fl::i32 n) FL_NOEXCEPT;
    ostream& operator<<(fl::u32 n) FL_NOEXCEPT;

    ostream& operator<<(float f) FL_NOEXCEPT {
        string temp;
        temp.append(f);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(double d) FL_NOEXCEPT {
        string temp;
        temp.append(d);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(const CRGB& rgb) FL_NOEXCEPT {
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
    operator<<(T val) FL_NOEXCEPT {
        using target_t = typename int_cast_detail::cast_target<T>::type;
        string temp;
        temp.append(static_cast<target_t>(val));
        print(temp.c_str());
        return *this;
    }

    // Get current formatting base (1=decimal, 16=hex, 8=octal)
    int getBase() const FL_NOEXCEPT { return mBase; }

    // Friend operators for manipulators
    friend ostream& operator<<(ostream&, const hex_t&) FL_NOEXCEPT;
    friend ostream& operator<<(ostream&, const dec_t&) FL_NOEXCEPT;
    friend ostream& operator<<(ostream&, const oct_t&) FL_NOEXCEPT;

private:
    int mBase = 10;  // Default to decimal
};

// Global cout instance for immediate output
extern ostream cout;

// Line ending manipulator
struct endl_t {};
extern const endl_t endl;

// endl manipulator implementation
inline ostream& operator<<(ostream& os, const endl_t&) FL_NOEXCEPT {
    os << "\n";
    return os;
}

// hex, dec, oct manipulator implementations
// (declared as friend functions in ostream class, implemented in ostream.cpp)
ostream& operator<<(ostream& os, const hex_t&) FL_NOEXCEPT;
ostream& operator<<(ostream& os, const dec_t&) FL_NOEXCEPT;
ostream& operator<<(ostream& os, const oct_t&) FL_NOEXCEPT;

} // namespace fl

#pragma once

// allow-include-after-namespace

// Forward declaration to avoid pulling in fl/io.h and causing fl/io.cpp to be compiled
#ifndef FL_IO_H_INCLUDED
namespace fl {
    void print(const char* str);
}
#endif

#include "fl/str.h"
#include "fl/int.h"
#include "fl/type_traits.h"
#include "crgb.h"

namespace fl {

class ostream {
public:
    ostream() = default;

    // Stream output operators that immediately print
    ostream& operator<<(const char* str) {
        if (str) {
            print(str);
        }
        return *this;
    }

    ostream& operator<<(const string& str) {
        print(str.c_str());
        return *this;
    }

    ostream& operator<<(char c) {
        char str[2] = {c, '\0'};
        print(str);
        return *this;
    }

    ostream& operator<<(fl::i8 n) {
        string temp;
        temp.append(fl::i16(n));
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(fl::u8 n) {
        string temp;
        temp.append(fl::u16(n));
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(fl::i16 n) {
        string temp;
        temp.append(n);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(fl::i32 n) {
        string temp;
        temp.append(n);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(fl::u32 n) {
        string temp;
        temp.append(n);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(float f) {
        string temp;
        temp.append(f);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(double d) {
        string temp;
        temp.append(d);
        print(temp.c_str());
        return *this;
    }

    ostream& operator<<(const CRGB& rgb) {
        string temp;
        temp.append(rgb);
        print(temp.c_str());
        return *this;
    }

    // Unified handler for fl:: namespace size-like unsigned integer types to avoid conflicts
    // This handles fl::size and fl::u16 from the fl:: namespace only
    template<typename T>
    typename fl::enable_if<
        fl::is_same<T, fl::size>::value ||
        fl::is_same<T, fl::u16>::value,
        ostream&
    >::type operator<<(T n) {
        string temp;
        temp.append(fl::u32(n));
        print(temp.c_str());
        return *this;
    }

    // Generic template for other types that have string append support
    // Note: This must come after the specific SFINAE template to avoid conflicts
    template<typename T>
    typename fl::enable_if<
        !fl::is_same<T, fl::size>::value &&
        !fl::is_same<T, fl::u16>::value,
        ostream&
    >::type operator<<(const T& value) {
        string temp;
        temp.append(value);
        print(temp.c_str());
        return *this;
    }
};

// Global cout instance for immediate output
extern ostream cout;

// Line ending manipulator
struct endl_t {};
extern const endl_t endl;

// endl manipulator implementation
inline ostream& operator<<(ostream& os, const endl_t&) {
    os << "\n";
    return os;
}

} // namespace fl

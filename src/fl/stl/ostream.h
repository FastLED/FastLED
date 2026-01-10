#pragma once

#include "fl/int.h"
#include "fl/stl/type_traits.h"
#include "fl/rgb8.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "fl/stl/ios.h"

// Forward declaration to avoid pulling in fl/stl/cstdio.h and causing fl/stl/cstdio.cpp to be compiled
#ifndef FTL_CSTDIO_H_INCLUDED
namespace fl {
    void print(const char* str);
}
#endif

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

    ostream& operator<<(fl::i8 n);
    ostream& operator<<(fl::u8 n);
    ostream& operator<<(fl::i16 n);
    ostream& operator<<(fl::i32 n);
    ostream& operator<<(fl::u32 n);

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

    // Get current formatting base (1=decimal, 16=hex, 8=octal)
    int getBase() const { return mBase; }

    // Friend operators for manipulators
    friend ostream& operator<<(ostream&, const hex_t&);
    friend ostream& operator<<(ostream&, const dec_t&);
    friend ostream& operator<<(ostream&, const oct_t&);

private:
    int mBase = 10;  // Default to decimal
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

// hex, dec, oct manipulator implementations
// (declared as friend functions in ostream class, implemented in ostream.cpp)
ostream& operator<<(ostream& os, const hex_t&);
ostream& operator<<(ostream& os, const dec_t&);
ostream& operator<<(ostream& os, const oct_t&);

} // namespace fl

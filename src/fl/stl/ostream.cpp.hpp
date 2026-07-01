#include "fl/stl/ostream.h"
#include "fl/stl/charconv.h"

namespace fl {

// Global cout instance for immediate output.
// FL_LINT_ALLOW_GLOBAL(public API surface — `fl::cout << x;` is called by name, cannot become `cout()` without a source-breaking API change. Migration deferred; see FastLED#3481 discussion of streams.)
ostream cout;

// FL_LINT_ALLOW_GLOBAL(stream manipulator tag `endl` — public API surface, empty struct, matches `hex`/`dec`/`oct` in ios.cpp.hpp)
const endl_t endl;

// Numeric output operators with formatting support
ostream& ostream::operator<<(fl::i8 n) {
    char buf[64] = {0};
    fl::itoa(static_cast<fl::i32>(n), buf, mBase);
    print(buf);
    return *this;
}

ostream& ostream::operator<<(fl::u8 n) {
    char buf[64] = {0};
    fl::utoa32(static_cast<fl::u32>(n), buf, mBase);
    print(buf);
    return *this;
}

ostream& ostream::operator<<(fl::i16 n) {
    char buf[64] = {0};
    fl::itoa(static_cast<fl::i32>(n), buf, mBase);
    print(buf);
    return *this;
}

ostream& ostream::operator<<(fl::i32 n) {
    char buf[64] = {0};
    fl::itoa(n, buf, mBase);
    print(buf);
    return *this;
}

ostream& ostream::operator<<(fl::u32 n) {
    char buf[64] = {0};
    fl::utoa32(n, buf, mBase);
    print(buf);
    return *this;
}

// Manipulator operator implementations (declared as friends in ostream)
ostream& operator<<(ostream& os, const hex_t&) {
    os.mBase = 16;
    return os;
}

ostream& operator<<(ostream& os, const dec_t&) {
    os.mBase = 10;
    return os;
}

ostream& operator<<(ostream& os, const oct_t&) {
    os.mBase = 8;
    return os;
}

} // namespace fl

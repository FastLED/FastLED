#include "fl/stl/strstream.h"
#include "crgb.h"
#include "fl/stl/string.h"
#include "fl/stl/ios.h"
#include "fl/stl/charconv.h"

namespace fl {


// Manipulator operator implementations (declared as friends in sstream)
sstream& operator<<(sstream& ss, const hex_t&) {
    ss.mBase = 16;
    return ss;
}

sstream& operator<<(sstream& ss, const dec_t&) {
    ss.mBase = 10;
    return ss;
}

sstream& operator<<(sstream& ss, const oct_t&) {
    ss.mBase = 8;
    return ss;
}

// Helper method implementations for formatted integer output
void sstream::appendFormatted(fl::i8 val) {
    appendFormatted(fl::i16(val));
}

void sstream::appendFormatted(fl::i16 val) {
    appendFormatted(fl::i32(val));
}

void sstream::appendFormatted(fl::i32 val) {
    char buf[64] = {0};
    int len = fl::itoa(val, buf, mBase);
    mStr.append(buf, len);
}

void sstream::appendFormatted(fl::i64 val) {
    char buf[64] = {0};
    int len;
    if (mBase == 16 || mBase == 8) {
        // For hex/oct, treat as unsigned bit pattern
        len = fl::utoa64(static_cast<u64>(val), buf, mBase);
    } else {
        // For decimal, handle negative sign manually
        if (val < 0) {
            mStr.append("-", 1);
            len = fl::utoa64(u64(0) - static_cast<u64>(val), buf, mBase);
        } else {
            len = fl::utoa64(static_cast<u64>(val), buf, mBase);
        }
    }
    mStr.append(buf, len);
}

void sstream::appendFormatted(fl::u16 val) {
    appendFormatted(fl::u32(val));
}

void sstream::appendFormatted(fl::u32 val) {
    char buf[64] = {0};
    int len = fl::utoa32(val, buf, mBase);
    mStr.append(buf, len);
}

void sstream::appendFormatted(fl::u64 val) {
    char buf[64] = {0};
    int len = fl::utoa64(val, buf, mBase);
    mStr.append(buf, len);
}

} // namespace fl

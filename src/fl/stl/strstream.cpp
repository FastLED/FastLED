#include "fl/stl/strstream.h"
#include "crgb.h"
#include "fl/tile2x2.h"
#include "fl/fft.h"
#include "fl/str.h"
#include "fl/stl/ios.h"
#include "fl/stl/charconv.h"

namespace fl {


StrStream &StrStream::operator<<(const Tile2x2_u8 &subpixel) {
    mStr.append("Tile2x2_u8(");
    mStr.append(subpixel.bounds());
    mStr.append(" => ");
    mStr.append(subpixel.at(0, 0));
    mStr.append(",");
    mStr.append(subpixel.at(0, 1));
    mStr.append(",");
    mStr.append(subpixel.at(1, 0));
    mStr.append(",");
    mStr.append(subpixel.at(1, 1));
    mStr.append(")");
    return *this;
}

// FFTBins support - show both raw and db bins  
StrStream &StrStream::operator<<(const FFTBins &bins) {
    mStr.append("FFTBins(size=");
    mStr.append(bins.size());
    mStr.append(", raw=");
    (*this) << bins.bins_raw;
    mStr.append(", db=");
    (*this) << bins.bins_db;
    mStr.append(")");
    return *this;
}

// Tile2x2_u8_wrap support - delegates to fl::string::append which already knows how to format it
StrStream &StrStream::operator<<(const Tile2x2_u8_wrap &tile) {
    mStr.append(tile);
    return *this;
}

// Manipulator operator implementations (declared as friends in StrStream)
StrStream& operator<<(StrStream& ss, const hex_t&) {
    ss.mBase = 16;
    return ss;
}

StrStream& operator<<(StrStream& ss, const dec_t&) {
    ss.mBase = 10;
    return ss;
}

StrStream& operator<<(StrStream& ss, const oct_t&) {
    ss.mBase = 8;
    return ss;
}

// Helper method implementations for formatted integer output
void StrStream::appendFormatted(fl::i8 val) {
    appendFormatted(fl::i16(val));
}

void StrStream::appendFormatted(fl::i16 val) {
    appendFormatted(fl::i32(val));
}

void StrStream::appendFormatted(fl::i32 val) {
    char buf[64] = {0};
    int len = fl::itoa(val, buf, mBase);
    mStr.append(buf, len);
}

void StrStream::appendFormatted(fl::i64 val) {
    char buf[64] = {0};
    int len;
    if (mBase == 16 || mBase == 8) {
        // For hex/oct, treat as unsigned bit pattern
        len = fl::utoa64(static_cast<uint64_t>(val), buf, mBase);
    } else {
        // For decimal, handle negative sign manually
        if (val < 0) {
            mStr.append("-", 1);
            len = fl::utoa64(static_cast<uint64_t>(-val), buf, mBase);
        } else {
            len = fl::utoa64(static_cast<uint64_t>(val), buf, mBase);
        }
    }
    mStr.append(buf, len);
}

void StrStream::appendFormatted(fl::u16 val) {
    appendFormatted(fl::u32(val));
}

void StrStream::appendFormatted(fl::u32 val) {
    char buf[64] = {0};
    int len = fl::utoa32(val, buf, mBase);
    mStr.append(buf, len);
}

void StrStream::appendFormatted(fl::u64 val) {
    char buf[64] = {0};
    int len = fl::utoa64(val, buf, mBase);
    mStr.append(buf, len);
}

} // namespace fl

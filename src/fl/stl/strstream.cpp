#include "fl/stl/strstream.h"
#include "crgb.h"
#include "fl/tile2x2.h"
#include "fl/fft.h"
#include "fl/str.h"
#include "fl/stl/ios.h"

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
    fl::StrN<FASTLED_STR_INLINED_SIZE> temp;
    if (mBase == 16) {
        fl::StringFormatter::appendHex(val, &temp);
    } else if (mBase == 8) {
        fl::StringFormatter::appendOct(val, &temp);
    } else {
        fl::StringFormatter::append(val, &temp);
    }
    mStr.append(temp.c_str(), temp.size());
}

void StrStream::appendFormatted(fl::i64 val) {
    fl::StrN<FASTLED_STR_INLINED_SIZE> temp;
    if (mBase == 16) {
        fl::StringFormatter::appendHex(static_cast<uint64_t>(val), &temp);
    } else if (mBase == 8) {
        fl::StringFormatter::appendOct(static_cast<uint64_t>(val), &temp);
    } else {
        fl::StringFormatter::append(static_cast<uint64_t>(val), &temp);
    }
    mStr.append(temp.c_str(), temp.size());
}

void StrStream::appendFormatted(fl::u16 val) {
    appendFormatted(fl::u32(val));
}

void StrStream::appendFormatted(fl::u32 val) {
    fl::StrN<FASTLED_STR_INLINED_SIZE> temp;
    if (mBase == 16) {
        fl::StringFormatter::appendHex(val, &temp);
    } else if (mBase == 8) {
        fl::StringFormatter::appendOct(val, &temp);
    } else {
        fl::StringFormatter::append(val, &temp);
    }
    mStr.append(temp.c_str(), temp.size());
}

void StrStream::appendFormatted(fl::u64 val) {
    fl::StrN<FASTLED_STR_INLINED_SIZE> temp;
    if (mBase == 16) {
        fl::StringFormatter::appendHex(val, &temp);
    } else if (mBase == 8) {
        fl::StringFormatter::appendOct(val, &temp);
    } else {
        fl::StringFormatter::append(val, &temp);
    }
    mStr.append(temp.c_str(), temp.size());
}

} // namespace fl

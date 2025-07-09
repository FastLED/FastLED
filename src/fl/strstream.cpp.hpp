#include "fl/strstream.h"
#include "crgb.h"
#include "fl/tile2x2.h"
#include "fl/fft.h"
#include "str.h"

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

} // namespace fl

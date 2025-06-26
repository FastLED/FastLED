
#include "fl/strstream.h"
#include "crgb.h"
#include "fl/tile2x2.h"
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

} // namespace fl

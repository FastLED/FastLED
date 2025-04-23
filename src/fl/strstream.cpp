#pragma once

#include "crgb.h"
#include "str.h"
#include "fl/strstream.h"
#include "fl/tile2x2.h"


namespace fl {

StrStream& StrStream::operator<<(const Tile2x2& subpixel) {
    mStr.append("Tile2x2(");
    mStr.append(subpixel.bounds());
    mStr.append(")");
    return *this;
}

} // namespace fl

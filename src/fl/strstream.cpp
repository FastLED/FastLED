#pragma once

#include "crgb.h"
#include "str.h"
#include "fl/strstream.h"
#include "fl/subpixel.h"


namespace fl {

StrStream& StrStream::operator<<(const SubPixel2x2& subpixel) {
    mStr.append("SubPixel2x2(");
    mStr.append(subpixel.bounds());
    mStr.append(")");
    return *this;
}

} // namespace fl

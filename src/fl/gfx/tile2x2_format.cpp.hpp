// IWYU pragma: private
// ok no header - public declarations remain in fl/stl/string.h and fl/stl/strstream.h.

/// @brief Tile formatting, linked with graphics utilities.

#include "fl/gfx/tile2x2.h"
#include "fl/math/geometry.h"
#include "fl/stl/strstream.h"
#include "fl/stl/string.h"

namespace fl {

sstream &sstream::operator<<(const Tile2x2_u8 &subpixel) FL_NO_EXCEPT {
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

// Tile2x2_u8_wrap support - delegates to fl::string::append which already knows how to format it
sstream &sstream::operator<<(const Tile2x2_u8_wrap &tile) FL_NO_EXCEPT {
    mStr.append(tile);
    return *this;
}

string &string::append(const Tile2x2_u8_wrap &tile) FL_NO_EXCEPT {
    Tile2x2_u8_wrap::Entry data[4] = {
        tile.at(0, 0),
        tile.at(0, 1),
        tile.at(1, 0),
        tile.at(1, 1),
    };

    append("Tile2x2_u8_wrap(");
    for (int i = 0; i < 4; i++) {
        vec2<u16> pos = data[i].first;
        u8 alpha = data[i].second;
        append("(");
        append(pos.x);
        append(",");
        append(pos.y);
        append(",");
        append(alpha);
        append(")");
        if (i < 3) {
            append(",");
        }
    }
    append(")");
    return *this;
}

} // namespace fl

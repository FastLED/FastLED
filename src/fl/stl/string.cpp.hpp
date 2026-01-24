#include "fl/stl/string.h"
#include "fl/stl/stdio.h"   // For fl::snprintf
#include "fl/stl/cstring.h" // For fl::memcpy, fl::strcmp

#include "crgb.h"
#include "fl/fft.h"
#include "fl/unused.h"
#include "fl/xymap.h"
#include "fl/json.h"
#include "fl/tile2x2.h"
#include "fl/compiler_control.h"
// UI dependency moved to separate compilation unit to break dependency chain

namespace fl {

// Define static constexpr member for npos (required for ODR-uses)
const fl::size string::npos;

// Explicit template instantiations for commonly used sizes
template class StrN<64>;

int string::strcmp(const string& a, const string& b) {
    return fl::strcmp(a.c_str(), b.c_str());
}

string &string::append(const FFTBins &str) {
    append("\n FFTImpl Bins:\n  ");
    append(str.bins_raw);
    append("\n");
    append(" FFTImpl Bins DB:\n  ");
    append(str.bins_db);
    append("\n");
    return *this;
}

string &string::append(const XYMap &map) {
    append("XYMap(");
    append(map.getWidth());
    append(",");
    append(map.getHeight());
    append(")");
    return *this;
}

string &string::append(const Tile2x2_u8_wrap &tile) {
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

void string::swap(string &other) {
    if (this != &other) {
        fl::swap(mLength, other.mLength);
        fl::swap(mStorage, other.mStorage);
    }
}

void string::compileTimeAssertions() {
    static_assert(FASTLED_STR_INLINED_SIZE > 0,
                  "FASTLED_STR_INLINED_SIZE must be greater than 0");
    static_assert(FASTLED_STR_INLINED_SIZE == kStrInlineSize,
                  "If you want to change the FASTLED_STR_INLINED_SIZE, then it "
                  "must be through a build define and not an include define.");
}

string &string::append(const CRGB &rgb) {
    append("CRGB(");
    append(rgb.r);
    append(",");
    append(rgb.g);
    append(",");
    append(rgb.b);
    append(")");
    return *this;
}

string &string::appendCRGB(const CRGB &rgb) {
    append("CRGB(");
    append(rgb.r);
    append(",");
    append(rgb.g);
    append(",");
    append(rgb.b);
    append(")");
    return *this;
}

// JsonUiInternal append implementation moved to str_ui.cpp to break dependency chain

// JSON type append implementations
// NOTE: These use forward declarations to avoid circular dependency with json.h
string &string::append(const JsonValue& val) {
    // Use the JsonValue's to_string method if available
    // For now, just append a placeholder to avoid compilation errors
    FL_UNUSED(val);
    append("<JsonValue>");
    return *this;
}

string &string::append(const Json& val) {
    // Use the Json's to_string method if available
    // For now, just append a placeholder to avoid compilation errors
    //append("<Json>");
    append("Json(");
    append(val.to_string());
    append(")");
    return *this;
}

} // namespace fl

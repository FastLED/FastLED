#include "fl/stl/string.h"
#include "fl/stl/cstring.h" // For fl::memcpy, fl::strcmp
#include "fl/stl/string_interner.h" // For global_interner()
#include "fl/stl/type_traits.h"      // for swap
#include "fl/stl/variant.h"          // for variant

#include "fl/audio/fft/fft.h"
#include "fl/math/geometry.h"             // for vec2
#include "fl/stl/int.h"                  // for size, u16, u8
#include "fl/stl/json.h"
#include "fl/gfx/crgb.h"                 // for CRGB
#include "fl/gfx/tile2x2.h"
#include "fl/stl/compiler_control.h"
#include "fl/math/xymap.h"
// UI dependency moved to separate compilation unit to break dependency chain

#if FL_STRING_NEEDS_ARDUINO_CONVERSION
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep  // ok header
#endif

namespace fl {

// Define static constexpr member for npos (required for ODR-uses)
const fl::size string::npos;

int string::strcmp(const string& a, const string& b) {
    return fl::strcmp(a.c_str(), b.c_str());
}

string &string::append(const audio::fft::Bins &str) {
    append("\n Impl Bins:\n  ");
    append(str.raw());
    append("\n");
    append(" Impl Bins DB:\n  ");
    append(str.db());
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
    if (this == &other) return;
    string tmp(fl::move(*this));
    *this = fl::move(other);
    other = fl::move(tmp);
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
string &string::append(const json_value& val) {
    // Use the json_value's to_string method if available
    // For now, just append a placeholder to avoid compilation errors
    FL_UNUSED(val);
    append("<json_value>");
    return *this;
}

string &string::append(const json& val) {
    // Use the json's to_string method if available
    // For now, just append a placeholder to avoid compilation errors
    //append("<json>");
    append("json(");
    append(val.to_string());
    append(")");
    return *this;
}

#if FL_STRING_NEEDS_ARDUINO_CONVERSION
string::string(const ::String &str) {
    copy(str.c_str(), strlen(str.c_str()));
}

string &string::operator=(const ::String &str) FL_NOEXCEPT {
    copy(str.c_str(), strlen(str.c_str()));
    return *this;
}

string &string::append(const ::String &str) {
    write(str.c_str(), strlen(str.c_str()));
    return *this;
}
#endif

// String interning method implementation
string& string::intern() {
    // Skip interning if using inline storage (SSO) - already efficient, no heap allocation
    if (isInline()) {
        return *this;
    }

    // Intern via global interner - replaces this string with deduplicated version
    *this = global_interner().intern(*this);
    return *this;
}

} // namespace fl

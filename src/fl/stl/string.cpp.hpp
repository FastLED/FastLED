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
#include "fl/stl/static_assert.h"
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

// ======= STATIC FACTORY METHODS =======

string string::from_literal(const char* literal) FL_NOEXCEPT {
    string result;
    result.setLiteral(literal);
    return result;
}

string string::from_view(const char* data, fl::size len) FL_NOEXCEPT {
    string result;
    result.setView(data, len);
    return result;
}

string string::from_view(const string_view& sv) FL_NOEXCEPT {
    return from_view(sv.data(), sv.size());
}

string string::interned(const char* str, fl::size len) FL_NOEXCEPT {
    if (!str || len == 0) return string();
    // Route through the global interner so identical content
    // returns the same shared StringHolder (O(1) average lookup,
    // matches `string::intern()`'s semantics). Previously this
    // family wrapped a fresh StringHolder per call with no
    // deduplication — see #2961 CR thread + the follow-on commit.
    return global_interner().intern(fl::string_view(str, len));
}

string string::interned(const char* str) FL_NOEXCEPT {
    if (!str) return string();
    return global_interner().intern(str);
}

string string::interned(const string_view& sv) FL_NOEXCEPT {
    return global_interner().intern(sv);
}

string string::copy_no_view(const string& str) FL_NOEXCEPT {
    if (str.is_referencing()) {
        string result;
        result.copy(str.c_str(), str.size());
        return result;
    }
    return str;
}

int string::strcmp(const string& a, const string& b) FL_NOEXCEPT {
    return fl::strcmp(a.c_str(), b.c_str());
}

// ======= CONSTRUCTORS =======
// All forward to string_n<FASTLED_STR_INLINED_SIZE>'s ctors which
// own the actual initialisation logic. fl::string is just the
// composite-formatter + factory layer on top.

string::string() FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>() {}

string::string(const char* str) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(str) {}

string::string(const char* str, fl::size len) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(str, len) {}

string::string(fl::size len, char c) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(len, c) {}

string::string(const string& other) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(static_cast<const string_n<FASTLED_STR_INLINED_SIZE>&>(other)) {}

string::string(string&& other) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(static_cast<string_n<FASTLED_STR_INLINED_SIZE>&&>(other)) {}

string::string(const basic_string& other) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(other) {}

string::string(const string_view& sv) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(sv) {}

string::string(const fl::span<const char>& s) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(s) {}

string::string(const fl::span<char>& s) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(s) {}

string::string(const fl::shared_ptr<StringHolder>& holder) FL_NOEXCEPT
    : string_n<FASTLED_STR_INLINED_SIZE>(holder) {}

// ======= ASSIGNMENT =======

string& string::operator=(const string& other) FL_NOEXCEPT {
    copy(static_cast<const basic_string&>(other));
    return *this;
}

string& string::operator=(string&& other) FL_NOEXCEPT {
    moveAssign(fl::move(other));
    return *this;
}

string& string::operator=(const char* str) FL_NOEXCEPT {
    // Match the string(const char*) ctor's contract: nullptr → empty.
    if (str) {
        copy(str, fl::strlen(str));
    } else {
        clear();
    }
    return *this;
}

string& string::assign(string_view sv) FL_NOEXCEPT {
    if (sv.empty()) {
        clear();
    } else {
        copy(sv.data(), sv.size());
    }
    return *this;
}

// ======= SUBSTRING =======

string string::substring(fl::size start, fl::size end) const FL_NOEXCEPT {
    if (start == 0 && end == size()) return *this;
    if (start >= size()) return string();
    if (end > size()) end = size();
    if (start >= end) return string();
    string out;
    out.copy(c_str() + start, end - start);
    return out;
}

string string::substr(fl::size start, fl::size length) const FL_NOEXCEPT {
    // Handle `npos` / overflow: when `length == npos` the caller
    // means "to end of string", and when `length` is large enough
    // that `start + length` would wrap, we clamp to the end before
    // the addition can overflow.
    fl::size end;
    if (length == npos || length > size() - (start < size() ? start : size())) {
        end = size();
    } else {
        end = start + length;
        if (end > size()) end = size();
    }
    return substring(start, end);
}

string string::substr(fl::size start) const FL_NOEXCEPT {
    return substring(start, size());
}

string string::trim() const FL_NOEXCEPT {
    fl::size start = 0;
    fl::size end_pos = size();
    while (start < size() && fl::isspace(c_str()[start])) start++;
    while (end_pos > start && fl::isspace(c_str()[end_pos - 1])) end_pos--;
    return substring(start, end_pos);
}

#ifdef FL_IS_WASM
string::string(const std::string& str)  // okay std namespace
    : string_n<FASTLED_STR_INLINED_SIZE>() {
    copy(str.c_str(), str.size());
}

string& string::operator=(const std::string& str) FL_NOEXCEPT {  // okay std namespace
    copy(str.c_str(), str.size());
    return *this;
}

string& string::append(const std::string& str) {  // okay std namespace
    write(str.c_str(), str.size());
    return *this;
}
#endif

// ======= COMPARISON OPERATORS =======

// Length-aware lexicographic compare. Doesn't rely on strcmp's
// NUL-termination, so view-backed strings (`setView`) and any
// future embedded-NUL content compare correctly. Returns the same
// sign convention as memcmp / strcmp.
static inline int string_compare(const string& a, const string& b) FL_NOEXCEPT {
    fl::size n = (a.size() < b.size()) ? a.size() : b.size();
    int r = fl::memcmp(a.c_str(), b.c_str(), n);
    if (r != 0) return r;
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    return 0;
}

bool string::operator>(const string& other) const FL_NOEXCEPT {
    return string_compare(*this, other) > 0;
}

bool string::operator>=(const string& other) const FL_NOEXCEPT {
    return string_compare(*this, other) >= 0;
}

bool string::operator<(const string& other) const FL_NOEXCEPT {
    return string_compare(*this, other) < 0;
}

bool string::operator<=(const string& other) const FL_NOEXCEPT {
    return string_compare(*this, other) <= 0;
}

bool string::operator==(const string& other) const FL_NOEXCEPT {
    if (size() != other.size()) {
        return false;
    }
    return fl::memcmp(c_str(), other.c_str(), size()) == 0;
}

bool string::operator!=(const string& other) const FL_NOEXCEPT {
    return !(*this == other);
}

// ======= CONCATENATION =======

string& string::operator+=(const string& other) FL_NOEXCEPT {
    append(other.c_str(), other.size());
    return *this;
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
    FL_STATIC_ASSERT(FASTLED_STR_INLINED_SIZE > 0,
                  "FASTLED_STR_INLINED_SIZE must be greater than 0");
    FL_STATIC_ASSERT(FASTLED_STR_INLINED_SIZE == kStrInlineSize,
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
string::string(const ::String &str)
    : string_n<FASTLED_STR_INLINED_SIZE>() {
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

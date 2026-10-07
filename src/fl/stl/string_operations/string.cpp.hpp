// ok no header - public declarations remain in fl/stl/string.h

#include "fl/stl/string.h"
#include "fl/stl/cstring.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/compiler_control.h"
#include "fl/gfx/crgb.h"

namespace fl {

string string::from_view(const char* data, fl::size len) FL_NO_EXCEPT {
    string result;
    result.setView(data, len);
    return result;
}

string string::from_view(const string_view& sv) FL_NO_EXCEPT {
    return from_view(sv.data(), sv.size());
}

string string::copy_no_view(const string& str) FL_NO_EXCEPT {
    if (str.is_referencing()) {
        string result;
        result.copy(str.c_str(), str.size());
        return result;
    }
    return str;
}

int string::strcmp(const string& a, const string& b) FL_NO_EXCEPT {
    return fl::strcmp(a.c_str(), b.c_str());
}

string& string::assign(string_view sv) FL_NO_EXCEPT {
    if (sv.empty()) {
        clear();
    } else {
        copy(sv.data(), sv.size());
    }
    return *this;
}

string string::substring(fl::size start, fl::size end) const FL_NO_EXCEPT {
    if (start == 0 && end == size()) return *this;
    if (start >= size()) return string();
    if (end > size()) end = size();
    if (start >= end) return string();
    string out;
    out.copy(c_str() + start, end - start);
    return out;
}

string string::substr(fl::size start, fl::size length) const FL_NO_EXCEPT {
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

string string::substr(fl::size start) const FL_NO_EXCEPT {
    return substring(start, size());
}

string string::trim() const FL_NO_EXCEPT {
    fl::size start = 0;
    fl::size end_pos = size();
    while (start < size() && fl::isspace(c_str()[start])) start++;
    while (end_pos > start && fl::isspace(c_str()[end_pos - 1])) end_pos--;
    return substring(start, end_pos);
}

void string::swap(string &other) {
    if (this == &other) return;
    string tmp(fl::move(*this));
    *this = fl::move(other);
    other = fl::move(tmp);
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

string &string::append(const json_value& val) {
    // Use the json_value's to_string method if available
    // For now, just append a placeholder to avoid compilation errors
    FL_UNUSED(val);
    append("<json_value>");
    return *this;
}

} // namespace fl

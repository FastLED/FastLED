// ok no header - string is declared in fl/stl/string.h
// IWYU pragma: private

#include "fl/stl/string.h"
#include "fl/stl/json.h"
#include "fl/stl/noexcept.h"

namespace fl {

string &string::append(const json& val) FL_NO_EXCEPT {
    // Use the json's to_string method if available
    // For now, just append a placeholder to avoid compilation errors
    //append("<json>");
    append("json(");
    append(val.to_string());
    append(")");
    return *this;
}

} // namespace fl

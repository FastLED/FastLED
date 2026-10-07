// IWYU pragma: private
// ok no header - public declaration remains in fl/stl/string.h.

#include "fl/math/xymap.h"
#include "fl/stl/string.h"

namespace fl {

string &string::append(const XYMap &map) FL_NO_EXCEPT {
    append("XYMap(");
    append(map.getWidth());
    append(",");
    append(map.getHeight());
    append(")");
    return *this;
}

} // namespace fl

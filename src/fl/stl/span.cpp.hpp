/// @file span.cpp.hpp
/// @brief Runtime diagnostics for fl::span.

#include "fl/stl/span.h"
#include "fl/log/log.h"

namespace fl {
namespace detail {

void span_invalid_pointer_range() FL_NO_EXCEPT {
    FL_ERROR_F("fl::span pointer range has end before begin");
}

} // namespace detail
} // namespace fl

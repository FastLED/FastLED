#pragma once
#include "fl/stl/function.h"
#include "fl/log/log.h"

#if FL_PLATFORM_HAS_LARGE_MEMORY
namespace fl {
namespace detail {
void warnFunctionListAllocationFailure() FL_NO_EXCEPT {
    FL_WARN_ONCE("function_list: State allocation failed; add returns -1");
}
} // namespace detail
} // namespace fl
#endif

#pragma once

/**
## Trace System

The trace system provides source location information for debugging.

### Components:
- `fl::TracePoint`: A tuple of (file, line, timestamp)
- `FL_TRACE`: Macro that captures current file, line, and timestamp

 */

#include "fl/tuple.h"
#include "fl/time.h"
#include "fl/stdint.h"

namespace fl {

/// @brief A structure to hold source trace information.
/// Contains the file name, line number, and the time at which the trace was captured.
using TracePoint = fl::tuple<const char*, int, fl::u32>;

} // namespace fl

/// @brief A macro to capture the current source file, line number, and time.
#define FL_TRACE fl::make_tuple(__FILE__, int(__LINE__), fl::time())

#pragma once

/**
## Trace System

The trace system provides source location information and call stack tracking for debugging.

### Components:
- `fl::TracePoint`: A tuple of (file, line, timestamp)
- `FL_TRACE`: Macro that captures current file, line, and timestamp
- `fl::ScopedTrace`: RAII guard for automatic call stack tracking
- `FL_SCOPED_TRACE(name)`: Convenience macro for stack tracing
- `FL_TRACE_DUMP()`: Print current call stack

### Stack Trace Usage:
```cpp
void criticalFunction() {
    FL_SCOPED_TRACE;  // Automatically uses function name
    // Function body - trace automatically pushed/popped

    if (error) {
        FL_TRACE_DUMP();  // Print call stack for debugging
    }
}

// For custom trace names:
void helper() {
    FL_SCOPED_TRACE_NAMED("custom_trace_name");
    // ...
}
```

### Configuration:
- `FASTLED_DEBUG_STACK_TRACE` - Enable stack tracing (default: disabled)
- Stack depth limit: 32 entries (configurable via FL_STACK_TRACE_MAX_DEPTH)

 */

#include "fl/stl/tuple.h"
#include "fl/stl/time.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/compiler_control.h"  // For FL_FUNCTION

namespace fl {

/// @brief A structure to hold source trace information.
/// Contains the file name, line number, and the time at which the trace was captured.
using TracePoint = fl::tuple<const char*, int, fl::u32>;

/// @brief RAII guard for automatic call stack tracking.
/// Pushes function name on construction, pops on destruction.
/// Non-copyable, non-movable for simplicity.
///
/// This class is always compiled but only functional when FASTLED_DEBUG_STACK_TRACE is defined.
/// The linker can eliminate unused code when trace macros are not used.
class ScopedTrace {
public:
    /// @brief Construct and push function name onto stack
    /// @param function Function name (must be string literal or have static lifetime)
    /// @param line Line number (from __LINE__)
    explicit ScopedTrace(const char* function, int line);

    /// @brief Destructor - automatically pops from stack
    ~ScopedTrace();

    // Non-copyable, non-movable
    ScopedTrace(const ScopedTrace&) = delete;
    ScopedTrace& operator=(const ScopedTrace&) = delete;
    ScopedTrace(ScopedTrace&&) = delete;
    ScopedTrace& operator=(ScopedTrace&&) = delete;

    /// @brief Push a function name onto the call stack
    /// @param function Function name (must be string literal or have static lifetime)
    /// @param line Line number (from __LINE__)
    static void push(const char* function, int line);

    /// @brief Pop the most recent function from the call stack
    static void pop();

    /// @brief Get the current stack depth
    /// @return Number of valid entries in the stack (may exceed storage capacity)
    static fl::size depth();

    /// @brief Dump the current call stack to a string
    /// @return String representation of the stack trace
    static fl::string dump();

    /// @brief Dump the current call stack to an inlined vector of TracePoints
    /// @param out Pointer to vector to receive the stack trace entries
    static void dump(fl::vector<TracePoint>* out);

    /// @brief Clear the entire call stack (primarily for testing)
    static void clear();
};

} // namespace fl

/// @brief A macro to capture the current source file, line number, and time.
#define FL_TRACE fl::make_tuple(__FILE__, int(__LINE__), fl::millis())

#ifdef FASTLED_DEBUG_STACK_TRACE
/// @brief Token pasting helper for FL_SCOPED_TRACE_NAMED - innermost macro
#define FL_SCOPED_TRACE_CONCAT(name, line) fl::ScopedTrace __fl_trace_##line(name, line)

/// @brief Helper macro for FL_SCOPED_TRACE_NAMED to ensure __LINE__ expansion
#define FL_SCOPED_TRACE_IMPL(name, line) FL_SCOPED_TRACE_CONCAT(name, line)

/// @brief Convenience macro for automatic stack tracing via RAII with custom name
/// Creates a ScopedTrace object with unique name per line, capturing line number
/// @param name Custom trace name (must be string literal or have static lifetime)
#define FL_SCOPED_TRACE_NAMED(name) FL_SCOPED_TRACE_IMPL(name, __LINE__)

/// @brief Convenience macro for automatic stack tracing via RAII
/// Automatically uses the current function name via FL_FUNCTION
/// Creates a ScopedTrace object with unique name per line, capturing line number
#define FL_SCOPED_TRACE FL_SCOPED_TRACE_IMPL(FL_FUNCTION, __LINE__)

/// @brief Dump the current call stack to debug output
/// Prints the stack trace using FL_DBG
#define FL_TRACE_DUMP() FL_DBG(fl::ScopedTrace::dump())

#else
// No-op macros when stack tracing is disabled
#define FL_SCOPED_TRACE_NAMED(name) do {} while(0)
#define FL_SCOPED_TRACE do {} while(0)
#define FL_TRACE_DUMP() do {} while(0)
#endif // FASTLED_DEBUG_STACK_TRACE

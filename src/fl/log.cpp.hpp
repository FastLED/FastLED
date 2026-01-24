/// @file log.cpp
/// @brief Logging infrastructure - background flush service and helper functions

#include "fl/log.h"
#include "fl/detail/async_logger.h"

namespace fl {

// ============================================================================
// Debug Output Helpers
// ============================================================================

const char *fastled_file_offset(const char *file) {
    const char *p = file;
    const char *last_slash = nullptr;

    while (*p) {
        // Check for "src/" or "src\\" (handle both Unix and Windows paths)
        if (p[0] == 's' && p[1] == 'r' && p[2] == 'c' && (p[3] == '/' || p[3] == '\\')) {
            return p; // Skip past "src/" or "src\\"
        }
        // Track both forward slash and backslash
        if (*p == '/' || *p == '\\') {
            last_slash = p;
        }
        p++;
    }
    // If "src/" or "src\\" not found but we found at least one slash, return after the
    // last slash
    if (last_slash) {
        return last_slash + 1;
    }
    return file; // If no slashes found at all, return original path
}

// NOTE: AsyncLogger implementation moved to fl/detail/async_logger.cpp
// Background flush infrastructure and async_log_service also moved there

} // namespace fl

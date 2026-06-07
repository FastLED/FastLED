/// @file log.cpp
/// @brief Logging infrastructure - background flush service and helper functions

#include "fl/log/log.h"
#include "fl/log/async_logger.h"
#include "fl/stl/strstream.h"

namespace fl {
namespace detail {

// =============================================================================
// Centralised log emit (#2963 Path C)
// =============================================================================
// One out-of-line copy of the prefix-format chain. FL_WARN / FL_ERROR /
// FL_INFO call sites pay only the cost of constructing
// `fl::sstream() << X` (cheap when X is a literal — see strstream.h's
// Path C literal-storage overload), then trampoline through here.
//
// The legacy macro inlined the entire `<< file << "(" << line <<
// "): WARN: " << X` chain at every call site, emitting per-site code
// and `.rodata` for the prefix format. With this helper:
//
//   * The prefix-format chain exists in exactly one place in the
//     binary, not at each of the ~1,372 FL_WARN / FL_ERROR / FL_INFO
//     call sites.
//   * When the caller passed a single literal (e.g.
//     `FL_WARN("constant message")`), the body sstream stays in
//     literal storage — `body.c_str()` returns the .rodata pointer
//     directly with zero buffer use. This is the FL_WARN_LIT-style
//     fast path applied automatically.
//   * When the caller used a `<< value` chain, the body materialises
//     and the same single helper emits it.
//
// `body.is_pure_literal()` remains a public sstream accessor —
// useful for tests and for future emit backends that might want to
// vector-write (prefix + literal + newline) without composing them
// into one buffer.
void log_emit(log_kind kind, const char* file, int line, const fl::sstream& body) {
    const char* tag;
    switch (kind) {
    case log_kind::WARN:  tag = "): WARN: ";  break;
    case log_kind::ERROR: tag = "): ERROR: "; break;
    case log_kind::INFO:
    default:              tag = "): INFO: ";  break;
    }
    fl::sstream prefixed;
    prefixed << file << "(" << line << tag << body.c_str();
    fl::println(prefixed.c_str());
}

} // namespace detail


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

// NOTE: AsyncLogger implementation moved to fl/log/async_logger.cpp
// Background flush infrastructure and async_log_service also moved there

} // namespace fl

/// @file log.cpp
/// @brief Logging infrastructure - background flush service and helper functions

#include "fl/log/log.h"
#include "fl/log/async_logger.h"
#include "fl/stl/strstream.h"
#include "fl/stl/string.h"
#include "fl/stl/move.h"

namespace fl {
namespace detail {

FL_NO_INLINE const char* log_kind_name(log_kind kind) FL_NO_EXCEPT {
    switch (kind) {
    case log_kind::WARN:  return "WARN";
    case log_kind::ERROR: return "ERROR";
    case log_kind::INFO:  return "INFO";
    }
    return "LOG";
}

// Writes "<file>(<line>): <KIND>: " with plain prints. Deliberately not
// fl::printf: every log site reaches this prefix, so a format string here
// linked the whole printf engine into images whose own logs never format
// anything (~3 KB on the LPC845; FastLED#4671).
FL_NO_INLINE void log_emit_prefix(log_kind kind, const char* file,
                                  int line) FL_NO_EXCEPT {
    char digits[12];  // "-2147483648" + NUL
    char* p = digits + sizeof(digits);
    *--p = '\0';
    unsigned magnitude = line < 0 ? 0u - static_cast<unsigned>(line)
                                  : static_cast<unsigned>(line);
    do {
        *--p = static_cast<char>('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u);
    if (line < 0) {
        *--p = '-';
    }
    fl::print(file);
    fl::print("(");
    fl::print(p);
    fl::print("): ");
    fl::print(log_kind_name(kind));
    fl::print(": ");
}

FL_NO_INLINE void log_emit_newline() FL_NO_EXCEPT { fl::print("\n"); }

// =============================================================================
// Centralised log emit (#2963 Proposal B, Option 3)
// =============================================================================
// Rewrites `body` in place to hold "<file>(<line>): <KIND>: <user>" then
// emits via `fl::println(body.c_str())`. `body` is an rvalue reference
// bound to the FL_WARN/FL_ERROR/FL_INFO temporary in the CALLER's
// stack frame — its lifetime extends to the end of the full
// expression, exactly as the OLD inline macro's temp did. So
// `body.c_str()` here has the same async-handler-safe lifetime as
// before (Path C / Option 1's `prefixed` local broke this by
// introducing an extra stack-frame teardown between println and
// the full-expression end; Option 3 reuses the caller's temp and
// avoids that hazard).
//
// Single out-of-line compilation of the prefix-format chain. Per
// FL_WARN call site, the inlined `<< file << "(" << line << "): KIND: "`
// burst (~30-50 B) collapses to one `log_emit` call (~4 B).

FL_NO_INLINE void log_emit(log_kind kind, const char* file, int line, fl::sstream& body) FL_NO_EXCEPT {
    const char* tag;
    switch (kind) {
    case log_kind::WARN:  tag = "): WARN: ";  break;
    case log_kind::ERROR: tag = "): ERROR: "; break;
    case log_kind::INFO:
    default:              tag = "): INFO: ";  break;
    }
    // Snapshot the user-supplied payload (the only thing `body`
    // contains at entry — the macro fed it only `<< X`).
    fl::string user_payload(body.str());
    // Reset body's buffer and rebuild prefix-then-payload INTO body.
    // body is in the caller's frame; the c_str() pointer we hand to
    // println below lives until the end of the FL_WARN expression.
    body.clear();
    body << file << "(" << line << tag << user_payload.c_str();
    fl::println(body.c_str());
}

FL_NO_INLINE void log_emit_literal(log_kind kind, const char* file, int line,
                                   const char* body) FL_NO_EXCEPT {
    // Prefix + literal + newline, with no formatter involved.
    log_emit_prefix(kind, file, line);
    fl::print(body);
    log_emit_newline();
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

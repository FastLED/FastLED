#pragma once

#include "fl/strstream.h"
#include "fl/sketch_macros.h"
#include "fl/io.h"

namespace fl {
// ".build/src/fl/dbg.h" -> "src/fl/dbg.h"
// "blah/blah/blah.h" -> "blah.h"
inline const char *fastled_file_offset(const char *file) {
    const char *p = file;
    const char *last_slash = nullptr;

    while (*p) {
        if (p[0] == 's' && p[1] == 'r' && p[2] == 'c' && p[3] == '/') {
            return p; // Skip past "src/"
        }
        if (*p == '/') { // fallback to using last slash
            last_slash = p;
        }
        p++;
    }
    // If "src/" not found but we found at least one slash, return after the
    // last slash
    if (last_slash) {
        return last_slash + 1;
    }
    return file; // If no slashes found at all, return original path
}
} // namespace fl

// Debug printing is now always enabled and uses fl::println()
#define FASTLED_HAS_DBG 1
#define _FASTLED_DGB(X)                                                        \
    fl::println(                                                               \
        (fl::StrStream() << (fl::fastled_file_offset(__FILE__))                \
                         << "(" << __LINE__ << "): " << X)                     \
            .c_str())

#define FASTLED_DBG(X) _FASTLED_DGB(X)

#ifndef FASTLED_DBG_IF
#define FASTLED_DBG_IF(COND, MSG)                                              \
    if (COND)                                                                  \
    FASTLED_DBG(MSG)
#endif // FASTLED_DBG_IF

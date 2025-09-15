#pragma once

#include "fl/strstream.h"
#include "fl/sketch_macros.h"
#include "fl/int.h"
#include "fl/stdint.h"

// Forward declaration to avoid pulling in fl/io.h and causing fl/io.cpp to be compiled
// This prevents ~5KB memory bloat for simple applications
#ifndef FL_DBG_PRINTLN_DECLARED
#define FL_DBG_PRINTLN_DECLARED
namespace fl {
    void println(const char* str);
}
#endif

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

#if __EMSCRIPTEN__ || !defined(RELEASE) || defined(FASTLED_TESTING)
#define FASTLED_FORCE_DBG 1
#endif

// Debug printing: Enable only when explicitly requested to avoid ~5KB memory bloat
#if !defined(FASTLED_FORCE_DBG) || !SKETCH_HAS_LOTS_OF_MEMORY
// By default, debug printing is disabled to prevent memory bloat in simple applications
#define FASTLED_HAS_DBG 0
#define _FASTLED_DGB(X) do { if (false) { fl::println(""); } } while(0)  // No-op that handles << operator
#else
// Explicit debug mode enabled - uses fl::println()
#define FASTLED_HAS_DBG 1
#define _FASTLED_DGB(X)                                                        \
    fl::println(                                                               \
        (fl::StrStream() << (fl::fastled_file_offset(__FILE__))                \
                         << "(" << int(__LINE__) << "): " << X)                     \
            .c_str())
#endif

#define FASTLED_DBG(X) _FASTLED_DGB(X)

#ifndef FASTLED_DBG_IF
#define FASTLED_DBG_IF(COND, MSG)                                              \
    if (COND)                                                                  \
    FASTLED_DBG(MSG)
#endif // FASTLED_DBG_IF

#pragma once

#include "fl/sketch_macros.h"
#include "fl/stl/strstream.h"

// Forward declare println to avoid pulling in full dbg.h machinery
#ifndef FL_PRINTLN_DECLARED
#define FL_PRINTLN_DECLARED
namespace fl {
    void println(const char* str);
}
#endif

#ifndef FASTLED_WARN
// FASTLED_WARN: Supports stream-style formatting with << operator
// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
#define FASTLED_WARN(MSG) fl::println((fl::StrStream() << "WARN: " << MSG).c_str())
#define FASTLED_WARN_IF(COND, MSG) do { if (COND) FASTLED_WARN(MSG); } while(0)
#endif

#ifndef FL_WARN
#if SKETCH_HAS_LOTS_OF_MEMORY
// FL_WARN: Supports both string literals and stream-style formatting with << operator
// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
#define FL_WARN(X) fl::println((fl::StrStream() << "WARN: " << X).c_str())
#define FL_WARN_IF(COND, MSG) do { if (COND) FL_WARN(MSG); } while(0)

// FL_WARN_ONCE: Emits warning only once per unique location (static flag per call site)
// Uses static bool flag initialized to false - first call prints, subsequent calls no-op
#define FL_WARN_ONCE(X) do { \
    static bool _warned = false; \
    if (!_warned) { \
        _warned = true; \
        FL_WARN(X); \
    } \
} while(0)

// FL_WARN_FMT: Alias for FL_WARN (kept for backwards compatibility)
#define FL_WARN_FMT(X) FL_WARN(X)
#define FL_WARN_FMT_IF(COND, MSG) FL_WARN_IF(COND, MSG)
#else
// No-op macros for memory-constrained platforms
#define FL_WARN(X) do { } while(0)
#define FL_WARN_IF(COND, MSG) do { } while(0)
#define FL_WARN_ONCE(X) do { } while(0)
#define FL_WARN_FMT(X) do { } while(0)
#define FL_WARN_FMT_IF(COND, MSG) do { } while(0)
#endif
#endif
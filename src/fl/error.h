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

#ifndef FASTLED_ERROR
// FASTLED_ERROR: Supports stream-style formatting with << operator
// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
#define FASTLED_ERROR(MSG) fl::println((fl::StrStream() << "ERROR: " << MSG).c_str())
#define FASTLED_ERROR_IF(COND, MSG) do { if (COND) FASTLED_ERROR(MSG); } while(0)
#endif

#ifndef FL_ERROR
#if SKETCH_HAS_LOTS_OF_MEMORY
// FL_ERROR: Supports both string literals and stream-style formatting with << operator
// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
#define FL_ERROR(X) fl::println((fl::StrStream() << "ERROR: " << X).c_str())
#define FL_ERROR_IF(COND, MSG) do { if (COND) FL_ERROR(MSG); } while(0)
#else
// No-op macros for memory-constrained platforms
#define FL_ERROR(X) do { } while(0)
#define FL_ERROR_IF(COND, MSG) do { } while(0)
#endif
#endif

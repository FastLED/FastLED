// ok no namespace fl
#pragma once

// TODO: Please fix this header, it's a little messy.
//
// Note: Do NOT include "fl/warn.h" here as it causes circular dependency:
// assert_defs.h -> warn.h -> log.h -> sketch_macros.h -> ostream.h -> string.h -> shared_ptr.h -> atomic.h -> thread.h -> mutex_teensy.h -> assert.h -> assert_defs.h
//
// Instead, we use a minimal warning mechanism for assertions that doesn't require string formatting

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/js_assert.h"
#elif defined(ESP32)
#include "platforms/esp/esp_assert.h"
#endif

// Forward declare minimal print function for assertions
// Implementation is in fl/stl/cstdio.cpp
#ifndef FL_PRINTLN_DECLARED
#define FL_PRINTLN_DECLARED
namespace fl {
    void println(const char* str);
}
#endif

// Minimal FASTLED_WARN_IF macro that only accepts string literals (no formatting)
// This breaks the circular dependency by not requiring StrStream/string.h
#ifndef FASTLED_WARN_IF
#define FASTLED_WARN_IF(COND, MSG) do { if (COND) fl::println(MSG); } while(0)
#endif

#if !defined(__EMSCRIPTEN__) && !defined(ESP32)

#ifndef FASTLED_USES_SYSTEM_ASSERT
#if defined(FASTLED_TESTING)
#define FASTLED_USES_SYSTEM_ASSERT 1
#else
#define FASTLED_USES_SYSTEM_ASSERT 0
#endif
#endif

#if FASTLED_USES_SYSTEM_ASSERT
// Note: We use FASTLED_WARN_IF instead of assert() to avoid needing to include
// <cassert> in this header file (which is in strict no-stdlib-headers mode).
// The warning will still be issued in testing builds.
#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        FASTLED_WARN_IF(!(x), MSG);                                            \
    }
#else
#define FASTLED_ASSERT(x, MSG) FASTLED_WARN_IF(!(x), MSG)
#endif
#endif

// ok no namespace fl
#pragma once

#include "fl/stl/strstream.h"
#include "fl/warn.h"

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/js_assert.h"
#elif defined(ESP32)
#include "platforms/esp/esp_assert.h"
#else

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

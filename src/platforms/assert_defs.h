#pragma once

#include "fl/strstream.h"
#include "fl/warn.h"

#include <assert.h>

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
#include <assert.h>
#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        FASTLED_WARN_IF(!(x), MSG);                                            \
        assert(x);                                                             \
    }
#else
#define FASTLED_ASSERT(x, MSG) FASTLED_WARN_IF(!(x), MSG)
#endif
#endif

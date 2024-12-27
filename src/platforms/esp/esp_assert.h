#pragma once

#include "fl/warn.h"
#include "fl/has_define.h"

#ifdef ESP32
#include "platforms/esp/esp_assert.h"
#else
#if __has_include("assert.h")
#include <assert.h>  // ok include.
#else
#define assert(x) ((void)0)
#endif

#define FASTLED_ASSERT(x, MSG)         \
    {                                  \
        if (!(x)) {                    \
            FASTLED_WARN(MSG);         \
            assert(false);             \
        }                              \
    }
#endif
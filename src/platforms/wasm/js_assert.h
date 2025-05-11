#pragma once

#include "fl/warn.h"
#include <emscripten.h>

#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        if (!(x)) {                                                            \
            FASTLED_WARN(MSG);                                                 \
            emscripten_debugger();                                             \
        }                                                                      \
    }
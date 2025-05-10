#pragma once

#include <emscripten.h>
#include "fl/warn.h"

#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        if (!(x)) {                                                            \
            FASTLED_WARN(MSG);                                                 \
            emscripten_debugger();                                             \
        }                                                                      \
    }
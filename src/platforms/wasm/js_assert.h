// ok no namespace fl
#pragma once

#include "fl/warn.h"
#include <emscripten.h>

#define FASTLED_ASSERT(x, MSG)                                                 \
    do {                                                                       \
        if (!(x)) {                                                            \
            FASTLED_WARN(MSG);                                                 \
            emscripten_debugger();                                             \
        }                                                                      \
    } while (0)
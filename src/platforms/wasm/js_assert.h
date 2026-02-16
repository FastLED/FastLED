// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "fl/warn.h"
// IWYU pragma: begin_keep
#include <emscripten.h>
// IWYU pragma: end_keep

#define FASTLED_ASSERT(x, MSG)                                                 \
    do {                                                                       \
        if (!(x)) {                                                            \
            FASTLED_WARN(MSG);                                                 \
            emscripten_debugger();                                             \
        }                                                                      \
    } while (0)
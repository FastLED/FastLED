#pragma once

#include "fl/stl/strstream.h"

// Forward declaration to avoid pulling in fl/stl/cstdio.h and causing fl/stl/cstdio.cpp to be compiled
namespace fl {
    void println(const char* str);
}

#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        if (!(x)) {                                                            \
            fl::println((fl::StrStream() << "FASTLED ASSERT FAILED: " << MSG).c_str());  \
        }                                                                      \
    }

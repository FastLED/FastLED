#pragma once

#include "ftl/strstream.h"

// Forward declaration to avoid pulling in ftl/cstdio.h and causing ftl/cstdio.cpp to be compiled
namespace fl {
    void println(const char* str);
}

#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        if (!(x)) {                                                            \
            fl::println((fl::StrStream() << "FASTLED ASSERT FAILED: " << MSG).c_str());  \
        }                                                                      \
    }

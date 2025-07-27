#pragma once

// allow-include-after-namespace

// Forward declaration to avoid pulling in fl/io.h and causing fl/io.cpp to be compiled
namespace fl {
    void println(const char* str);
}

#include "fl/strstream.h"

#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        if (!(x)) {                                                            \
            fl::println((fl::StrStream() << "FASTLED ASSERT FAILED: " << MSG).c_str());  \
        }                                                                      \
    }

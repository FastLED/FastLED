#pragma once

#include "fl/strstream.h"

// Forward declaration to avoid pulling in fl/io.h and causing fl/io.cpp to be compiled
namespace fl {
    void println(const char* str);
}

#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        if (!(x)) {                                                            \
            fl::println((fl::StrStream() << "FASTLED ASSERT FAILED: " << MSG).c_str());  \
        }                                                                      \
    }

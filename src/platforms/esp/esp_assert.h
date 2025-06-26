#pragma once

#include "fl/io.h"
#include "fl/strstream.h"

#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        if (!(x)) {                                                            \
            fl::println((fl::StrStream() << "FASTLED ASSERT FAILED: " << MSG).c_str());  \
        }                                                                      \
    }

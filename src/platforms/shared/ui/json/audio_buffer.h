#pragma once

#include "ftl/vector.h"
#include "ftl/stdint.h"

namespace fl {

struct AudioBuffer {
    vector<int16_t> samples;
    uint32_t timestamp = 0;
};

} // namespace fl

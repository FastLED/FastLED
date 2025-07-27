#pragma once

#include "fl/vector.h"
#include "fl/stdint.h"

namespace fl {

struct AudioBuffer {
    vector<int16_t> samples;
    uint32_t timestamp = 0;
};

} // namespace fl

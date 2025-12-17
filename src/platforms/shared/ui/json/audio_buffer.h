#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"

namespace fl {

struct AudioBuffer {
    vector<int16_t> samples;
    uint32_t timestamp = 0;
};

} // namespace fl

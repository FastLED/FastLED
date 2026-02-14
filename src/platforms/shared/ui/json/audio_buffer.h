#pragma once

// IWYU pragma: private

#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"

namespace fl {

struct AudioBuffer {
    vector<i16> samples;
    u32 timestamp = 0;
};

} // namespace fl

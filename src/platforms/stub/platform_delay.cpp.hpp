#pragma once

// Only compile for stub platform
#ifdef FASTLED_STUB_IMPL

#include "fl/stl/thread.h"
#include "fl/stl/chrono.h"
#include "platforms/delay_platform.h"

// Forward declare the delay override (defined in time_stub.cpp.hpp)
extern fl::function<void(uint32_t)> g_delay_override;

namespace fl {
namespace platform {

void delay(fl::u32 ms) {
    // Check for test override first (for fast testing)
    if (g_delay_override) {
        g_delay_override(ms);
        return;
    }

    fl::this_thread::sleep_for(fl::chrono::milliseconds(ms));
}

}  // namespace platform
}  // namespace fl

#endif  // FASTLED_STUB_IMPL

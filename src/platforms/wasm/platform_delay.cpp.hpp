#pragma once

// Only compile for WASM platform
#ifdef __EMSCRIPTEN__

#include "platforms/delay_platform.h"
#include "fl/stl/stdint.h"
#include "js.h"  // For millis()

namespace fl {
namespace platform {

void delay(fl::u32 ms) {
    if (ms == 0) return;

    uint32_t end = fl::millis() + ms;

    // Busy-wait with 1ms granularity
    while (fl::millis() < end) {
        // No async pumping - just wait
    }
}

}  // namespace platform
}  // namespace fl

#endif  // __EMSCRIPTEN__

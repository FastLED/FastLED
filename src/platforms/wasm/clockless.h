#pragma once

// IWYU pragma: private

// ok no namespace fl

#include "platforms/wasm/is_wasm.h"
#if defined(FL_IS_WASM)
    // Use channel-based controller for WASM platform (mirrors ESP32/stub architecture)
    // This allows legacy FastLED.addLeds<>() API to route through channel drivers
    #include "platforms/wasm/clockless_channel_wasm.h"
    // Old implementation available if needed (not currently used):
    // #include "platforms/stub/clockless_stub_generic.h"
#else
    #error "This file should only be included for WASM/Emscripten builds"
#endif

// ok no namespace fl
#pragma once

#include "platforms/wasm/is_wasm.h"
#if defined(FL_IS_WASM)
#include "platforms/wasm/clockless.h"
#elif defined(FASTLED_STUB_IMPL)
// Use channel-based controller for stub platform (mirrors ESP32 architecture)
// This allows legacy FastLED.addLeds<>() API to route through channel engines
#include "platforms/stub/clockless_channel_stub.h"
// Keep old implementation available if needed (not currently used)
// #include "platforms/stub/clockless_stub_generic.h"
#else
#error "This file should only be included for stub or emscripten builds"
#endif

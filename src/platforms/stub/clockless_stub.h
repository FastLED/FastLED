#pragma once

#if defined(__EMSCRIPTEN__)
#include "platforms/wasm/clockless.h"
#elif defined(FASTLED_STUB_IMPL)
#include "platforms/stub/clockless_stub_generic.h"
#else
#error "This file should only be included for stub or emscripten builds"
#endif

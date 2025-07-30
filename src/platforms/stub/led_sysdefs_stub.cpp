

#ifdef FASTLED_STUB_IMPL  // Only use this if explicitly defined.

#include "platforms/stub/led_sysdefs_stub.h"

#if defined(__EMSCRIPTEN__)
#include "platforms/wasm/led_sysdefs_wasm.h"
#else
#include "platforms/stub/generic/led_sysdefs_generic.hpp"
#endif

// Always include timing functions when using stub
#define FASTLED_STUB_TIME_IMPL
#include "platforms/stub/time_stub.cpp"

#endif  // FASTLED_STUB_IMPL

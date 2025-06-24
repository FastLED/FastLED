#pragma once

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/led_sysdefs_wasm.h"
#else
#include "led_sysdefs_stub_generic.h"
#endif

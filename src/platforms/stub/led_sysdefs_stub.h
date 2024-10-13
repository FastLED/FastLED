#pragma once

#ifdef __EMSCRIPTEN__
#include "wasm/led_sysdefs_wasm.h"
#else
#include "led_sysdefs_stub_generic.h"
#endif

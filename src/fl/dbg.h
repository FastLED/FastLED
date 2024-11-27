#pragma once

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/dbg_defs.h"
#endif


#ifndef FASTLED_HAS_DBG
// FASTLED_DBG is a macro that can be defined to enable debug printing.
#define FASTLED_DBG(X)
#endif

#ifndef FASTLED_DBG_IF
#ifdef FASTLED_HAS_DBG
#define FASTLED_DBG_IF(COND, MSG) if (COND) FASTLED_DBG(MSG)
#else
#define FASTLED_DBG_IF(COND, MSG)
#endif  // FASTLED_HAS_DBG
#endif  // FASTLED_DBG_IF
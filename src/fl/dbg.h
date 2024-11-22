#pragma once

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/dbg_defs.h"
#endif


#ifndef FASTLED_HAS_DBG
// FLDPRINT is a macro that can be defined to enable debug printing.
#define FASTLED_DBG(X)
#endif

#ifndef FASTLED_DBG_IF
#define FASTLED_DBG_IF(COND, MSG) if (COND) FASTLED_DBG(MSG)
#endif
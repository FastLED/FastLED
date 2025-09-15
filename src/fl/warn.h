#pragma once

#include "fl/dbg.h"
#include "fl/sketch_macros.h"

#ifndef FASTLED_WARN
// in the future this will do something
#define FASTLED_WARN FASTLED_DBG
#define FASTLED_WARN_IF FASTLED_DBG_IF
#endif

#ifndef FL_WARN
#if SKETCH_HAS_LOTS_OF_MEMORY
#define FL_WARN FASTLED_WARN
#define FL_WARN_IF FASTLED_WARN_IF
#else
// No-op macros for memory-constrained platforms
#define FL_WARN(X) do { } while(0)
#define FL_WARN_IF(COND, MSG) do { } while(0)
#endif
#endif
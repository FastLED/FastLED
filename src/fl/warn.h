#pragma once

#include "fl/dbg.h"

#ifndef FASTLED_WARN
// in the future this will do something
#define FASTLED_WARN FASTLED_DBG
#define FASTLED_WARN_IF FASTLED_DBG_IF
#endif

#ifndef FL_WARN
#define FL_WARN FASTLED_WARN
#define FL_WARN_IF FASTLED_WARN_IF
#endif
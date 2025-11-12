#pragma once

#include "platforms/assert_defs.h"

#ifndef FL_ASSERT
#define FL_ASSERT(x, MSG) FASTLED_ASSERT(x, MSG)
#define FL_ASSERT_IF FASTLED_ASSERT_IF
#endif

#pragma once

#include "led_sysdefs.h"
#include "namespace.h"

namespace fl {

#if defined(__AVR__)
typedef int cycle_t;  ///< 8.8 fixed point (signed) value
#else
typedef int64_t cycle_t;  ///< 8.8 fixed point (signed) value
#endif

}


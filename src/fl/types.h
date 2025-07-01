#pragma once

#include "fl/namespace.h"
#include "fl/int.h"
#include "led_sysdefs.h"

namespace fl {

#if defined(__AVR__)
typedef int cycle_t; ///< 8.8 fixed point (signed) value
#else
typedef fl::i64 cycle_t; ///< 8.8 fixed point (signed) value
#endif

} // namespace fl

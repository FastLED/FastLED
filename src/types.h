#ifndef TYPES_H
#define TYPES_H

#include "led_sysdefs.h"

FASTLED_NAMESPACE_BEGIN

#if defined(__AVR__)
typedef int cycle_t;  ///< 8.8 fixed point (signed) value
#else
typedef int64_t cycle_t;  ///< 8.8 fixed point (signed) value
#endif

FASTLED_NAMESPACE_END

#endif  // TYPES_H
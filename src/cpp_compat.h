#ifndef __INC_CPP_COMPAT_H
#define __INC_CPP_COMPAT_H

#include "FastLED.h"

#if __cplusplus <= 199711L

#define static_assert(expression, message)
#define constexpr const

#else

// things that we can turn on if we're in a C++11 environment
#endif

#if __cplusplus < 201703L
#define REGISTER register
#else
#define REGISTER
#endif

#endif

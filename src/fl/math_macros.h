#pragma once

#include "fl/has_include.h"


#if FL_HAS_INCLUDE(<math.h>)
#include <math.h>  // for early definitions of M_PI and other math macros.
#endif // FL_HAS_INCLUDE(<math.h>)


#include "fl/compiler_control.h"
#include "fl/type_traits.h"

namespace fl {

// Fun fact, we can't define any function by the name of min,max,abs because
// on some platforms these are macros. Therefore we can only use fl_min, fl_max, fl_abs.
// This is needed for math macro ABS to work optimally.
template <typename T> inline T fl_abs(T value) {
    return value < 0 ? -value : value;
}

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(sign-compare)
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
FL_DISABLE_WARNING_FLOAT_CONVERSION


// Template functions for MIN and MAX to avoid statement repetition
// Returns the most promotable type between the two arguments
template <typename T, typename U> inline common_type_t<T, U> fl_min(T a, U b) {
    return (a < b) ? a : b;
}

template <typename T, typename U> inline common_type_t<T, U> fl_max(T a, U b) {
    return (a > b) ? a : b;
}

FL_DISABLE_WARNING_POP
} // namespace fl

#ifndef MAX
#define MAX(a, b) fl::fl_max(a, b)
#endif

#ifndef MIN
#define MIN(a, b) fl::fl_min(a, b)
#endif

#ifndef ABS
#define ABS(x) fl::fl_abs(x)
#endif

#ifndef EPSILON_F
// smallest possible float
#define EPSILON_F 1.19209290e-07F
#endif

#ifndef EPSILON_D
// smallest possible double
#define EPSILON_D 2.2204460492503131e-16
#endif

#ifndef ALMOST_EQUAL
#define ALMOST_EQUAL(a, b, small) (ABS((a) - (b)) < small)
#endif

#ifndef ALMOST_EQUAL_FLOAT
#define ALMOST_EQUAL_FLOAT(a, b) (ABS((a) - (b)) < EPSILON_F)
#endif



#ifndef ALMOST_EQUAL_DOUBLE
#define ALMOST_EQUAL_EPSILON(a, b, epsilon) (ABS((a) - (b)) < (epsilon))
#endif

#ifndef ALMOST_EQUAL_DOUBLE
#define ALMOST_EQUAL_DOUBLE(a, b) ALMOST_EQUAL_EPSILON(a, b, EPSILON_F)
#endif

#ifndef INFINITY_DOUBLE
#define INFINITY_DOUBLE (1.0 / 0.0)
#endif

#ifndef INFINITY_FLOAT
#define INFINITY_FLOAT (1.0f / 0.0f)
#endif

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F
#endif

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#ifndef M_PI
#define M_PI PI
#endif

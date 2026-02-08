/// @file types.h
/// Defines fractional types used for lib8tion functions

#pragma once

#include "fl/stl/stdint.h"
#include "fl/int.h"
namespace fl {
/// @addtogroup lib8tion
/// @{

/// @defgroup FractionalTypes Fixed-Point Fractional Types. 
/// Types for storing fractional data. 
/// Note: Fractional types have been moved to fl/int.h
/// @{


/// typedef for IEEE754 "binary32" float type internals
/// @see https://en.wikipedia.org/wiki/IEEE_754
typedef union {
    u32 i;  ///< raw value, as an integer
    float    f;  ///< raw value, as a float
    struct {
        u32 mantissa: 23;  ///< 23-bit mantissa
        u32 exponent:  8;  ///< 8-bit exponent
        u32 signbit:   1;  ///< sign bit
    };
    struct {
        u32 mant7 :  7;  ///< @todo Doc: what is this for?
        u32 mant16: 16;  ///< @todo Doc: what is this for?
        u32 exp_  :  8;  ///< @todo Doc: what is this for?
        u32 sb_   :  1;  ///< @todo Doc: what is this for?
    };
    struct {
        u32 mant_lo8 : 8;  ///< @todo Doc: what is this for?
        u32 mant_hi16_exp_lo1 : 16;  ///< @todo Doc: what is this for?
        u32 sb_exphi7 : 8;  ///< @todo Doc: what is this for?
    };
} IEEE754binary32_t;

/// @} FractionalTypes
/// @} lib8tion
}  // namespace fl
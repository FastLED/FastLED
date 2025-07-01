/// @file types.h
/// Defines fractional types used for lib8tion functions

#pragma once

#include "fl/stdint.h"
#include "fl/int.h"
#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

/// @addtogroup lib8tion
/// @{

/// @defgroup FractionalTypes Fixed-Point Fractional Types. 
/// Types for storing fractional data. 
/// Note: Fractional types have been moved to fl/int.h
/// @{


/// typedef for IEEE754 "binary32" float type internals
/// @see https://en.wikipedia.org/wiki/IEEE_754
typedef union {
    uint32_t i;  ///< raw value, as an integer
    float    f;  ///< raw value, as a float
    struct {
        uint32_t mantissa: 23;  ///< 23-bit mantissa
        uint32_t exponent:  8;  ///< 8-bit exponent
        uint32_t signbit:   1;  ///< sign bit
    };
    struct {
        uint32_t mant7 :  7;  ///< @todo Doc: what is this for?
        uint32_t mant16: 16;  ///< @todo Doc: what is this for?
        uint32_t exp_  :  8;  ///< @todo Doc: what is this for?
        uint32_t sb_   :  1;  ///< @todo Doc: what is this for?
    };
    struct {
        uint32_t mant_lo8 : 8;  ///< @todo Doc: what is this for?
        uint32_t mant_hi16_exp_lo1 : 16;  ///< @todo Doc: what is this for?
        uint32_t sb_exphi7 : 8;  ///< @todo Doc: what is this for?
    };
} IEEE754binary32_t;

/// @} FractionalTypes
/// @} lib8tion

FASTLED_NAMESPACE_END

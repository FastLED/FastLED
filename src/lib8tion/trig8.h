#pragma once

#ifndef __INC_LIB8TION_TRIG_H
#define __INC_LIB8TION_TRIG_H

#include "fl/stdint.h"
#include "lib8tion/lib8static.h"

#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION

/// @file trig8.h
/// Fast, efficient 8-bit trigonometry functions specifically
/// designed for high-performance LED programming.

/// @ingroup lib8tion
/// @{

/// @defgroup Trig Fast Trigonometry Functions
/// Fast 8-bit and 16-bit approximations of sin(x) and cos(x).
///
/// Don't use these approximations for calculating the
/// trajectory of a rocket to Mars, but they're great
/// for art projects and LED displays.
///
/// On Arduino/AVR, the 16-bit approximation is more than
/// 10X faster than floating point sin(x) and cos(x), while
/// the 8-bit approximation is more than 20X faster.
/// @{

#if defined(USE_SIN_32)

#define sin16 fl::sin16lut
#define cos16 fl::cos16lut

#include "fl/sin32.h"

#elif defined(__AVR__)

#include "trig8_avr.h"

#else

#include "trig8_c.h"

#endif

/// @} Trig
/// @} lib8tion

#endif

FL_DISABLE_WARNING_POP

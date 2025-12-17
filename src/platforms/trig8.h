#pragma once

#include "fl/stl/stdint.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"

// Platform-specific includes (before namespace declaration for include-order)
#if defined(USE_SIN_32)
#include "fl/sin32.h"
#elif defined(__AVR__)
#include "platforms/avr/atmega/common/trig8.h"
#else
#include "platforms/shared/trig8.h"
#endif

#ifndef __INC_LIB8TION_TRIG_H
#define __INC_LIB8TION_TRIG_H

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

namespace fl {

#if defined(USE_SIN_32)

inline int16_t sin16(uint16_t theta) { return fl::sin16lut(theta); }
inline int16_t cos16(uint16_t theta) { return fl::cos16lut(theta); }

#endif

}  // namespace fl

/// @} Trig
/// @} lib8tion

#endif

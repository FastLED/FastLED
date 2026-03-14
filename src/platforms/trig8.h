#pragma once

#include "fl/stl/stdint.h"
#include "lib8tion/lib8static.h"
#include "fl/stl/compiler_control.h"
#include "platforms/intmap.h"

// Platform-specific includes (before namespace declaration for include-order)
#if defined(USE_SIN_32)
#include "fl/math/sin32.h"
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

inline i16 sin16(u16 theta) { return fl::sin16lut(theta); }
inline i16 cos16(u16 theta) { return fl::cos16lut(theta); }

// 8-bit wrappers derived from 16-bit LUTs.
// sin16 returns i16 [-32768..32767], offset to u16 [0..65535],
// then use int_scale for proper u16→u8 downscale (bit-replication
// rounding, not naive truncation).
inline u8 sin8(u8 theta) {
	u16 unsigned_result = static_cast<u16>(sin16(static_cast<u16>(theta) << 8)) + 32768;
	return fl::int_scale<u16, u8>(unsigned_result);
}

inline u8 cos8(u8 theta) { return sin8(theta + 64); }

#endif

}  // namespace fl

/// @} Trig
/// @} lib8tion

#endif

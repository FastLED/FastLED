#pragma once

#include "lib8tion/config.h"
#include "scale8.h"
#include "lib8tion/lib8static.h"
#include "intmap.h"
#include "fl/namespace.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION

/// @file math8_arm.h
/// ARM DSP assembly language implementations of 8-bit math functions.
/// This file contains optimized ARM DSP assembly versions for Teensy and ARM platforms.

/// @ingroup lib8tion
/// @{

/// @defgroup Math_ARM ARM DSP Math Implementations
/// Fast ARM DSP assembly implementations of 8-bit math operations (Teensy, etc.)
/// @{

/// Add one byte to another, saturating at 0xFF (ARM DSP assembly)
LIB8STATIC_ALWAYS_INLINE uint8_t qadd8(uint8_t i, uint8_t j) {
    asm volatile("uqadd8 %0, %0, %1" : "+r"(i) : "r"(j));
    return i;
}

/// Add one byte to another, saturating at 0x7F and -0x80 (ARM DSP assembly)
LIB8STATIC_ALWAYS_INLINE int8_t qadd7(int8_t i, int8_t j) {
    asm volatile("qadd8 %0, %0, %1" : "+r"(i) : "r"(j));
    return i;
}

/// @} Math_ARM
/// @} lib8tion

FL_DISABLE_WARNING_POP

#pragma once

/// @file scalar_ops.h
/// Cross-type scalar fixed-point operations (implemented after all types are defined)

#include "fl/fixed_point/s0x32.h"
#include "fl/fixed_point/s16x16.h"

namespace fl {

// s0x32 × s16x16 → s16x16
inline s16x16 s0x32::operator*(s16x16 b) const {
    return s16x16::from_raw(static_cast<i32>(
        (static_cast<i64>(mValue) * b.raw()) >> 31));
}

// s16x16 × s0x32 → s16x16 (commutative)
inline s16x16 operator*(s16x16 a, s0x32 b) {
    return b * a;
}

} // namespace fl

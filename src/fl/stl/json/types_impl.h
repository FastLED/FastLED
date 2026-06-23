#pragma once

// FastLED #3022: the IEEE-754 bit-codec primitives that used to live here
// have moved to `fl/math/soft_float.h` so non-JSON callers can use them
// too. This file is now a thin shim that exposes the old `json_*` names
// (still referenced by the existing forward declarations in `types.h`)
// while delegating to the new home.

#include "fl/math/soft_float.h"

namespace fl {
namespace detail {

inline u32 json_round_shift_right_u64(u64 value, int shift) FL_NO_EXCEPT {
    return fl::detail::soft_round_shift_right_u64(value, shift);
}

inline int json_floor_log2_u64(u64 value) FL_NO_EXCEPT {
    return fl::detail::soft_floor_log2_u64(value);
}

inline u32 json_ieee754_float_bits_from_scaled_u64(
    u32 sign, u64 magnitude, int binary_exp) FL_NO_EXCEPT {
    return fl::detail::soft_float_bits_from_scaled_u64(sign, magnitude, binary_exp);
}

inline u32 json_ieee754_float_bits_from_double_bits(u64 bits) FL_NO_EXCEPT {
    return fl::soft_float_bits_from_double_bits(bits);
}

inline u64 json_ieee754_double_bits_from_float_bits(u32 bits) FL_NO_EXCEPT {
    return fl::soft_double_bits_from_float_bits(bits);
}

template <typename To, typename From>
inline To json_bit_copy(const From& value) FL_NO_EXCEPT {
    return fl::detail::soft_bit_copy<To, From>(value);
}

inline float json_double_to_float(double value) FL_NO_EXCEPT {
    return fl::soft_double_to_float(value);
}

inline double json_float_to_double(float value) FL_NO_EXCEPT {
    return fl::soft_float_to_double(value);
}

} // namespace detail
} // namespace fl

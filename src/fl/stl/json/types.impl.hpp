#pragma once

namespace fl {
namespace detail {

inline u32 json_round_shift_right_u64(u64 value, int shift) FL_NOEXCEPT {
    if (shift <= 0) {
        return static_cast<u32>(value);
    }
    if (shift >= 64) {
        return 0;
    }
    const u64 half = u64(1) << (shift - 1);
    const u64 mask = (u64(1) << shift) - 1;
    const u64 truncated = value >> shift;
    const u64 remainder = value & mask;
    const bool round_up = remainder > half || (remainder == half && (truncated & 1));
    return static_cast<u32>(truncated + (round_up ? 1 : 0));
}

inline int json_floor_log2_u64(u64 value) FL_NOEXCEPT {
    int bit = -1;
    while (value != 0) {
        value >>= 1;
        ++bit;
    }
    return bit;
}

inline u32 json_ieee754_float_bits_from_scaled_u64(
    u32 sign, u64 magnitude, int binary_exp) FL_NOEXCEPT {
    if (magnitude == 0) {
        return sign;
    }

    const int top_bit = json_floor_log2_u64(magnitude);
    int exponent = top_bit + binary_exp;
    if (exponent > 127) {
        return sign | 0x7F800000u;
    }

    if (exponent >= -126) {
        const int shift = top_bit - 23;
        u32 significand = shift > 0
            ? json_round_shift_right_u64(magnitude, shift)
            : static_cast<u32>(magnitude << -shift);
        if (significand >= 0x01000000u) {
            significand >>= 1;
            ++exponent;
            if (exponent > 127) {
                return sign | 0x7F800000u;
            }
        }
        return sign | (static_cast<u32>(exponent + 127) << 23) |
               (significand & 0x007FFFFFu);
    }

    const int subnormal_shift = -(binary_exp + 149);
    u32 mantissa = 0;
    if (subnormal_shift > 0) {
        mantissa = json_round_shift_right_u64(magnitude, subnormal_shift);
    } else {
        const int left_shift = -subnormal_shift;
        if (left_shift >= 64 || magnitude > (u64(-1) >> left_shift)) {
            return sign | 0x7F800000u;
        }
        const u64 shifted = magnitude << left_shift;
        mantissa = shifted > 0x007FFFFFu ? 0x00800000u : static_cast<u32>(shifted);
    }
    if (mantissa >= 0x00800000u) {
        return sign | 0x00800000u;
    }
    return sign | mantissa;
}

inline u32 json_ieee754_float_bits_from_double_bits(u64 bits) FL_NOEXCEPT {
    const u32 sign = (bits >> 63) ? 0x80000000u : 0u;
    const u32 exp_bits = static_cast<u32>((bits >> 52) & 0x7FFu);
    const u64 mantissa_bits = bits & 0x000FFFFFFFFFFFFFull;
    if (exp_bits == 0x7FFu) {
        return mantissa_bits == 0 ? sign | 0x7F800000u : sign | 0x7FC00000u;
    }
    if (exp_bits == 0) {
        return json_ieee754_float_bits_from_scaled_u64(sign, mantissa_bits, -1074);
    }
    return json_ieee754_float_bits_from_scaled_u64(
        sign, mantissa_bits | 0x0010000000000000ull,
        static_cast<int>(exp_bits) - 1075);
}

template <typename To, typename From>
inline To json_bit_copy(const From& value) FL_NOEXCEPT {
    To out = 0;
    const char* src = reinterpret_cast<const char*>(&value);
    char* dst = reinterpret_cast<char*>(&out);
    const fl::size n = sizeof(To) < sizeof(From) ? sizeof(To) : sizeof(From);
    for (fl::size i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
    return out;
}

inline float json_double_to_float(double value) FL_NOEXCEPT {
    if (sizeof(double) == sizeof(u32)) {
        return fl::bit_cast<float>(json_bit_copy<u32>(value));
    }
    return fl::bit_cast<float>(
        json_ieee754_float_bits_from_double_bits(json_bit_copy<u64>(value)));
}

} // namespace detail
} // namespace fl

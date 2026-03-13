#pragma once

// Type traits for fixed-point arithmetic.
// Computes raw types, intermediate precision, and constants based on IntBits/FracBits.

#include "fl/stl/stdint.h"
#include "fl/stl/type_traits.h"

namespace fl {

namespace detail {

// True when T is a non-bool integer type.  Used by every fixed-point
// integer constructor so they bind to any width (portable across AVR
// 16-bit int and 32-bit platforms).
template <typename T>
struct is_non_bool_integer {
    static constexpr bool value =
        fl::is_integral<T>::value &&
        !fl::is_same<typename fl::remove_cv<T>::type, bool>::value;
};

// SFINAE helper — drop into a template parameter list:
//   template <typename IntT, detail::enable_if_integer_t<IntT> = 0>
template <typename T>
using enable_if_integer_t =
    typename fl::enable_if<is_non_bool_integer<T>::value, int>::type;

// Not constexpr: calling from a constexpr context triggers a compile error.
// This is the C++11 "constexpr assert" pattern.
inline void integer_out_of_range_for_fixed_point_type() {}

// Range check: is n in [0, MAX_U]?  Works for both signed and unsigned IntT.
// For signed IntT, explicitly rejects negative values via i32 comparison.
// For unsigned IntT, the negativity check is always true (optimized away).
template <typename IntT>
constexpr bool in_unsigned_range(IntT n, u32 max_u) {
    return static_cast<i32>(fl::is_signed<IntT>::value ? (n >= IntT(0)) : 1) &&
           static_cast<u32>(n) <= max_u;
}

// Checked integer-to-raw conversion for fixed-point constructors.
// Verifies n fits in INT_BITS at constexpr time; fires a compile error if not.
// Two specializations: 16-bit storage (i16/u16) and 32-bit storage (i32/u32).
template <int IntBits, int FracBits, bool TotalLE16 = (IntBits + FracBits <= 16)>
struct int_to_fixed;

// 16-bit storage types (s12x4, s8x8, s4x12, u12x4, u8x8, u4x12).
// Intermediate multiply in i32/u32 (one step wider).
template <int IntBits, int FracBits>
struct int_to_fixed<IntBits, FracBits, true> {
    static constexpr i32 SCALE = static_cast<i32>(1) << FracBits;
    static constexpr i32 MAX_S = (static_cast<i32>(1) << (IntBits - 1)) - 1;
    static constexpr i32 MIN_S = -(static_cast<i32>(1) << (IntBits - 1));
    static constexpr u32 MAX_U = (static_cast<u32>(1) << IntBits) - 1;

    template <typename IntT>
    static constexpr i16 from_signed(IntT n) {
        return (static_cast<i32>(n) >= MIN_S && static_cast<i32>(n) <= MAX_S)
            ? static_cast<i16>(static_cast<i32>(n) * SCALE)
            : (integer_out_of_range_for_fixed_point_type(), i16(0));
    }

    template <typename IntT>
    static constexpr u16 from_unsigned(IntT n) {
        return in_unsigned_range(n, MAX_U)
            ? static_cast<u16>(static_cast<u32>(n) * SCALE)
            : (integer_out_of_range_for_fixed_point_type(), u16(0));
    }
};

// 32-bit storage types (s16x16, s24x8, s8x24, u16x16, u24x8, u8x24).
// Signed uses unsigned multiply to avoid signed overflow UB.
template <int IntBits, int FracBits>
struct int_to_fixed<IntBits, FracBits, false> {
    static constexpr u32 USCALE = static_cast<u32>(static_cast<i32>(1) << FracBits);
    static constexpr i32 MAX_S = (static_cast<i32>(1) << (IntBits - 1)) - 1;
    static constexpr i32 MIN_S = -(static_cast<i32>(1) << (IntBits - 1));
    static constexpr u32 MAX_U = (static_cast<u32>(1) << IntBits) - 1;

    template <typename IntT>
    static constexpr i32 from_signed(IntT n) {
        return (static_cast<i32>(n) >= MIN_S && static_cast<i32>(n) <= MAX_S)
            ? static_cast<i32>(static_cast<u32>(n) * USCALE)
            : (integer_out_of_range_for_fixed_point_type(), i32(0));
    }

    template <typename IntT>
    static constexpr u32 from_unsigned(IntT n) {
        return in_unsigned_range(n, MAX_U)
            ? static_cast<u32>(n) * USCALE
            : (integer_out_of_range_for_fixed_point_type(), u32(0));
    }
};

} // namespace detail

template <int IntBits, int FracBits>
struct fixed_point_traits {
    // Total bits required
    static constexpr int TOTAL_BITS = IntBits + FracBits;

    // Raw storage type: i16 if fits in 16 bits, else i32
    using raw_type = fl::conditional_t<(TOTAL_BITS <= 16), i16, i32>;
    using unsigned_raw_type = fl::conditional_t<(TOTAL_BITS <= 16), u16, u32>;

    // Intermediate types for multiplication/division (needs double width)
    using intermediate_type = fl::conditional_t<(TOTAL_BITS <= 16), i32, i64>;
    using unsigned_intermediate_type = fl::conditional_t<(TOTAL_BITS <= 16), u32, u64>;

    // Intermediate fractional precision for log2/exp2 polynomial evaluation
    // Pattern: extend FRAC_BITS to higher precision to minimize rounding error
    // - FRAC >= 24: IFRAC = FRAC (already max precision, no extension)
    // - FRAC >= 16: IFRAC = 24  (extend to 24 bits)
    // - FRAC >= 12: IFRAC = 20  (extend to 20 bits)
    // - FRAC >= 8:  IFRAC = 16  (extend to 16 bits)
    // - FRAC < 8:   IFRAC = 12  (minimum reasonable precision)
    static constexpr int IFRAC =
        (FracBits >= 24) ? FracBits :
        (FracBits >= 16) ? 24 :
        (FracBits >= 12) ? 20 :
        (FracBits >= 8)  ? 16 : 12;

    // Intermediate type for polynomial evaluation (i32 or i64 based on IFRAC)
    using poly_intermediate_type = fl::conditional_t<(IFRAC <= 16), i32, i64>;

    // Maximum overflow value for exp2_fp saturation
    static constexpr auto MAX_OVERFLOW =
        (TOTAL_BITS <= 16) ? static_cast<raw_type>(0x7FFF)
                           : static_cast<raw_type>(0x7FFFFFFF);

    // Sin/cos shift amount: sin32/cos32 output is i32 with 31 fractional bits
    // We shift right by (31 - FRAC_BITS) to get FRAC_BITS precision
    static constexpr int SIN_COS_SHIFT = 31 - FracBits;

    // Sqrt function selection: use isqrt32 for i16, isqrt64 for i32
    static constexpr bool USE_ISQRT32 = (TOTAL_BITS <= 16);

    // Compile-time overflow safety checks
    // Verify intermediate_type can hold worst-case products for operator*
    // raw_max * raw_max needs 2*TOTAL_BITS bits
    static_assert(sizeof(intermediate_type) * 8 >= TOTAL_BITS * 2,
                  "intermediate_type too narrow for operator* overflow safety");

    // Verify poly_intermediate_type can hold polynomial products
    // For IFRAC <= 16: acc * t_ifrac fits in i32 after >> IFRAC (widened to i64 for multiply)
    // For IFRAC > 16: acc * t_ifrac needs i64 throughout
    // The assertion checks we have enough bits for the intermediate product before shift
    // Max product: ~1.5 * 2^IFRAC * 2^IFRAC = ~1.5 * 2^(2*IFRAC)
    // For i32: needs 2*IFRAC + 1 <= 31, so IFRAC <= 15 (but we use i64 for multiply, safe)
    // For i64: needs 2*IFRAC + 1 <= 63, so IFRAC <= 31 (all our types fit)
    static_assert((IFRAC <= 16 && sizeof(poly_intermediate_type) * 8 >= 32) ||
                  (IFRAC > 16 && sizeof(poly_intermediate_type) * 8 >= 64),
                  "poly_intermediate_type insufficient for polynomial evaluation");
};

} // namespace fl

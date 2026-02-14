#pragma once

// Type traits for fixed-point arithmetic.
// Computes raw types, intermediate precision, and constants based on IntBits/FracBits.

#include "fl/stl/stdint.h"
#include "fl/stl/type_traits.h"

namespace fl {

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

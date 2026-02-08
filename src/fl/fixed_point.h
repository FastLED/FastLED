#pragma once

// Template alias for signed fixed-point types.
// Usage: fl::fixed_point<16, 16> resolves to fl::s16x16, etc.

#include "fl/fixed_point/s4x12.h"
#include "fl/fixed_point/s8x8.h"
#include "fl/fixed_point/s8x24.h"
#include "fl/fixed_point/s12x4.h"
#include "fl/fixed_point/s16x16.h"
#include "fl/fixed_point/s24x8.h"

namespace fl {

// Primary template — intentionally undefined.
// Only valid specializations (matching concrete types) may be used.
template <int IntBits, int FracBits>
struct fixed_point_impl;

template <> struct fixed_point_impl<4, 12>  { using type = s4x12; };
template <> struct fixed_point_impl<8, 8>   { using type = s8x8; };
template <> struct fixed_point_impl<8, 24>  { using type = s8x24; };
template <> struct fixed_point_impl<12, 4>  { using type = s12x4; };
template <> struct fixed_point_impl<16, 16> { using type = s16x16; };
template <> struct fixed_point_impl<24, 8>  { using type = s24x8; };

template <int IntBits, int FracBits>
using fixed_point = typename fixed_point_impl<IntBits, FracBits>::type;

} // namespace fl

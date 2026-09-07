#pragma once

// Source transfer-function decode for the color pipeline (P6, #4040).
//
// One semantic conversion: normalize an 8-bit code and apply the source
// profile's inverse transfer, producing u16 linear light. The input domain is
// 256 values, so a table is exact and costs one load per channel -- no pow()
// and no float in the per-pixel path.

#include "fl/gfx/color_profile.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Decode one 8-bit source code to u16 linear light under `transfer`.
///
/// Exact at both endpoints: code 0 -> 0, code 255 -> 65535.
u16 decodeTransferU16(TransferFunction transfer, u8 code) FL_NO_EXCEPT;

}  // namespace fl

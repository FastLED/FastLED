#pragma once

// Source RGB -> XYZ in the pipeline working domain (P6, #4040).
//
// The matrix is derived once when a profile is bound, in float, and stored
// quantized. The per-pixel path is three multiply-accumulates per component
// in fixed point -- no float, no division, no allocation.

#include "fl/gfx/color_profile.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Source-primaries matrix in s16.16, rows ordered X, Y, Z.
///
/// Built at bind time by quantizing the float derivation, so the cost of the
/// inverse and the white-point solve is paid once per channel rather than
/// once per pixel.
struct SourceMatrixQ16 {
    i32 m[3][3];
};

/// Quantize the source matrix for `primaries`. False if the primaries are
/// collinear, which makes the primary matrix singular.
bool buildSourceMatrixQ16(const RgbPrimaries& primaries,
                          SourceMatrixQ16* out) FL_NO_EXCEPT;

/// One pixel: u16 linear RGB -> s16.16 XYZ.
///
/// Inputs are the u16 linear values `decodeTransferU16` produces and are read
/// as s16.16 raw, so full scale is 65535/65536 rather than exactly 1. That
/// bias is uniform across all three channels and both the matrix and the
/// white point, so it scales the result by 0.9999847 -- a luminance error of
/// 0.0015%, against a 0.5% budget -- and it buys an exact shift in place of a
/// division by 65535 in the per-pixel path.
void linearRgbToXyzQ16(const SourceMatrixQ16& matrix, u16 r, u16 g, u16 b,
                       i32 out_xyz[3]) FL_NO_EXCEPT;

}  // namespace fl

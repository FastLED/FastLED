#pragma once

// OKLab in the pipeline working domain (P7, #4041).
//
// The gamut mapper selected in docs/color-gamut-algorithm-selection.md works
// in OKLab, so the working domain needs the transform in s16.16 rather than
// in float. Both directions are three multiply-accumulates per component
// around a per-component nonlinearity -- a cube root going in, a cube coming
// back -- with no float, no division, no allocation and no trigonometry.
//
// The matrices are universal constants rather than profile-derived, so they
// are quantized at compile time and cost nothing to build.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Largest magnitude either direction accepts, as an s16.16 raw value (4.0).
///
/// Inputs are clamped to this before any multiply. The working domain never
/// approaches it -- XYZ stays inside roughly [0, 2] and OKLab inside
/// [-1, 1.5] -- but the transform must not be a way to reach signed overflow
/// from a caller's bad data. With coefficients bounded by 2.43 (Q16 159160,
/// under 2^17.3) and inputs bounded here by 2^18, each product is under
/// 2^35.3 and a row sum under 2^37, so the i64 accumulator has ~26 bits of
/// headroom. Clamping also bounds the cube: 4.0 cubed is 64, far inside i32.
constexpr i32 kOklabQ16MaxMagnitude = 4 * 65536;

/// XYZ (s16.16, D65-relative) -> OKLab (s16.16), L then a then b.
///
/// The cube root is `fl::icbrt64`, exact to within one ULP. That the exact
/// root is what the mapper needs, and how much slack it has, is measured in
/// docs/color-gamut-algorithm-selection.md.
void xyzToOklabQ16(const i32 (&xyz)[3], i32 (&out_lab)[3]) FL_NO_EXCEPT;

/// OKLab (s16.16) -> XYZ (s16.16). The inverse of `xyzToOklabQ16`.
///
/// Not bit-exact in round trip: both directions round to nearest and the
/// cube root truncates. `oklabToXyzQ16(xyzToOklabQ16(v))` returns within
/// 32 ULP over the working domain.
///
/// The other order degrades near black. OKLab cube-roots the cone
/// responses, and that root's derivative, 1 / (3 * lms^(2/3)), diverges as a
/// response approaches zero: around L = 0.05 it is roughly 1200, so any
/// error in the response is hugely amplified coming back out. Restricted to
/// in-gamut colours the worst measured round trip there is about 300 ULP.
/// This is conditioning rather than representation -- carrying lms or XYZ at
/// Q32 was measured and does not fix it -- and it costs the P7 mapper
/// nothing. `tests/fl/gfx/oklab_q16.cpp` pins both ends.
void oklabToXyzQ16(const i32 (&lab)[3], i32 (&out_xyz)[3]) FL_NO_EXCEPT;

}  // namespace fl

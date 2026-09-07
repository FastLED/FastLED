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

/// Largest XYZ magnitude the forward transform accepts, as an s16.16 raw
/// value (64.0).
///
/// Not a theoretical bound -- the working domain really does get large. An
/// emitter profile normalized to unit *luminance* per emitter, which is what
/// `EmitterProfile` carries, puts the blue emitter's Z near 13 (see
/// `device_solve.h`), so a saturated blue at a few times unit drive reaches
/// XYZ in the tens. An earlier revision of this file clamped at 4.0 on the
/// assumption that XYZ stayed inside [0, 2]; the P7 gamut mapper found that
/// wrong immediately, and silently, by producing identical OKLab for targets
/// 30x apart in luminance.
///
/// 64.0 leaves the accumulator far inside i64: coefficients are bounded by
/// 2.43 (Q16 159160, under 2^17.3) and inputs here by 2^22, so each product
/// is under 2^39.3 and a row sum under 2^41.
constexpr i32 kOklabQ16MaxXyz = 64 * 65536;

/// Largest OKLab magnitude the inverse transform accepts (4.0).
///
/// Tighter than the XYZ bound because the inverse *cubes* its intermediate.
/// At this bound the cube-rooted LMS stays under 15.6, whose cube is 3796 --
/// still inside i32 as s16.16, with the widest intermediate product around
/// 2^44. Raising it much further would overflow the result, and there is
/// nothing to raise it for: OKLab lightness is about 1 for a display white
/// and stays near 4 even for absurd inputs, while chroma rarely passes 0.4.
constexpr i32 kOklabQ16MaxLab = 4 * 65536;

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

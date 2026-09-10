#pragma once

// Device solve: XYZ -> per-emitter light (P7, #4041).
//
// For a three-emitter device the solve is exact and linear: the emitter
// matrix is inverted once when a profile is bound, and each pixel is one
// matrix multiply. A3/B11 forbid iterative solvers such as nnls3 on the
// per-pixel path; nothing here is iterative, and a structural test enforces
// that the implementation stays that way.

#include "fl/gfx/colorimetric_response.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

// These three symbols are public for the same reason the P6 stage headers
// are: the pipeline is being landed stage by stage, and the `show()` wiring
// that will call them needs P7's gamut mapper first. They are not exposed
// through `fl/gfx/gfx.h` and are not part of the sketch-facing API.
//
// The existing colorimetric API cannot carry this. `solve_rgb_colorimetric`
// is float and may fall back to `nnls3`, which A3/B11 forbid per pixel --
// that is precisely what this replaces on the streaming path, and
// ci/tests/test_no_iterative_solver_per_pixel.py enforces the separation.

/// Inverse emitter matrix in s16.16, mapping XYZ to three emitter drives.
///
/// Entries exceed 1.0 for realistic primaries -- the blue emitter's Z is
/// around 13 at unit luminance -- so this is s16.16, not fractional-only.
struct EmitterSolveMatrixQ16 {
    i32 m[3][3];
};

/// A three-emitter profile with every field in s16.16.
///
/// `EmitterProfile` stores floats, so a bind path that starts from it reaches
/// float before it reaches anything else. This is the float-free input P9
/// item 2 asks for (FastLED#4043), kept as its own type rather than as a
/// change to `EmitterProfile` -- that one is consumed by the legacy RGBW
/// stack too, and converting it is cross-cutting with P2.
struct EmitterChromaticitiesQ16 {
    /// CIE 1931 xy per emitter, s16.16, so 0.64 is 41943.
    i32 xy_r[2];
    i32 xy_g[2];
    i32 xy_b[2];
    /// Peak luminance per emitter relative to source white, s16.16.
    i32 lum_r;
    i32 lum_g;
    i32 lum_b;
};

/// Build the inverse emitter matrix from a Q16 profile, without floats.
///
/// The float-free counterpart of `buildRgbSolveMatrixQ16`, and the second
/// half of P9 item 2: that one converts float chromaticities through
/// `xyY_to_XYZ` and `invert3x3` before quantising, so a target that never
/// wants a float symbol cannot use it.
///
/// `ci/color_fixed_profile_study.py` prices what this costs against the float
/// derivation: worst 0.1085 dE2000 on BT.2020, against A1's 0.5 and the ~0.35
/// the derivation has once the gamut mapper's 0.15 is counted.
///
/// False on the same profiles `buildRgbSolveMatrixQ16` refuses -- a
/// non-positive or degenerate chromaticity, a non-positive luminance, or a
/// matrix the inverse cannot represent -- with one addition the float path
/// has no equivalent of: a `y` small enough to quantise to zero, which would
/// otherwise be divided by.
bool buildRgbSolveMatrixFromQ16(const EmitterChromaticitiesQ16& profile,
                                EmitterSolveMatrixQ16* out) FL_NO_EXCEPT;

/// Inverse of an s16.16 3x3 matrix, in s16.16, computed without floats.
///
/// The float-free half of P9 item 2 (FastLED#4043). `buildRgbSolveMatrixQ16`
/// still reaches this through `invert3x3`; a bind path that never touches
/// float calls it directly.
///
/// False on a singular matrix, on a coefficient too large for s16.16, and on
/// any input whose exact cofactors or determinant do not fit an i64. That
/// last case is detected rather than bounded away: a deep-blue emitter at
/// xy = (0.14, 0.03) already has a Z column near 28, so a fixed input limit
/// generous enough to be safe would reject real parts.
bool invert3x3Q16(const i32 (&in)[3][3], i32 (&out)[3][3]) FL_NO_EXCEPT;

/// Invert the emitter matrix for a three-emitter profile.
///
/// False when the emitter chromaticities are non-finite, degenerate, or
/// collinear, any of which makes the matrix singular and the solve
/// meaningless.
bool buildRgbSolveMatrixQ16(const colorimetric_response::EmitterProfile& profile,
                            EmitterSolveMatrixQ16* out) FL_NO_EXCEPT;

/// One pixel: XYZ in s16.16 to three emitter drives in s16.16.
///
/// Drives may come out negative for a target outside the device gamut. That
/// is deliberate: clamping belongs to the gamut mapper, and silently
/// clipping here would hide out-of-gamut targets from it.
void solveRgbDrivesQ16(const EmitterSolveMatrixQ16& matrix,
                       const i32 (&xyz)[3], i32 (&drives)[3]) FL_NO_EXCEPT;

}  // namespace fl

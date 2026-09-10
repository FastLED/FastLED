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

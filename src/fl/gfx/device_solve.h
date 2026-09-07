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

/// Inverse emitter matrix in s16.16, mapping XYZ to three emitter drives.
///
/// Entries exceed 1.0 for realistic primaries -- the blue emitter's Z is
/// around 13 at unit luminance -- so this is s16.16, not fractional-only.
struct EmitterSolveMatrixQ16 {
    i32 m[3][3];
};

/// Invert the emitter matrix for a three-emitter profile.
///
/// False when the emitter chromaticities are non-finite, degenerate, or
/// collinear, any of which makes the matrix singular and the solve
/// meaningless.
bool buildRgbSolveMatrixQ16(const EmitterProfile& profile,
                            EmitterSolveMatrixQ16* out) FL_NO_EXCEPT;

/// One pixel: XYZ in s16.16 to three emitter drives in s16.16.
///
/// Drives may come out negative for a target outside the device gamut. That
/// is deliberate: clamping belongs to the gamut mapper, and silently
/// clipping here would hide out-of-gamut targets from it.
void solveRgbDrivesQ16(const EmitterSolveMatrixQ16& matrix,
                       const i32 (&xyz)[3], i32 (&drives)[3]) FL_NO_EXCEPT;

}  // namespace fl

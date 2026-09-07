#pragma once

// Chromatic adaptation between white points (P6, #4040).
//
// B7 makes Bradford the normative CAT. The whole adaptation collapses to one
// 3x3 matrix, built when a profile is bound, so nothing iterative or
// trigonometric runs per pixel -- and folding it into the source matrix makes
// it free, since the two then cost a single multiply together.

#include "fl/gfx/color_profile.h"
#include "fl/gfx/source_xyz.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// A collapsed adaptation transform in s16.16.
///
/// Entries exceed 1.0 -- the D50 to D65 Z scale is about 1.33 -- so this is
/// s16.16 rather than a fractional-only format.
struct AdaptationMatrixQ16 {
    i32 m[3][3];
};

/// Build the Bradford transform from `source_white` to `destination_white`.
/// False if either white has a zero cone response, which no real white does
/// but a corrupt profile might.
bool buildBradfordMatrixQ16(Chromaticity source_white,
                            Chromaticity destination_white,
                            AdaptationMatrixQ16* out) FL_NO_EXCEPT;

/// Adapt one XYZ triple.
///
/// Public because it is the only way to exercise adaptation on its own.
/// `foldAdaptationIntoSourceMatrix` cannot carry it: that folds into a source
/// matrix and never exposes an adapted XYZ, so there is no existing seam this
/// could route through. The tracker asks P5's vectors to be "serialized per
/// stage so embedded phases can bisect failures", and bisecting a stage
/// requires being able to run that stage alone -- P7 and P8 will need this
/// when their own budgets miss. The streaming path uses the fold.
void adaptXyzQ16(const AdaptationMatrixQ16& matrix, const i32 (&xyz)[3],
                 i32 (&out_xyz)[3]) FL_NO_EXCEPT;

/// Pre-multiply the adaptation into a source matrix, in place.
///
/// This is how the streaming path should use it: primaries and adaptation
/// become one matrix at bind time, so the per-pixel cost of adaptation is
/// zero rather than a second matrix multiply per channel.
void foldAdaptationIntoSourceMatrix(const AdaptationMatrixQ16& adaptation,
                                    SourceMatrixQ16* source) FL_NO_EXCEPT;

}  // namespace fl

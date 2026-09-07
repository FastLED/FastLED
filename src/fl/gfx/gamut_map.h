#pragma once

// Gamut mapping onto the device hull (P7, #4041).
//
// `docs/color-gamut-algorithm-selection.md` selects OKLCh chroma compression
// and rules out the cheaper alternatives: clipping and max-normalization
// both land about 20 dE2000 from the reference against a budget of 0.5, and
// no amount of clamping substitutes for the objective.
//
// The per-pixel path here is fully bounded and contains no iterative solver
// in the A3/B11 sense, no division, no float, and no trigonometry:
//
//   1. one forward OKLab transform (three cube roots),
//   2. one comparison against a precomputed device constant for lightness,
//   3. eight halvings, each an inverse OKLab transform and one solve.
//
// Chroma is compressed by scaling (a, b) toward zero rather than by going
// polar. Since a = C cos h and b = C sin h, scaling both by one factor is
// exactly scaling C at fixed hue -- so hue is preserved by construction and
// atan2, hypot, cos and sin never appear.

#include "fl/gfx/device_solve.h"
#include "fl/gfx/white_allocation.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Number of chroma halvings the mapper runs, fixed at compile time.
///
/// Eight is what the study selected: it meets A1 with about a threefold
/// margin (0.15 dE2000 against 0.5) and needs no lookup table -- a 16 KB LUT
/// scored worse than *four* halvings, because a grid cannot represent the
/// gamut boundary's corners at the primaries.
///
/// Being a constant is the point. A3/B11 forbid iterative solvers such as
/// `nnls3` on the per-pixel path; a fixed halving count is a bounded
/// computation whose cost is known when the firmware is built, not a loop
/// that runs until a convergence criterion is met.
constexpr int kGamutMapHalvings = 8;

/// Everything the per-pixel mapper needs, derived once when a profile binds.
struct GamutMapQ16 {
    EmitterSolveMatrixQ16 solve;

    /// OKLab lightness of the brightest D65 neutral this device can reach.
    ///
    /// Chroma compression cannot rescue a target that is too *bright*: at
    /// zero chroma the point is still outside the hull, so a chroma-only
    /// bisection converges on an infeasible answer. The reference clamps
    /// lightness into the attainable range first, and this is that bound.
    ///
    /// It is a device constant rather than a per-pixel search. Along the D65
    /// neutral ray the drives are s * (M^-1 . D65), so the largest feasible
    /// s is 1 / max(M^-1 . D65) and the bound follows -- exactly, with no
    /// iteration. Measured against a 40-step bisection over targets from
    /// 1.05x to 50x device white, the two agree to 0 ULP.
    i32 max_neutral_lightness;
};

/// The same mapper for a device with a white emitter.
///
/// The distinction is not cosmetic. A white emitter enlarges the reachable
/// set, and testing a target against the RGB hull alone under-reports it
/// badly: over 200 000 targets drawn from inside a four-emitter device's own
/// zonotope, the RGB-only solve rejects **43%** of them. Every one of those
/// would be compressed by the three-emitter mapper despite the device being
/// able to produce it exactly.
struct GamutMapRgbwQ16 {
    WhiteAllocationQ16 allocation;

    /// OKLab lightness of the brightest D65 neutral this device can reach.
    ///
    /// Larger than the three-emitter bound by whatever the white emitter
    /// adds -- 2.398 against 1.398 for a white at D65 and unit luminance,
    /// exactly the one unit it contributes.
    ///
    /// Derived at bind time by bisecting between two bounds: the
    /// three-emitter one, always reachable with the white emitter off, and a
    /// relaxation of the problem that ignores the lower limits on the RGB
    /// drives. The relaxation is an upper bound but not generally
    /// attainable -- full white can push a channel negative -- so it is
    /// tested rather than trusted. The bisection runs once per profile,
    /// never per pixel.
    i32 max_neutral_lightness;
};

/// Derive the mapper for a three-primary profile plus one white emitter.
///
/// `white_xyz` is the white emitter's XYZ at full drive, in s16.16.
/// `policy` is C3's per-profile choice of which end of the feasible white
/// interval to take; it changes the drives, never which targets are
/// reachable, so the hull this maps onto is the same either way.
bool buildGamutMapRgbwQ16(const EmitterProfile& profile,
                          const i32 (&white_xyz)[3],
                          WhiteAllocationPolicy policy,
                          GamutMapRgbwQ16* out) FL_NO_EXCEPT;

/// One pixel: XYZ in s16.16 to four in-gamut drives, red, green, blue, white.
///
/// Same shape as the three-emitter path -- one forward OKLab transform, one
/// lightness comparison, then eight halvings -- but every feasibility test
/// goes through the white-preferred allocation, so the hull it maps onto is
/// the device's real one.
void mapAndAllocateRgbwQ16(const GamutMapRgbwQ16& map, const i32 (&xyz)[3],
                           i32 (&drives)[4]) FL_NO_EXCEPT;

/// Derive the mapper for a three-emitter profile.
///
/// False on the same degenerate profiles `buildRgbSolveMatrixQ16` rejects,
/// and on a profile that cannot reach any neutral at all.
bool buildGamutMapQ16(const EmitterProfile& profile, GamutMapQ16* out) FL_NO_EXCEPT;

/// One pixel: XYZ in s16.16 to in-gamut emitter drives in s16.16.
///
/// Drives always come back inside [0, 1]. A target already inside the hull
/// is returned untouched, so an in-gamut image pays only the solve and the
/// bounds check.
void mapAndSolveDrivesQ16(const GamutMapQ16& map, const i32 (&xyz)[3],
                          i32 (&drives)[3]) FL_NO_EXCEPT;

}  // namespace fl

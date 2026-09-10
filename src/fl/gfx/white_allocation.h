#pragma once

// White-preferred allocation for a device with a white emitter (C3, #4041).
//
// A white emitter makes the emitter matrix wide: a target's preimage is no
// longer unique, and C3 asks for the white-preferred one -- as much light
// from the white emitter as the target allows.
//
// The P5 reference finds it by enumerating vertices, which A3/B11 forbid per
// pixel. It does not need to be. With the white emitter at drive w, the RGB
// drives making up the difference are
//
//     d(w) = M^-1 . target  -  w * (M^-1 . white)
//
// which is affine in w. Each of the six bounds on the three RGB drives is
// therefore one inequality in w, the feasible set is a single interval, and
// white-preferred is its upper end.
//
// `docs/color-gamut-algorithm-selection.md` records the measurement: over
// the corpus's `rgbw` and `non_d65_white` devices this reproduces the
// reference's drives to 1.1e-15 and 7.8e-16 respectively -- float64
// rounding. The derivation never assumed the white sat on the neutral axis,
// which is what the second device checks.
//
// Two whites are a different problem, solved by
// `allocateTwoWhiteDrivesQ16` further down: maximizing w1 + w2 is a linear
// program over a polygon, and the obvious reduction -- run this twice and
// keep the better answer -- agrees with the reference on every corpus vector
// while being wrong on 85% of random reachable targets, because a single
// emitter's drive is capped at 1. See the same document.

#include "fl/gfx/device_solve.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Which end of the feasible white interval to take (C3).
///
/// The interval exists because the preimage is not unique: any white level
/// inside it reproduces the target exactly, with the RGB drives making up
/// the difference. Picking an end is a policy, not a calculation, which is
/// why it is per profile rather than per pixel.
enum class WhiteAllocationPolicy {
    /// As much white as the target allows. C3's default -- the white emitter
    /// is usually the efficient one and the better colour renderer.
    WhitePreferred,

    /// As little as the target allows, which for most targets is none. The
    /// per-profile override C3 asks for: a device whose white emitter
    /// renders worse than its primaries, or whose primaries are wanted for
    /// saturation, takes this end instead.
    ///
    /// Not the same as ignoring the white emitter. Where RGB alone cannot
    /// reach the target, this returns the *smallest* white level that makes
    /// it reachable rather than failing.
    RgbPreferred,
};

/// Everything the per-pixel allocation needs, derived once when a profile
/// binds.
struct WhiteAllocationQ16 {
    /// Inverse RGB emitter matrix.
    EmitterSolveMatrixQ16 rgb_solve;

    /// `M^-1 . white`: the RGB drives one unit of white light replaces.
    ///
    /// Precomputing this is what leaves the per-pixel path with a single
    /// matrix multiply. It does not depend on the pixel.
    i32 per_white[3];

    /// Per-channel drive slack, in s16.16 raw units.
    ///
    /// The allowance a drive may fall outside [0, 1] and still be treated as
    /// reachable. It is per channel because its *cost* is per channel: one
    /// drive unit of an emitter is that emitter's XYZ column, and a saturated
    /// blue primary carries a column an order of magnitude larger than a
    /// green one. A single number in drive space therefore buys a different
    /// amount of colour error on each channel. FastLED#4303.
    i32 slack[3];

    /// Which end of the interval this profile takes.
    WhiteAllocationPolicy policy;
};

/// Derive the allocation for a three-primary profile plus one white emitter.
///
/// `white_xyz` is the white emitter's XYZ at full drive, in s16.16 -- the
/// same working domain the rest of the pipeline carries.
///
/// False on the profiles `buildRgbSolveMatrixQ16` rejects, and on a white
/// emitter the RGB primaries cannot express at all.
bool buildWhiteAllocationQ16(const EmitterProfile& profile,
                             const i32 (&white_xyz)[3],
                             WhiteAllocationPolicy policy,
                             WhiteAllocationQ16* out) FL_NO_EXCEPT;

/// One pixel: XYZ in s16.16 to four drives, in the order red, green, blue,
/// white, at whichever end of the feasible interval the profile's policy
/// names.
///
/// False when no white level keeps the RGB drives in range, which means the
/// target is outside the device hull -- the gamut mapper's job, not this
/// one's. `drives` is not written in that case.
///
/// Cost note, recorded rather than optimized away on a guess: this performs
/// up to six s16.16 divisions per pixel, and is the only stage in the
/// pipeline that divides per pixel at all. Each bound can instead be
/// compared by cross-multiplication, leaving one division for the chosen
/// level, at the price of i64 multiplies that are not obviously cheaper on
/// an 8-bit target. Nobody has measured which wins, so the straightforward
/// version is what ships.
bool allocateEmitterDrivesQ16(const WhiteAllocationQ16& allocation,
                              const i32 (&xyz)[3],
                              i32 (&drives)[4]) FL_NO_EXCEPT;

/// Everything the two-white allocation needs, derived once when a profile
/// binds (C3, #4198).
struct TwoWhiteAllocationQ16 {
    /// Inverse RGB emitter matrix.
    EmitterSolveMatrixQ16 rgb_solve;

    /// `M^-1 . w2`: the RGB drives one unit of the second white replaces.
    i32 per_white2[3];

    /// `M^-1 . w1 - M^-1 . w2`, the difference column.
    ///
    /// Held rather than recomputed because it is the coefficient of the
    /// split variable in every bound, and the per-pixel path reads it eight
    /// times. The first white's own column is not kept: it only ever
    /// appears through this difference.
    i32 difference[3];

    /// Which end of the feasible total this profile takes.
    WhiteAllocationPolicy policy;
};

/// Derive the allocation for three primaries plus two white emitters.
///
/// `white1_xyz` / `white2_xyz` are each white's XYZ at full drive, in
/// s16.16, the same working domain the rest of the pipeline carries.
///
/// False on the profiles `buildRgbSolveMatrixQ16` rejects, and when either
/// white lands so far outside what the primaries express that the per-pixel
/// bounds would overflow their accumulators -- see `kTwoWhiteMaxColumn` in
/// the implementation, which a real white emitter is nowhere near.
bool buildTwoWhiteAllocationQ16(const EmitterProfile& profile,
                                const i32 (&white1_xyz)[3],
                                const i32 (&white2_xyz)[3],
                                WhiteAllocationPolicy policy,
                                TwoWhiteAllocationQ16* out) FL_NO_EXCEPT;

/// One pixel: XYZ in s16.16 to five drives -- red, green, blue, white1,
/// white2 -- at whichever end of the feasible total the profile's policy
/// names.
///
/// The method, and why it is not a search. Writing the total `s = w1 + w2`
/// and substituting `w2 = s - w1` leaves the RGB drives as
///
///     (d0 - s * per_white2) - w1 * difference
///
/// which is the one-white shape with a shifted target. So at any fixed total
/// the feasible `w1` is again an interval, bounded by five lower and five
/// upper bounds: three from the RGB drives, and two more because `w1` and
/// `w2 = s - w1` are each a drive in their own right. Every one of those
/// bounds is *affine in s*, so "some `w1` exists at this total" is exactly
/// the conjunction of the pairwise inequalities `lower_k(s) <= upper_j(s)`,
/// each linear in `s` and each solvable for one `s` bound. Intersecting them
/// gives the feasible totals in closed form -- no bisection, no vertex
/// enumeration, no per-pixel iteration (A3/B11).
///
/// That the interval has to be *found* rather than assumed is the whole
/// point. Achievable totals form `[s_lo, s_hi]`, which need not start at
/// zero: a bright target is unreachable with the primaries alone, so a
/// bisection seeded at zero has no feasible starting point and reports such
/// targets unreachable. The first attempt at this made exactly that
/// assumption (#4198).
///
/// At the chosen total the split is *not* free, which is measurement rather
/// than assumption. The reference settles a free split by minimizing the sum
/// of squares of the RGB drives, and an earlier revision did the same here;
/// the feasible splits at the extreme total turn out to be a single point,
/// so there was nothing for the rule to choose. Width measured 0.0 over 4000
/// random targets on the corpus's cool/warm device and 951 on a device built
/// to make one drive's constraint parallel to `w1 + w2` -- the shape that
/// could have produced an optimal edge. Two whites of the same colour are
/// the exception, and there the reference's own tie-break is the end this
/// takes.
///
/// False when no total keeps every drive in range, which means the target is
/// outside the device hull -- the gamut mapper's job, not this one's.
/// `drives` is not written in that case.
///
/// Cost note, recorded rather than optimized away on a guess: up to 25
/// s16.16 divisions per pixel against the one-white path's six. Whether
/// cross-multiplying the pairwise comparisons -- which would leave one
/// division and a great many i64 multiplies -- wins on an 8-bit target is
/// unmeasured, so the straightforward version is what ships.
bool allocateTwoWhiteDrivesQ16(const TwoWhiteAllocationQ16& allocation,
                               const i32 (&xyz)[3],
                               i32 (&drives)[5]) FL_NO_EXCEPT;

}  // namespace fl

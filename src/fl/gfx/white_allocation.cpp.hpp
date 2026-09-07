// ok no header - implementation for fl/gfx/white_allocation.h

#include "fl/gfx/white_allocation.h"

namespace fl {

namespace {

/// Full drive, as an s16.16 raw value.
constexpr i32 kWhiteFullDrive = 65536;

/// Slack on the drive bounds, in s16.16 raw units.
///
/// The same reason the gamut mapper carries one: a colour the device
/// reproduces exactly does not solve to exactly 0 or 1, and without slack
/// the allocation would reject targets it can actually hit. 64 raw units is
/// a quarter of one code at 8-bit output.
constexpr i32 kWhiteSlack = 64;

/// `(numerator << 16) / denominator` as s16.16, kept in i64.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it.
///
/// The shift happens in i64: a numerator near i32's range shifted left by 16
/// needs 47 bits. The result stays there too, and is deliberately not
/// clamped.
///
/// An earlier revision clamped it to full drive and argued the clamp was
/// decision-preserving -- a bound outside +/-1.0 either does not bind or
/// means infeasible, so the min and max that follow reach the same verdict
/// either way. The argument does not hold: it turns a lower bound of
/// "w >= 6.64", which no drive can satisfy, into "w >= 1.0", which an upper
/// bound of exactly 1.0 then meets, so the interval collapses to a single
/// point instead of being empty.
///
/// That is latent here rather than a live bug, and it is worth being precise
/// about why. The drives are `at_zero - level * per_white`, which reproduce
/// the target for *any* level -- that is an identity, not a property of the
/// level -- so the real feasibility test is the range check on the drives at
/// the end, and it rejects these targets whether or not the bound was
/// clamped. A sweep of 300 000 random targets finds no difference in what
/// this function returns.
///
/// It is still wrong to leave. The bound is consumed as an interval, and any
/// caller that uses the interval for something other than picking one of its
/// two ends inherits the collapse -- which is exactly how it was found, by
/// work on a two-white allocation that bisects over totals. i64 costs
/// nothing: the comparisons that follow are between values the caller
/// already holds, and the chosen level is inside [0, full] before narrowing.
i64 divideWhiteQ16(i32 numerator, i32 denominator) FL_NO_EXCEPT {
    const i64 scaled = static_cast<i64>(numerator) << 16;
    return scaled / static_cast<i64>(denominator);
}

/// Scale an s16.16 value by an s16.16 factor, rounding to nearest.
i32 scaleWhiteQ16(i32 value, i32 factor) FL_NO_EXCEPT {
    const i64 product = static_cast<i64>(value) * static_cast<i64>(factor);
    return static_cast<i32>((product + 32768) >> 16);
}

}  // namespace

bool buildWhiteAllocationQ16(const EmitterProfile& profile,
                             const i32 (&white_xyz)[3],
                             WhiteAllocationPolicy policy,
                             WhiteAllocationQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    out->policy = policy;
    if (!buildRgbSolveMatrixQ16(profile, &out->rgb_solve)) {
        return false;
    }
    solveRgbDrivesQ16(out->rgb_solve, white_xyz, out->per_white);

    // A white emitter the primaries cannot express at all leaves nothing for
    // the allocation to trade against, and every bound below would divide by
    // zero or be vacuous.
    bool expressible = false;
    for (int i = 0; i < 3; ++i) {
        if (out->per_white[i] != 0) {
            expressible = true;
        }
    }
    return expressible;
}

bool allocateEmitterDrivesQ16(const WhiteAllocationQ16& allocation,
                              const i32 (&xyz)[3],
                              i32 (&drives)[4]) FL_NO_EXCEPT {
    i32 at_zero[3];
    solveRgbDrivesQ16(allocation.rgb_solve, xyz, at_zero);

    // The interval of white levels that keeps every RGB drive in range.
    // Each bound is one inequality in the white level because d(w) is affine
    // in w -- that is the whole reason no search is needed here.
    //
    // Strict bounds, deliberately. Slack here would be *spent* rather than
    // merely tolerated: the policy maximizes white, so widening the bounds
    // by 64 raw units lets it take 64 units out of every RGB drive on every
    // pixel. That is not a rounding allowance, it is a systematic colour
    // shift -- measured at white = 886 where the reference says 0. The
    // slack lives on the drive check below, which is what actually decides
    // feasibility.
    i64 low = 0;
    i64 high = kWhiteFullDrive;
    for (int i = 0; i < 3; ++i) {
        const i32 slope = allocation.per_white[i];
        if (slope != 0) {
            // Dividing by a negative slope swaps which bound is which.
            const i64 first = divideWhiteQ16(at_zero[i], slope);
            const i64 second = divideWhiteQ16(at_zero[i] - kWhiteFullDrive, slope);
            const i64 upper = slope > 0 ? first : second;
            const i64 lower = slope > 0 ? second : first;
            if (upper < high) {
                high = upper;
            }
            if (lower > low) {
                low = lower;
            }
        } else if (at_zero[i] < -kWhiteSlack ||
                   at_zero[i] > kWhiteFullDrive + kWhiteSlack) {
            // White cannot move this drive at all, and it is already out.
            return false;
        }
    }
    // At the dark floor the solve rounds a drive to -1 raw on a colour the
    // device reproduces exactly -- corpus target (4, 1, 19) solves to
    // (0, -1, 1) -- which drags `high` below zero. Clamp rather than refuse;
    // the drive check below decides whether the target is really reachable.
    if (high < 0) {
        high = 0;
    }
    if (low > high) {
        return false;
    }

    // Either end reproduces the target exactly; which one is policy. Both
    // ends are inside [0, full] by construction, so the narrowing is safe.
    const i32 level = static_cast<i32>(
        allocation.policy == WhiteAllocationPolicy::RgbPreferred ? low : high);
    i32 rgb[3];
    for (int i = 0; i < 3; ++i) {
        const i32 drive = at_zero[i] - scaleWhiteQ16(allocation.per_white[i], level);
        if (drive < -kWhiteSlack || drive > kWhiteFullDrive + kWhiteSlack) {
            return false;
        }
        rgb[i] = drive < 0 ? 0 : (drive > kWhiteFullDrive ? kWhiteFullDrive : drive);
    }
    drives[0] = rgb[0];
    drives[1] = rgb[1];
    drives[2] = rgb[2];
    drives[3] = level < 0 ? 0
                          : (level > kWhiteFullDrive ? kWhiteFullDrive : level);
    return true;
}

}  // namespace fl

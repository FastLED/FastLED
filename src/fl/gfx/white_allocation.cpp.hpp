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

/// `(numerator << 16) / denominator`, in s16.16, with `denominator` non-zero.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it.
///
/// The shift happens in i64: a numerator near i32's range shifted left by 16
/// needs 47 bits, which is exactly the overflow this exists to avoid.
i32 divideWhiteQ16(i32 numerator, i32 denominator) FL_NO_EXCEPT {
    const i64 scaled = static_cast<i64>(numerator) << 16;
    return static_cast<i32>(scaled / static_cast<i64>(denominator));
}

/// Scale an s16.16 value by an s16.16 factor, rounding to nearest.
i32 scaleWhiteQ16(i32 value, i32 factor) FL_NO_EXCEPT {
    const i64 product = static_cast<i64>(value) * static_cast<i64>(factor);
    return static_cast<i32>((product + 32768) >> 16);
}

}  // namespace

bool buildWhiteAllocationQ16(const EmitterProfile& profile,
                             const i32 (&white_xyz)[3],
                             WhiteAllocationQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
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

bool allocateWhitePreferredQ16(const WhiteAllocationQ16& allocation,
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
    i32 low = 0;
    i32 high = kWhiteFullDrive;
    for (int i = 0; i < 3; ++i) {
        const i32 slope = allocation.per_white[i];
        if (slope != 0) {
            // Dividing by a negative slope swaps which bound is which.
            const i32 first = divideWhiteQ16(at_zero[i], slope);
            const i32 second = divideWhiteQ16(at_zero[i] - kWhiteFullDrive, slope);
            const i32 upper = slope > 0 ? first : second;
            const i32 lower = slope > 0 ? second : first;
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

    // White-preferred: the top of the interval.
    const i32 level = high;
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

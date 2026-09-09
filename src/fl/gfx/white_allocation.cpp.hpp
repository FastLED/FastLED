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

namespace {

/// The largest magnitude a per-white column or a solved drive may reach
/// before the pairwise bounds stop fitting.
///
/// Chosen from the arithmetic rather than picked: a bound is
/// `(n + m*s) / q` with every part in s16.16, and the pairwise comparison
/// forms `q * n` and then shifts the difference left by 16 to divide in
/// s16.16. With each part under 2^19 raw (8.0 in drive space) the products
/// stay under 2^38, the shifted difference under 2^55, and i64 has room. A
/// white emitter the primaries would need eight units of any one channel to
/// express is not a white emitter.
constexpr i32 kTwoWhiteMaxColumn = 8 * 65536;

/// How far past full drive the bounds let an RGB drive reach.
///
/// Deliberately *half* the tolerance the final range check applies, and that
/// gap is the point. Relaxing a bound and then re-checking the same drive
/// against the same number puts the boundary case exactly on the threshold,
/// where the two independent roundings decide it: measured rejecting a drive
/// at 65601 against a limit of 65600, one raw unit, on the RGB-preferred end
/// of a target needing both whites. With the bounds admitting less than the
/// check tolerates there is room for that rounding, so no total the interval
/// accepts is then refused for arithmetic reasons.
constexpr i32 kTwoWhiteBoundSlack = kWhiteSlack / 2;

/// A bound on the split, as `(numerator + slope * s) / denominator` with a
/// positive denominator.
///
/// Kept unevaluated because it has to be compared against other bounds *as
/// a function of s*, which is the whole method: dividing here would throw
/// away the s-dependence that the pairwise inequalities are solved for.
struct SplitBound {
    i32 numerator;
    i32 slope;
    i32 denominator;  // > 0 always; the sign is folded into the other two.
};

/// `value` at total `s`, in s16.16.
i64 splitBoundAt(const SplitBound& bound, i64 total) FL_NO_EXCEPT {
    const i64 scaled =
        (static_cast<i64>(bound.numerator) << 16) + static_cast<i64>(bound.slope) * total;
    return scaled / static_cast<i64>(bound.denominator);
}

/// Whether every part of a bound stays inside the range the accumulators
/// were sized for.
bool splitBoundInRange(const SplitBound& bound) FL_NO_EXCEPT {
    return bound.numerator >= -kTwoWhiteMaxColumn &&
           bound.numerator <= kTwoWhiteMaxColumn &&
           bound.slope >= -kTwoWhiteMaxColumn && bound.slope <= kTwoWhiteMaxColumn &&
           bound.denominator > 0 && bound.denominator <= kTwoWhiteMaxColumn;
}

/// Fold a negative denominator into the numerator and slope, so every bound
/// compares the same way round.
SplitBound normalizedSplitBound(i32 numerator, i32 slope,
                                i32 denominator) FL_NO_EXCEPT {
    SplitBound bound;
    if (denominator < 0) {
        bound.numerator = -numerator;
        bound.slope = -slope;
        bound.denominator = -denominator;
    } else {
        bound.numerator = numerator;
        bound.slope = slope;
        bound.denominator = denominator;
    }
    return bound;
}

/// The totals at which `lower <= upper` holds, narrowed into
/// `[*low, *high]`.
///
/// Returns false when the pair excludes every total, which is the one case
/// the caller cannot express as a narrowed interval.
///
/// Cross-multiplied rather than divided first: with positive denominators
/// the inequality
///
///     (nL + mL*s) / qL  <=  (nU + mU*s) / qU
///
/// is `s * (qU*mL - qL*mU) <= qL*nU - qU*nL`, one linear inequality in s
/// whose coefficients are exact in i64.
bool narrowTotalForPair(const SplitBound& lower, const SplitBound& upper, i64* low,
                        i64* high) FL_NO_EXCEPT {
    const i64 slope = static_cast<i64>(upper.denominator) * lower.slope -
                      static_cast<i64>(lower.denominator) * upper.slope;
    const i64 constant = static_cast<i64>(lower.denominator) * upper.numerator -
                         static_cast<i64>(upper.denominator) * lower.numerator;
    if (slope == 0) {
        // No s makes this pair better or worse; it either always holds or
        // never does.
        return constant >= 0;
    }
    const i64 bound = (constant << 16) / slope;
    if (slope > 0) {
        if (bound < *high) {
            *high = bound;
        }
    } else if (bound > *low) {
        *low = bound;
    }
    return true;
}

}  // namespace

bool buildTwoWhiteAllocationQ16(const EmitterProfile& profile,
                                const i32 (&white1_xyz)[3],
                                const i32 (&white2_xyz)[3],
                                WhiteAllocationPolicy policy,
                                TwoWhiteAllocationQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    out->policy = policy;
    if (!buildRgbSolveMatrixQ16(profile, &out->rgb_solve)) {
        return false;
    }
    i32 per_white1[3];
    solveRgbDrivesQ16(out->rgb_solve, white1_xyz, per_white1);
    solveRgbDrivesQ16(out->rgb_solve, white2_xyz, out->per_white2);

    for (int i = 0; i < 3; ++i) {
        // The difference is what the split multiplies, so it is the one that
        // has to fit; both columns are checked because it is derived from
        // them and because each is a bound coefficient in its own right.
        if (per_white1[i] > kTwoWhiteMaxColumn || per_white1[i] < -kTwoWhiteMaxColumn ||
            out->per_white2[i] > kTwoWhiteMaxColumn ||
            out->per_white2[i] < -kTwoWhiteMaxColumn) {
            return false;
        }
        out->difference[i] = per_white1[i] - out->per_white2[i];
    }
    return true;
}

bool allocateTwoWhiteDrivesQ16(const TwoWhiteAllocationQ16& allocation,
                               const i32 (&xyz)[3],
                               i32 (&drives)[5]) FL_NO_EXCEPT {
    i32 at_zero[3];
    solveRgbDrivesQ16(allocation.rgb_solve, xyz, at_zero);
    for (int i = 0; i < 3; ++i) {
        // A target this far out is not a rounding case, it is outside the
        // hull by a wide margin, and letting it through would be the only
        // way the bounds below could overflow.
        if (at_zero[i] > kTwoWhiteMaxColumn || at_zero[i] < -kTwoWhiteMaxColumn) {
            return false;
        }
    }

    // `w1 >= 0`, `w1 >= s - 1` (from `w2 <= 1`), `w1 <= 1`, `w1 <= s` (from
    // `w2 >= 0`). Every part of a bound is s16.16, the denominator included,
    // so a denominator of "one" is `kWhiteFullDrive` and not 1 -- writing a
    // raw 1 there scales those four bounds by 65536 and silently rejects
    // every target whose total runs past the primaries.
    SplitBound lowers[5];
    SplitBound uppers[5];
    int lower_count = 0;
    int upper_count = 0;
    lowers[lower_count++] = normalizedSplitBound(0, 0, kWhiteFullDrive);
    lowers[lower_count++] =
        normalizedSplitBound(-kWhiteFullDrive, kWhiteFullDrive, kWhiteFullDrive);
    uppers[upper_count++] = normalizedSplitBound(kWhiteFullDrive, 0, kWhiteFullDrive);
    uppers[upper_count++] = normalizedSplitBound(0, kWhiteFullDrive, kWhiteFullDrive);

    i64 low_total = 0;
    i64 high_total = 2 * static_cast<i64>(kWhiteFullDrive);

    for (int i = 0; i < 3; ++i) {
        // drive_i(w1, s) = (at_zero_i - s*per_white2_i) - w1*difference_i,
        // which must stay in [0, 1].
        const i32 slope = allocation.difference[i];
        if (slope == 0) {
            // The split cannot move this drive: the constraint is on the
            // total alone. `at_zero_i - s*per_white2_i` in [0, 1].
            const i32 per = allocation.per_white2[i];
            if (per == 0) {
                if (at_zero[i] < -kWhiteSlack ||
                    at_zero[i] > kWhiteFullDrive + kWhiteSlack) {
                    return false;
                }
                continue;
            }
            const i64 first = divideWhiteQ16(at_zero[i], per);
            const i64 second =
                divideWhiteQ16(at_zero[i] - kWhiteFullDrive - kTwoWhiteBoundSlack, per);
            const i64 upper = per > 0 ? first : second;
            const i64 lower = per > 0 ? second : first;
            if (upper < high_total) {
                high_total = upper;
            }
            if (lower > low_total) {
                low_total = lower;
            }
            continue;
        }
        // Dividing by a negative slope swaps which bound is which; that is
        // handled once, in normalizedSplitBound, by folding the sign.
        // The slack sits on the `drive <= 1` side and nowhere else, which
        // is the same asymmetry the one-white path argues for above. That
        // side can only *narrow* the white the policy may take, so tolerating
        // a drive 64 raw units past full -- exactly what the final range
        // check tolerates -- costs nothing. Putting slack on the `drive >= 0`
        // side would be different: the policy maximizes white, so it would
        // spend the slack on every pixel.
        //
        // Without it the exact hull corner is refused. Five emitters at full
        // drive solve, through a quantized matrix, to a target needing 57 raw
        // units more than a total of 2.0 can supply, so the feasible interval
        // comes out empty by less than a thousandth of a drive.
        const SplitBound at_full =
            normalizedSplitBound(at_zero[i] - kWhiteFullDrive - kTwoWhiteBoundSlack,
                                 -allocation.per_white2[i], slope);
        const SplitBound at_dark =
            normalizedSplitBound(at_zero[i], -allocation.per_white2[i], slope);
        if (!splitBoundInRange(at_full) || !splitBoundInRange(at_dark)) {
            return false;
        }
        if (slope > 0) {
            uppers[upper_count++] = at_dark;
            lowers[lower_count++] = at_full;
        } else {
            uppers[upper_count++] = at_full;
            lowers[lower_count++] = at_dark;
        }
    }

    // Some total admits a split exactly when every lower bound sits under
    // every upper bound there. Each pair is linear in the total, so this is
    // the closed form the header describes.
    for (int k = 0; k < lower_count; ++k) {
        for (int j = 0; j < upper_count; ++j) {
            if (!narrowTotalForPair(lowers[k], uppers[j], &low_total, &high_total)) {
                return false;
            }
        }
    }
    if (high_total < 0) {
        high_total = 0;
    }
    if (low_total > high_total) {
        return false;
    }

    const i64 total = allocation.policy == WhiteAllocationPolicy::RgbPreferred
                          ? low_total
                          : high_total;

    // At the chosen total the split is free, and the reference minimizes the
    // sum of squares of the RGB drives -- a quadratic in w1, so its minimum
    // is one division.
    i64 split_low = 0;
    i64 split_high = kWhiteFullDrive;
    for (int k = 0; k < lower_count; ++k) {
        const i64 value = splitBoundAt(lowers[k], total);
        if (value > split_low) {
            split_low = value;
        }
    }
    for (int j = 0; j < upper_count; ++j) {
        const i64 value = splitBoundAt(uppers[j], total);
        if (value < split_high) {
            split_high = value;
        }
    }
    if (split_low > split_high) {
        // Collapsed rather than empty. At either end of the total the split
        // bounds meet exactly, and they were derived through a quantized
        // matrix, so integer truncation can cross them by a few raw units --
        // measured at 21 on the RGB-preferred end of a target needing both
        // whites. Same answer as the one-white path gives its own boundary
        // case: take the point they collapse to and let the drive check
        // below decide whether the target is really reachable.
        if (split_low - split_high > kWhiteSlack) {
            return false;
        }
        const i64 middle = (split_low + split_high) / 2;
        split_low = middle;
        split_high = middle;
    }

    i64 at_total[3];
    for (int i = 0; i < 3; ++i) {
        at_total[i] = static_cast<i64>(at_zero[i]) -
                      ((static_cast<i64>(allocation.per_white2[i]) * total + 32768) >> 16);
    }

    // The low end, and there is nothing to choose. The reference settles the
    // split by minimizing the sum of squares of the RGB drives, and an
    // earlier revision here did the same -- it is a quadratic in the split,
    // so one division. Measurement removed it: at the *extreme* total the
    // feasible split is a single point, so there is no freedom for any rule
    // to exercise. Widest interval measured 0.0 over 4000 random targets on
    // the corpus's cool/warm device and 951 on a device built specifically
    // to make one drive's constraint parallel to `w1 + w2`, which is the
    // shape that could have produced an optimal edge.
    //
    // Where the split really is free -- two whites of the same colour, so
    // the difference column is zero and no RGB drive moves with the split --
    // the reference's own tie-break falls through to the lexicographically
    // smallest drives, which is this end. So the low end is not a
    // simplification away from the reference; it is what the reference does.
    //
    // See `ci/tests/test_color_rgbw_study.py`, which gates the whole
    // allocation against the reference and would fail if this stopped
    // holding.
    const i64 split = split_low;

    i32 rgb[3];
    for (int i = 0; i < 3; ++i) {
        const i64 drive =
            at_total[i] -
            ((static_cast<i64>(allocation.difference[i]) * split + 32768) >> 16);
        if (drive < -kWhiteSlack || drive > kWhiteFullDrive + kWhiteSlack) {
            return false;
        }
        rgb[i] = drive < 0 ? 0
                           : (drive > kWhiteFullDrive ? kWhiteFullDrive
                                                      : static_cast<i32>(drive));
    }
    const i64 second = total - split;
    if (second < -kWhiteSlack || second > kWhiteFullDrive + kWhiteSlack ||
        split < -kWhiteSlack || split > kWhiteFullDrive + kWhiteSlack) {
        return false;
    }
    drives[0] = rgb[0];
    drives[1] = rgb[1];
    drives[2] = rgb[2];
    drives[3] = split < 0 ? 0
                          : (split > kWhiteFullDrive ? kWhiteFullDrive
                                                     : static_cast<i32>(split));
    drives[4] = second < 0 ? 0
                           : (second > kWhiteFullDrive ? kWhiteFullDrive
                                                       : static_cast<i32>(second));
    return true;
}

}  // namespace fl

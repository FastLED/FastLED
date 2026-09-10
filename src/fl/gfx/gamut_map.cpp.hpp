// ok no header - implementation for fl/gfx/gamut_map.h

#include "fl/gfx/gamut_map.h"

#include "fl/gfx/oklab_q16.h"

namespace fl {

namespace {

/// Full drive, as an s16.16 raw value.
constexpr i32 kGamutFullDrive = 65536;

/// Probes used to find a seed inside the feasible chroma interval when the
/// target is brighter than the device's brightest neutral (#4245).
///
/// Above that lightness the feasible chroma is still a single interval, but it
/// no longer contains zero -- so the halving bracket `[0, target]`, whose whole
/// invariant is that the low end is feasible, starts infeasible and converges
/// on zero. That is the clamp this replaces: it discards up to 29.7% of the
/// lightness the hull can actually reach on this device.
///
/// A linear scan rather than a search because there is nothing to search on:
/// the interval's location is what is unknown. Bounded like the halvings and
/// for the same reason (A3/B11) -- the cost of the above-cap path is
/// `kGamutMapProbes + 2 * kGamutMapHalvings` feasibility tests, fixed when the
/// firmware is built. Targets at or below the cap never run it.
constexpr int kGamutMapProbes = 8;

/// How far above the target's own chroma the probes reach, in Q16.
///
/// The interval above the cap can start *above* the requested chroma -- a
/// bright near-neutral is infeasible precisely because it is not saturated
/// enough -- so a ceiling of one would find no seed on exactly the targets
/// this is for. Four is a factor of the request rather than an absolute
/// chroma, which keeps a near-neutral request near-neutral instead of
/// answering it with a saturated colour.
constexpr i32 kGamutProbeCeilingQ16 = 4 << 16;

/// D65 in s16.16, the white the working domain is normalized to.
constexpr i32 kGamutD65Q16[3] = {62289, 65536, 71372};

/// Slack allowed on the drive bounds, in s16.16 raw units.
///
/// A colour the device reproduces *exactly* -- full white, or a primary at
/// full drive -- does not come back as exactly 1.0. Rounding through the
/// source matrix, the s16.16 quantization and the solve leaves it a handful
/// of ULP out; 8 was the largest overshoot observed. Without slack the
/// mapper would treat those colours as out of gamut and compress them, so
/// the most ordinary targets there are would be the ones it altered.
///
/// 64 ULP is 0.1% of full drive -- a quarter of one code at 8-bit output,
/// well under a quarter at 10-bit -- so nothing it admits can survive
/// quantization as a visible difference. The final clamp makes the returned
/// drives exactly in range regardless.
constexpr i32 kGamutFeasibilitySlack = 64;

/// True when every drive is inside [0, 1], allowing `slack` ULP either side.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it.
///
/// Both bounds matter. Checking only the lower one accepts a target that
/// needs drives above full scale -- too bright rather than too saturated --
/// which no device can produce, and the mapper would then hand it back
/// unchanged.
///
/// The slack is a parameter rather than a constant because the two callers
/// want different things. The entry check wants it, so a colour the device
/// reproduces exactly is not compressed over a few ULP of rounding. The
/// halving loop must not have it: a candidate accepted while a drive sits
/// slightly negative gets that drive clamped to zero afterwards, and since
/// this profile's blue emitter carries a Z near 13, even 64 ULP of blue is
/// enough to visibly rotate the hue -- which is the one thing the mapper
/// exists to preserve. Measured at 1.5 degrees on a near-boundary target
/// before the loop was made strict.
bool gamutDrivesAreInRange(const i32 (&drives)[3], i32 slack) FL_NO_EXCEPT {
    for (int i = 0; i < 3; ++i) {
        if (drives[i] < -slack || drives[i] > kGamutFullDrive + slack) {
            return false;
        }
    }
    return true;
}

/// Clamp drives into [0, 1].
///
/// The accepted candidate is already in range up to the rounding of the last
/// inverse transform, which can leave a drive a few ULP outside. Clamping
/// makes the postcondition exact rather than nearly true.
void clampGamutDrives(i32 (&drives)[3]) FL_NO_EXCEPT {
    for (int i = 0; i < 3; ++i) {
        if (drives[i] < 0) {
            drives[i] = 0;
        } else if (drives[i] > kGamutFullDrive) {
            drives[i] = kGamutFullDrive;
        }
    }
}

/// Scale an s16.16 value by an s16.16 factor, rounding to nearest.
i32 scaleGamutQ16(i32 value, i32 factor) FL_NO_EXCEPT {
    const i64 product = static_cast<i64>(value) * static_cast<i64>(factor);
    return static_cast<i32>((product + 32768) >> 16);
}

/// XYZ of the candidate at `factor` of the target's chroma.
void chromaCandidateXyz(const i32 (&lab)[3], i32 lightness, i32 factor,
                        i32 (&out_xyz)[3]) FL_NO_EXCEPT {
    const i32 candidate_lab[3] = {
        lightness,
        scaleGamutQ16(lab[1], factor),
        scaleGamutQ16(lab[2], factor),
    };
    oklabToXyzQ16(candidate_lab, out_xyz);
}

/// Feasibility against the three-emitter hull.
struct RgbFeasible {
    const EmitterSolveMatrixQ16& solve;

    bool operator()(const i32 (&xyz)[3]) const FL_NO_EXCEPT {
        i32 drives[3];
        solveRgbDrivesQ16(solve, xyz, drives);
        return gamutDrivesAreInRange(drives, 0);
    }
};

/// Feasibility against the device's real hull when it has a white emitter.
struct RgbwFeasible {
    const WhiteAllocationQ16& allocation;

    bool operator()(const i32 (&xyz)[3]) const FL_NO_EXCEPT {
        i32 drives[4];
        return allocateEmitterDrivesQ16(allocation, xyz, drives);
    }
};

/// Feasibility against the device's real hull when it has two white
/// emitters.
struct RgbwwFeasible {
    const TwoWhiteAllocationQ16& allocation;

    bool operator()(const i32 (&xyz)[3]) const FL_NO_EXCEPT {
        i32 drives[5];
        return allocateTwoWhiteDrivesQ16(allocation, xyz, drives);
    }
};

/// Largest chroma factor the hull accepts, by a fixed count of halvings.
///
/// Shared by both mappers so the search cannot drift between them; only the
/// feasibility predicate differs. `low` is the largest factor known to be
/// feasible, `high` the smallest known not to be.
template <typename Feasible>
i32 largestFeasibleChroma(const i32 (&lab)[3], i32 lightness,
                          Feasible feasible) FL_NO_EXCEPT {
    i32 low = 0;
    i32 high = kGamutFullDrive;
    for (int step = 0; step < kGamutMapHalvings; ++step) {
        const i32 factor = (low + high) >> 1;
        i32 candidate_xyz[3];
        chromaCandidateXyz(lab, lightness, factor, candidate_xyz);
        if (feasible(candidate_xyz)) {
            low = factor;
        } else {
            high = factor;
        }
    }
    return low;
}

/// The feasible chroma interval at the target's *own* lightness, as factors of
/// the target's chroma.
///
/// Above the brightest neutral the interval is still one run -- that much is
/// measured, on this device and on the wide ones -- but it no longer contains
/// zero (#4245). `largestFeasibleChroma` cannot find it: its bracket assumes a
/// feasible low end, and starting infeasible it walks down to zero, which is
/// the clamp this exists to avoid.
///
/// So the seed comes first, by a linear scan, and only then are the two edges
/// bisected. Returns false when no probe lands inside, which is the caller's
/// signal to keep the shipped clamp-then-bisect path -- the reason this is
/// never worse than what it replaces.
template <typename Feasible>
bool feasibleChromaInterval(const i32 (&lab)[3], i32 lightness,
                            Feasible feasible, i32* out_low,
                            i32* out_high) FL_NO_EXCEPT {
    i32 seed = 0;
    bool found = false;
    for (int probe = 1; probe <= kGamutMapProbes; ++probe) {
        const i32 factor = static_cast<i32>(
            (static_cast<i64>(kGamutProbeCeilingQ16) * probe) / kGamutMapProbes);
        i32 candidate_xyz[3];
        chromaCandidateXyz(lab, lightness, factor, candidate_xyz);
        if (feasible(candidate_xyz)) {
            seed = factor;
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }

    // Lower edge: the smallest feasible factor. `high` is feasible throughout,
    // `low` is not, which is the mirror of the usual invariant.
    i32 low = 0;
    i32 high = seed;
    for (int step = 0; step < kGamutMapHalvings; ++step) {
        const i32 factor = low + ((high - low) >> 1);
        i32 candidate_xyz[3];
        chromaCandidateXyz(lab, lightness, factor, candidate_xyz);
        if (feasible(candidate_xyz)) {
            high = factor;
        } else {
            low = factor;
        }
    }
    *out_low = high;

    // Upper edge: the largest feasible factor, the usual invariant again.
    low = seed;
    high = kGamutProbeCeilingQ16;
    for (int step = 0; step < kGamutMapHalvings; ++step) {
        const i32 factor = low + ((high - low) >> 1);
        i32 candidate_xyz[3];
        chromaCandidateXyz(lab, lightness, factor, candidate_xyz);
        if (feasible(candidate_xyz)) {
            low = factor;
        } else {
            high = factor;
        }
    }
    *out_high = low;
    return true;
}

/// The highest lightness at or below `requested` whose feasible chroma
/// interval the probe scan can still find.
///
/// Without this the mapper is discontinuous at the top of the reachable
/// region, and badly: holding the requested lightness works until the probes
/// stop landing inside the interval, and the fallback then drops all the way
/// to the neutral cap. Swept along a fixed hue at chroma 0.10 that step
/// measured 0.91 in summed drive -- 234 eight-bit codes, against the ~1 code
/// the rest of the mapper's paths stay inside.
///
/// Bisecting instead lands on the edge of the region rather than jumping past
/// it, so the answer walks down continuously as the target rises. The neutral
/// cap is the low end of the bracket because chroma zero is feasible there by
/// construction, which is the invariant the search needs.
///
/// Costs `kGamutMapHalvings * kGamutMapProbes` feasibility tests, and only on
/// an above-cap target whose interval was not found at its own lightness.
template <typename Feasible>
i32 highestReachableLightness(const i32 (&lab)[3], i32 cap, i32 requested,
                              Feasible feasible) FL_NO_EXCEPT {
    i32 low = cap;       // feasible: chroma zero is reachable at the cap
    i32 high = requested; // not feasible, or the caller would not be here
    for (int step = 0; step < kGamutMapHalvings; ++step) {
        const i32 middle = low + ((high - low) >> 1);
        i32 unused_low = 0;
        i32 unused_high = 0;
        if (feasibleChromaInterval(lab, middle, feasible, &unused_low,
                                   &unused_high)) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low;
}

/// The target's chroma clamped into `[low, high]`, in factor space.
i32 clampChromaFactor(i32 low, i32 high) FL_NO_EXCEPT {
    i32 factor = kGamutFullDrive;
    if (factor < low) {
        factor = low;
    }
    if (factor > high) {
        factor = high;
    }
    return factor;
}

}  // namespace

bool buildGamutMapQ16(const colorimetric_response::EmitterProfile& profile, GamutMapQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    if (!buildRgbSolveMatrixQ16(profile, &out->solve)) {
        return false;
    }

    // The drives this device needs to reproduce D65 at unit luminance. The
    // largest of them is what saturates first as the neutral is scaled up,
    // so it sets the brightest neutral the device can reach.
    i32 neutral_drives[3];
    solveRgbDrivesQ16(out->solve, kGamutD65Q16, neutral_drives);
    i32 largest = neutral_drives[0];
    for (int i = 0; i < 3; ++i) {
        // Every drive, not just the largest. A solve like {-x, y, z} has a
        // positive largest but describes a device whose primaries do not
        // enclose D65, so it cannot make a neutral at any brightness. Taking
        // the largest alone would scale that infeasible point up and hand
        // back the lightness of a colour the device cannot produce, leaving
        // the mapper's lightness bound too permissive.
        if (neutral_drives[i] <= 0) {
            return false;
        }
        if (neutral_drives[i] > largest) {
            largest = neutral_drives[i];
        }
    }

    // s_max = 1 / largest, in s16.16. This is the one division in the whole
    // module, and it runs once per bind rather than once per pixel.
    //
    // Computed and clamped in i64. `buildRgbSolveMatrixQ16` accepts emitter
    // luminances up to 1e6, and a profile bright enough to reach D65 on a
    // drive of one or two raw units puts 2^32 / largest at or past i32's
    // range -- 2^31 exactly, at largest == 2. Narrowing that is
    // implementation-defined, and the bound it produced would be nonsense.
    //
    // The clamp is at 64.0 because that is where the OKLab transform's
    // domain ends: a neutral scaled past it would be clamped there anyway,
    // so nothing downstream can tell the difference.
    i64 scale_wide = (static_cast<i64>(kGamutFullDrive) << 16) /
                     static_cast<i64>(largest);
    if (scale_wide > kOklabQ16MaxMagnitude) {
        scale_wide = kOklabQ16MaxMagnitude;
    }
    const i32 scale = static_cast<i32>(scale_wide);
    const i32 brightest_neutral[3] = {
        scaleGamutQ16(kGamutD65Q16[0], scale),
        scaleGamutQ16(kGamutD65Q16[1], scale),
        scaleGamutQ16(kGamutD65Q16[2], scale),
    };
    i32 lab[3];
    xyzToOklabQ16(brightest_neutral, lab);
    out->max_neutral_lightness = lab[0];
    return true;
}

void mapAndSolveDrivesQ16(const GamutMapQ16& map, const i32 (&xyz)[3],
                          i32 (&drives)[3]) FL_NO_EXCEPT {
    solveRgbDrivesQ16(map.solve, xyz, drives);
    if (gamutDrivesAreInRange(drives, kGamutFeasibilitySlack)) {
        // Already inside the hull. An in-gamut image pays one solve, this
        // comparison and the clamp, and never touches OKLab at all.
        clampGamutDrives(drives);
        return;
    }

    i32 lab[3];
    xyzToOklabQ16(xyz, lab);

    i32 lightness = lab[0];
    if (lightness < 0) {
        lightness = 0;
    }

    // Above the brightest neutral, try keeping the lightness (#4245). The
    // feasible chroma there is an interval that does not contain zero, so it
    // has to be located before either edge can be bisected; when it is found,
    // clamping the target's chroma into it holds the requested lightness
    // instead of discarding up to 29.7% of what the hull reaches.
    if (lightness > map.max_neutral_lightness) {
        i32 low_edge = 0;
        i32 high_edge = 0;
        if (feasibleChromaInterval(lab, lightness, RgbFeasible{map.solve},
                                   &low_edge, &high_edge)) {
            i32 candidate_xyz[3];
            chromaCandidateXyz(lab, lightness, clampChromaFactor(low_edge, high_edge),
                               candidate_xyz);
            i32 candidate[3];
            solveRgbDrivesQ16(map.solve, candidate_xyz, candidate);
            if (gamutDrivesAreInRange(candidate, kGamutFeasibilitySlack)) {
                for (int i = 0; i < 3; ++i) {
                    drives[i] = candidate[i];
                }
                clampGamutDrives(drives);
                return;
            }
        }
        // No seed at this lightness. Walking down to the highest lightness
        // that still has one keeps the answer continuous; dropping straight
        // to the neutral cap does not, and that step measured 234 eight-bit
        // codes before this search was added.
        const i32 reachable = highestReachableLightness(
            lab, map.max_neutral_lightness, lightness, RgbFeasible{map.solve});
        i32 edge_low = 0;
        i32 edge_high = 0;
        if (reachable > map.max_neutral_lightness &&
            feasibleChromaInterval(lab, reachable, RgbFeasible{map.solve},
                                   &edge_low, &edge_high)) {
            i32 candidate_xyz[3];
            chromaCandidateXyz(lab, reachable,
                               clampChromaFactor(edge_low, edge_high),
                               candidate_xyz);
            i32 candidate[3];
            solveRgbDrivesQ16(map.solve, candidate_xyz, candidate);
            if (gamutDrivesAreInRange(candidate, kGamutFeasibilitySlack)) {
                for (int i = 0; i < 3; ++i) {
                    drives[i] = candidate[i];
                }
                clampGamutDrives(drives);
                return;
            }
        }
        // Nothing above the cap works for this hue at all. Fall through to
        // the clamp, which is what shipped before #4245.
        lightness = map.max_neutral_lightness;
    }

    // Then chroma, by scaling (a, b) toward zero.
    const i32 factor = largestFeasibleChroma(lab, lightness, RgbFeasible{map.solve});
    i32 mapped_xyz[3];
    chromaCandidateXyz(lab, lightness, factor, mapped_xyz);
    solveRgbDrivesQ16(map.solve, mapped_xyz, drives);
    clampGamutDrives(drives);
}

bool buildGamutMapRgbwQ16(const colorimetric_response::EmitterProfile& profile,
                          const i32 (&white_xyz)[3],
                          WhiteAllocationPolicy policy,
                          GamutMapRgbwQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    if (!buildWhiteAllocationQ16(profile, white_xyz, policy, &out->allocation)) {
        return false;
    }

    i32 neutral_drives[3];
    solveRgbDrivesQ16(out->allocation.rgb_solve, kGamutD65Q16, neutral_drives);
    for (int i = 0; i < 3; ++i) {
        // As in the three-emitter build: every drive, not just the largest.
        if (neutral_drives[i] <= 0) {
            return false;
        }
    }

    // The three-emitter bound, reached with the white emitter off. Whatever
    // else happens this neutral is attainable: at 1 / max(d0) the RGB drives
    // are exactly in [0, 1] with no white at all.
    i32 largest_neutral_drive = neutral_drives[0];
    for (int i = 1; i < 3; ++i) {
        if (neutral_drives[i] > largest_neutral_drive) {
            largest_neutral_drive = neutral_drives[i];
        }
    }
    i64 reachable = (static_cast<i64>(kGamutFullDrive) << 16) /
                    static_cast<i64>(largest_neutral_drive);

    // Clamped for the same reason the three-emitter build clamps it, and to
    // the same place: `buildRgbSolveMatrixQ16` accepts emitter luminances up
    // to 1e6, so a profile bright enough to reach D65 on one or two raw
    // units of drive puts `2^32 / largest` at or past i32's range -- exactly
    // 2^31 at largest == 2. `optimistic` starts at `kOklabQ16MaxMagnitude`,
    // so for such a profile the bisection below is skipped and the narrowing
    // is reached directly. 64.0 is where the OKLab transform's domain ends,
    // so nothing downstream can tell the difference.
    if (reachable > kOklabQ16MaxMagnitude) {
        reachable = kOklabQ16MaxMagnitude;
    }

    // An upper bound on what the white emitter can add. Along the D65 ray
    // the RGB drives are s * d0 - w * dW, so the *upper* limit on drive i is
    // loosest at w = 1 when dW_i is positive and at w = 0 when it is
    // negative -- hence max(dW_i, 0). Dropping the lower limits, and letting
    // each channel pick its own w, are both relaxations, so this is an upper
    // bound and never an under-estimate.
    i64 optimistic = static_cast<i64>(kOklabQ16MaxMagnitude);
    for (int i = 0; i < 3; ++i) {
        const i32 per_white = out->allocation.per_white[i];
        const i64 numerator = static_cast<i64>(kGamutFullDrive) +
                              (per_white > 0 ? static_cast<i64>(per_white) : 0);
        const i64 candidate =
            (numerator << 16) / static_cast<i64>(neutral_drives[i]);
        if (candidate < optimistic) {
            optimistic = candidate;
        }
    }

    // That upper bound is not generally attainable, and treating it as if it
    // were is a real bug rather than a theoretical one: it enforces only the
    // upper limits on the RGB drives, and full white can push a *different*
    // channel negative. With dW = (0.9, 0.05, 0.05) against
    // d0 = (0.21, 0.72, 0.07) the formula gives 1.468, where the red drive
    // works out at 1.468 * 0.21 - 0.9 = -0.59. Storing that would leave the
    // mapper with a zero-chroma candidate its own halving search cannot
    // satisfy, and the fallback would then return four zero drives for a
    // colour that is not black.
    //
    // So the bound is bisected between the two at bind time. This is not an
    // iterative solver in the A3/B11 sense: it runs once per profile, never
    // per pixel, and the per-pixel path sees only the stored result.
    if (optimistic > reachable) {
        i64 low = reachable;
        i64 high = optimistic;
        for (int step = 0; step < 24; ++step) {
            const i64 middle = (low + high) / 2;
            const i32 trial_scale = static_cast<i32>(middle);
            const i32 trial[3] = {
                scaleGamutQ16(kGamutD65Q16[0], trial_scale),
                scaleGamutQ16(kGamutD65Q16[1], trial_scale),
                scaleGamutQ16(kGamutD65Q16[2], trial_scale),
            };
            i32 trial_drives[4];
            if (allocateEmitterDrivesQ16(out->allocation, trial, trial_drives)) {
                low = middle;
            } else {
                high = middle;
            }
        }
        reachable = low;
    }

    const i32 scale = static_cast<i32>(reachable);
    const i32 brightest_neutral[3] = {
        scaleGamutQ16(kGamutD65Q16[0], scale),
        scaleGamutQ16(kGamutD65Q16[1], scale),
        scaleGamutQ16(kGamutD65Q16[2], scale),
    };
    i32 lab[3];
    xyzToOklabQ16(brightest_neutral, lab);
    out->max_neutral_lightness = lab[0];
    return true;
}

void mapAndAllocateRgbwQ16(const GamutMapRgbwQ16& map, const i32 (&xyz)[3],
                           i32 (&drives)[4]) FL_NO_EXCEPT {
    if (allocateEmitterDrivesQ16(map.allocation, xyz, drives)) {
        // Already inside the device's hull -- which, with a white emitter,
        // is a good deal larger than the RGB one.
        return;
    }

    i32 lab[3];
    xyzToOklabQ16(xyz, lab);
    i32 lightness = lab[0];
    if (lightness < 0) {
        lightness = 0;
    }

    // Same above-cap interval clamp as the RGB path (#4245). The white
    // emitter moves the cap but does not change the shape: above it the
    // feasible chroma still misses zero.
    if (lightness > map.max_neutral_lightness) {
        i32 low_edge = 0;
        i32 high_edge = 0;
        if (feasibleChromaInterval(lab, lightness, RgbwFeasible{map.allocation},
                                   &low_edge, &high_edge)) {
            i32 candidate_xyz[3];
            chromaCandidateXyz(lab, lightness, clampChromaFactor(low_edge, high_edge),
                               candidate_xyz);
            if (allocateEmitterDrivesQ16(map.allocation, candidate_xyz, drives)) {
                return;
            }
        }

        // Same walk-down as the RGB path: dropping straight to the cap when
        // the probes stop landing is a step, not a mapping.
        const i32 reachable = highestReachableLightness(
            lab, map.max_neutral_lightness, lightness,
            RgbwFeasible{map.allocation});
        i32 edge_low = 0;
        i32 edge_high = 0;
        if (reachable > map.max_neutral_lightness &&
            feasibleChromaInterval(lab, reachable, RgbwFeasible{map.allocation},
                                   &edge_low, &edge_high)) {
            i32 edge_xyz[3];
            chromaCandidateXyz(lab, reachable,
                               clampChromaFactor(edge_low, edge_high), edge_xyz);
            if (allocateEmitterDrivesQ16(map.allocation, edge_xyz, drives)) {
                return;
            }
        }
        lightness = map.max_neutral_lightness;
    }

    const i32 factor =
        largestFeasibleChroma(lab, lightness, RgbwFeasible{map.allocation});
    i32 mapped_xyz[3];
    chromaCandidateXyz(lab, lightness, factor, mapped_xyz);
    if (allocateEmitterDrivesQ16(map.allocation, mapped_xyz, drives)) {
        return;
    }
    // The accepted candidate can fall a few ULP outside on the final
    // re-solve. Fall back to the neutral at this lightness, which the
    // lightness bound guarantees is reachable.
    const i32 neutral_lab[3] = {lightness, 0, 0};
    i32 neutral_xyz[3];
    oklabToXyzQ16(neutral_lab, neutral_xyz);
    if (!allocateEmitterDrivesQ16(map.allocation, neutral_xyz, drives)) {
        drives[0] = 0;
        drives[1] = 0;
        drives[2] = 0;
        drives[3] = 0;
    }
}


bool buildGamutMapRgbwwQ16(const colorimetric_response::EmitterProfile& profile,
                           const i32 (&white1_xyz)[3],
                           const i32 (&white2_xyz)[3],
                           WhiteAllocationPolicy policy,
                           GamutMapRgbwwQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    if (!buildTwoWhiteAllocationQ16(profile, white1_xyz, white2_xyz, policy,
                                    &out->allocation)) {
        return false;
    }

    i32 neutral_drives[3];
    solveRgbDrivesQ16(out->allocation.rgb_solve, kGamutD65Q16, neutral_drives);
    for (int i = 0; i < 3; ++i) {
        // As in the other two builds: every drive, not just the largest.
        if (neutral_drives[i] <= 0) {
            return false;
        }
    }

    // The three-emitter bound, reached with both whites off, and always
    // attainable.
    i32 largest_neutral_drive = neutral_drives[0];
    for (int i = 1; i < 3; ++i) {
        if (neutral_drives[i] > largest_neutral_drive) {
            largest_neutral_drive = neutral_drives[i];
        }
    }
    i64 reachable = (static_cast<i64>(kGamutFullDrive) << 16) /
                    static_cast<i64>(largest_neutral_drive);

    // Clamped for the same reason the three-emitter build clamps it, and to
    // the same place: `buildRgbSolveMatrixQ16` accepts emitter luminances up
    // to 1e6, so a profile bright enough to reach D65 on one or two raw
    // units of drive puts `2^32 / largest` at or past i32's range -- exactly
    // 2^31 at largest == 2. `optimistic` starts at `kOklabQ16MaxMagnitude`,
    // so for such a profile the bisection below is skipped and the narrowing
    // is reached directly. 64.0 is where the OKLab transform's domain ends,
    // so nothing downstream can tell the difference.
    if (reachable > kOklabQ16MaxMagnitude) {
        reachable = kOklabQ16MaxMagnitude;
    }

    // The one-white relaxation with a second white added. Along the D65 ray
    // the RGB drives are `s*d0 - w1*dW1 - w2*dW2`, so the upper limit on
    // drive i is loosest with each white at full when its column is positive
    // and off when it is not -- hence a `max(.., 0)` term per white.
    // Dropping the lower limits, and letting each channel choose its own
    // whites, are both relaxations, so this is an upper bound.
    //
    // `per_white1` is not stored: the allocation keeps the second column and
    // the difference, because that is all the per-pixel path needs. It is
    // recovered here rather than widening the struct for a bind-time sum.
    i64 optimistic = static_cast<i64>(kOklabQ16MaxMagnitude);
    for (int i = 0; i < 3; ++i) {
        const i32 per_white2 = out->allocation.per_white2[i];
        const i32 per_white1 = out->allocation.difference[i] + per_white2;
        i64 numerator = static_cast<i64>(kGamutFullDrive);
        if (per_white1 > 0) {
            numerator += static_cast<i64>(per_white1);
        }
        if (per_white2 > 0) {
            numerator += static_cast<i64>(per_white2);
        }
        const i64 candidate =
            (numerator << 16) / static_cast<i64>(neutral_drives[i]);
        if (candidate < optimistic) {
            optimistic = candidate;
        }
    }

    // Bisected against the attainable bound rather than trusted, for exactly
    // the reason spelled out on the one-white path: the relaxation enforces
    // only the upper limits, and full white can push a different channel
    // negative. Once per profile, never per pixel (A3/B11).
    if (optimistic > reachable) {
        i64 low = reachable;
        i64 high = optimistic;
        for (int step = 0; step < 24; ++step) {
            const i64 middle = (low + high) / 2;
            const i32 trial_scale = static_cast<i32>(middle);
            const i32 trial[3] = {
                scaleGamutQ16(kGamutD65Q16[0], trial_scale),
                scaleGamutQ16(kGamutD65Q16[1], trial_scale),
                scaleGamutQ16(kGamutD65Q16[2], trial_scale),
            };
            i32 trial_drives[5];
            if (allocateTwoWhiteDrivesQ16(out->allocation, trial, trial_drives)) {
                low = middle;
            } else {
                high = middle;
            }
        }
        reachable = low;
    }

    const i32 scale = static_cast<i32>(reachable);
    const i32 brightest_neutral[3] = {
        scaleGamutQ16(kGamutD65Q16[0], scale),
        scaleGamutQ16(kGamutD65Q16[1], scale),
        scaleGamutQ16(kGamutD65Q16[2], scale),
    };
    i32 lab[3];
    xyzToOklabQ16(brightest_neutral, lab);
    out->max_neutral_lightness = lab[0];
    return true;
}

void mapAndAllocateRgbwwQ16(const GamutMapRgbwwQ16& map, const i32 (&xyz)[3],
                            i32 (&drives)[5]) FL_NO_EXCEPT {
    if (allocateTwoWhiteDrivesQ16(map.allocation, xyz, drives)) {
        // Already inside the device's hull, which with two whites is larger
        // again than the one-white one.
        return;
    }

    i32 lab[3];
    xyzToOklabQ16(xyz, lab);
    i32 lightness = lab[0];
    if (lightness < 0) {
        lightness = 0;
    }

    // And again for two whites (#4245).
    if (lightness > map.max_neutral_lightness) {
        i32 low_edge = 0;
        i32 high_edge = 0;
        if (feasibleChromaInterval(lab, lightness, RgbwwFeasible{map.allocation},
                                   &low_edge, &high_edge)) {
            i32 candidate_xyz[3];
            chromaCandidateXyz(lab, lightness, clampChromaFactor(low_edge, high_edge),
                               candidate_xyz);
            if (allocateTwoWhiteDrivesQ16(map.allocation, candidate_xyz, drives)) {
                return;
            }
        }

        // Same walk-down as the RGB path: dropping straight to the cap when
        // the probes stop landing is a step, not a mapping.
        const i32 reachable = highestReachableLightness(
            lab, map.max_neutral_lightness, lightness,
            RgbwwFeasible{map.allocation});
        i32 edge_low = 0;
        i32 edge_high = 0;
        if (reachable > map.max_neutral_lightness &&
            feasibleChromaInterval(lab, reachable, RgbwwFeasible{map.allocation},
                                   &edge_low, &edge_high)) {
            i32 edge_xyz[3];
            chromaCandidateXyz(lab, reachable,
                               clampChromaFactor(edge_low, edge_high), edge_xyz);
            if (allocateTwoWhiteDrivesQ16(map.allocation, edge_xyz, drives)) {
                return;
            }
        }
        lightness = map.max_neutral_lightness;
    }

    const i32 factor =
        largestFeasibleChroma(lab, lightness, RgbwwFeasible{map.allocation});
    i32 mapped_xyz[3];
    chromaCandidateXyz(lab, lightness, factor, mapped_xyz);
    if (allocateTwoWhiteDrivesQ16(map.allocation, mapped_xyz, drives)) {
        return;
    }
    // Same last resort as the other paths: the accepted candidate can fall a
    // few ULP outside on the final re-solve, and the neutral at this
    // lightness is guaranteed reachable by the bound above.
    const i32 neutral_lab[3] = {lightness, 0, 0};
    i32 neutral_xyz[3];
    oklabToXyzQ16(neutral_lab, neutral_xyz);
    if (!allocateTwoWhiteDrivesQ16(map.allocation, neutral_xyz, drives)) {
        for (int i = 0; i < 5; ++i) {
            drives[i] = 0;
        }
    }
}

}  // namespace fl

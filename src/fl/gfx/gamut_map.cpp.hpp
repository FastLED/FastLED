// ok no header - implementation for fl/gfx/gamut_map.h

#include "fl/gfx/gamut_map.h"

#include "fl/gfx/oklab_q16.h"

namespace fl {

namespace {

/// Full drive, as an s16.16 raw value.
constexpr i32 kGamutFullDrive = 65536;

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

}  // namespace

bool buildGamutMapQ16(const EmitterProfile& profile, GamutMapQ16* out) FL_NO_EXCEPT {
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

    // Lightness first. Reducing chroma cannot bring an over-bright target
    // back into the hull -- at zero chroma it is still outside -- so without
    // this the halvings below converge on an infeasible answer.
    i32 lightness = lab[0];
    if (lightness > map.max_neutral_lightness) {
        lightness = map.max_neutral_lightness;
    }
    if (lightness < 0) {
        lightness = 0;
    }

    // Then chroma, by scaling (a, b) toward zero. `low` is the largest
    // factor known to be feasible, `high` the smallest known not to be.
    i32 low = 0;
    i32 high = kGamutFullDrive;
    for (int step = 0; step < kGamutMapHalvings; ++step) {
        const i32 factor = (low + high) >> 1;
        const i32 candidate_lab[3] = {
            lightness,
            scaleGamutQ16(lab[1], factor),
            scaleGamutQ16(lab[2], factor),
        };
        i32 candidate_xyz[3];
        oklabToXyzQ16(candidate_lab, candidate_xyz);
        i32 candidate_drives[3];
        solveRgbDrivesQ16(map.solve, candidate_xyz, candidate_drives);
        if (gamutDrivesAreInRange(candidate_drives, 0)) {
            low = factor;
        } else {
            high = factor;
        }
    }

    const i32 mapped_lab[3] = {
        lightness,
        scaleGamutQ16(lab[1], low),
        scaleGamutQ16(lab[2], low),
    };
    i32 mapped_xyz[3];
    oklabToXyzQ16(mapped_lab, mapped_xyz);
    solveRgbDrivesQ16(map.solve, mapped_xyz, drives);
    clampGamutDrives(drives);
}

}  // namespace fl

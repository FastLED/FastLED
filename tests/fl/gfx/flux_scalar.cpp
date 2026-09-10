// Brightness/power amplitude-stage coverage for color pipeline P6 (#4040).

#include "fl/gfx/flux_scalar.h"
#include "fl/stl/int.h"
#include "fl/stl/span.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("Full brightness is bit-exact identity") {
    // 255 must be exactly unity. A rounding step lost per channel on the
    // default path would be a visible regression on every existing sketch.
    FL_CHECK_EQ(FluxScalar::fromBrightness(255).rawQ16(), 65536);

    i32 drives[4] = {65535, 40000, 1, 0};
    const i32 before[4] = {65535, 40000, 1, 0};
    applyFluxScalar(FluxScalar::fromBrightness(255), span<i32>(drives, 4));
    for (int i = 0; i < 4; ++i) {
        FL_CHECK_EQ(drives[i], before[i]);
    }
}

FL_TEST_CASE("Zero brightness drives everything to zero") {
    FL_CHECK_EQ(FluxScalar::fromBrightness(0).rawQ16(), 0);
    i32 drives[3] = {65535, 12345, 7};
    applyFluxScalar(FluxScalar::fromBrightness(0), span<i32>(drives, 3));
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_EQ(drives[i], 0);
    }
}

FL_TEST_CASE("Scaling preserves channel ratios, which is what keeps hue fixed") {
    // One scalar for every channel means chromaticity cannot move except by
    // per-channel rounding, which is bounded by half a step.
    i32 drives[3] = {60000, 30000, 15000};
    applyFluxScalar(FluxScalar::fromRawQ16(16384), span<i32>(drives, 3));  // 0.25

    FL_CHECK_EQ(drives[0], 15000);
    FL_CHECK_EQ(drives[1], 7500);
    FL_CHECK_EQ(drives[2], 3750);
    // Ratios survive exactly here; in general they move by at most rounding.
    FL_CHECK_EQ(drives[0], drives[1] * 2);
    FL_CHECK_EQ(drives[1], drives[2] * 2);
}

FL_TEST_CASE("Composition is not associative, and the header says so") {
    // Q16 rounding breaks associativity. Pinned because the API previously
    // claimed the opposite, and a caller chaining three scalars would be
    // relying on something that does not hold.
    const FluxScalar a = FluxScalar::fromRawQ16(1);
    const FluxScalar b = FluxScalar::fromRawQ16(32768);
    const FluxScalar c = FluxScalar::fromRawQ16(32768);
    FL_CHECK_EQ(a.composedWith(b).composedWith(c).rawQ16(), 1);
    FL_CHECK_EQ(a.composedWith(b.composedWith(c)).rawQ16(), 0);
}

FL_TEST_CASE("Composition is multiplicative and order-independent") {
    const FluxScalar half = FluxScalar::fromRawQ16(32768);
    const FluxScalar quarter = FluxScalar::fromRawQ16(16384);

    FL_CHECK_EQ(half.composedWith(quarter).rawQ16(), 8192);          // 1/8
    FL_CHECK_EQ(quarter.composedWith(half).rawQ16(), 8192);          // commutative
    FL_CHECK_EQ(half.composedWith(FluxScalar::unity()).rawQ16(), 32768);
    FL_CHECK_EQ(FluxScalar::unity().composedWith(half).rawQ16(), 32768);
}

FL_TEST_CASE("A limiter fraction is clamped into [0, 1]") {
    FL_CHECK_EQ(FluxScalar::fromRawQ16(-5).rawQ16(), 0);
    FL_CHECK_EQ(FluxScalar::fromRawQ16(70000).rawQ16(), 65536);
    FL_CHECK_EQ(FluxScalar::fromRawQ16(16384).rawQ16(), 16384);
}

FL_TEST_CASE("Brightness is a linear flux scalar, not a gamma curve") {
    // b/255 exactly: half brightness is half the light, which is what makes
    // the stage composable with the power limiter by plain multiplication.
    // Every code is within half a step of b * 65536 / 255.
    for (int b = 0; b <= 255; ++b) {
        const i32 got = FluxScalar::fromBrightness(static_cast<u8>(b)).rawQ16();
        // exact * 2, to compare without floating point
        const i64 twice_exact = (static_cast<i64>(b) * 65536 * 2 + 255) / 255;
        const i64 twice_got = static_cast<i64>(got) * 2;
        const i64 error = twice_got > twice_exact ? twice_got - twice_exact
                                                  : twice_exact - twice_got;
        FL_CHECK(error <= 1);
    }
    // 1 and 254 are exact multiples, so they pin the endpoints of the ramp.
    FL_CHECK_EQ(FluxScalar::fromBrightness(1).rawQ16(), 257);
    FL_CHECK_EQ(FluxScalar::fromBrightness(254).rawQ16(), 65279);
}

FL_TEST_CASE("Signed wide intermediates scale symmetrically") {
    // Signed values are legal in the working domain before gamut mapping,
    // so the stage must not bias negatives differently from positives.
    i32 drives[2] = {40000, -40000};
    applyFluxScalar(FluxScalar::fromRawQ16(32768), span<i32>(drives, 2));
    FL_CHECK_EQ(drives[0], 20000);
    FL_CHECK_EQ(drives[1], -20000);
}

namespace {

/// A synthetic emitter response: light emitted for a given drive.
///
/// R2's counterexample exactly -- `F_R(d) = d` and `F_G(d) = d^2`, in s16.16.
/// Two emitters whose code-to-light curves differ is the whole point; a
/// device where they matched would show nothing.
i32 emittedLightQ16(int channel, i32 drive) {
    if (channel == 0) {
        return drive;
    }
    const i64 squared = static_cast<i64>(drive) * static_cast<i64>(drive);
    return static_cast<i32>((squared + 32768) >> 16);
}

/// The drive that emits a given light, i.e. `F^-1`.
i32 driveForLightQ16(int channel, i32 light) {
    if (channel == 0) {
        return light;
    }
    // Integer square root in Q16: sqrt(light) with the scale carried through.
    i64 low = 0;
    i64 high = 65536;
    while (low < high) {
        const i64 mid = (low + high + 1) / 2;
        if (((mid * mid + 32768) >> 16) <= light) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }
    return static_cast<i32>(low);
}

}  // namespace

FL_TEST_CASE("A shared scalar is only chromaticity-preserving on linear quantities") {
    // FastLED#4156 R2. P2 permits a per-channel nonlinear code-to-light
    // response; a shared brightness scalar preserves chromaticity only while
    // the quantities it scales are linear light. This is that argument as
    // arithmetic rather than prose, so whoever implements response
    // compensation has the ordering constraint in a form that fails.
    //
    // Two emitters, F_R(d) = d and F_G(d) = d^2. An equal-light target at
    // full output uses drives (1, 1).
    const i32 kOne = 65536;
    const FluxScalar quarter = FluxScalar::fromRawQ16(kOne / 4);

    // Wrong order: scale the compensated drives.
    i32 wrong[2] = {kOne, kOne};
    applyFluxScalar(quarter, span<i32>(wrong, 2));
    const i32 wrong_light[2] = {emittedLightQ16(0, wrong[0]),
                                emittedLightQ16(1, wrong[1])};

    // Right order: scale the light, then invert the response.
    i32 light[2] = {emittedLightQ16(0, kOne), emittedLightQ16(1, kOne)};
    applyFluxScalar(quarter, span<i32>(light, 2));
    const i32 right_drives[2] = {driveForLightQ16(0, light[0]),
                                 driveForLightQ16(1, light[1])};
    const i32 right_light[2] = {emittedLightQ16(0, right_drives[0]),
                                emittedLightQ16(1, right_drives[1])};

    // The numbers R2 gives. Scaling drives lands on light (1/4, 1/16) --
    // a four-to-one ratio where the target was one to one.
    FL_CHECK_EQ(wrong[0], kOne / 4);
    FL_CHECK_EQ(wrong[1], kOne / 4);
    FL_CHECK_EQ(wrong_light[0], kOne / 4);
    FL_CHECK_EQ(wrong_light[1], kOne / 16);

    // Scaling light lands on drives (1/4, 1/2) and light (1/4, 1/4).
    FL_CHECK_EQ(right_drives[0], kOne / 4);
    FL_CHECK_EQ(right_drives[1], kOne / 2);
    FL_CHECK_EQ(right_light[0], kOne / 4);
    FL_CHECK_EQ(right_light[1], kOne / 4);

    // Stated as the property rather than as four constants: the ratio the
    // target asked for survives one order and not the other.
    FL_CHECK_EQ(right_light[0], right_light[1]);
    FL_CHECK_NE(wrong_light[0], wrong_light[1]);
}

FL_TEST_CASE("The shipped order scales linear drives, which is the safe one") {
    // The other half, and the reason R2 is not a live defect. What
    // `processPixelQ16` hands to `applyFluxScalar` is the device solve's
    // output, and the emitter matrix it solves against is linear -- so the
    // quantity being scaled is emitter light and the counterexample above
    // cannot arise.
    //
    // A per-channel response would change that, and `EmitterProfile` already
    // carries `response_lut_r/g/b`. Those are validated on bind and applied
    // by nothing; `tests/fl/channels/color_profile.cpp` pins that. When they
    // are applied, the case above says where.
    //
    // Linearity, checked directly: scaling by a half twice is scaling by a
    // quarter once, which is only true of a linear quantity.
    i32 twice[3] = {60000, 40000, 12345};
    applyFluxScalar(FluxScalar::fromRawQ16(32768), span<i32>(twice, 3));
    applyFluxScalar(FluxScalar::fromRawQ16(32768), span<i32>(twice, 3));

    i32 once[3] = {60000, 40000, 12345};
    applyFluxScalar(FluxScalar::fromRawQ16(16384), span<i32>(once, 3));

    for (int i = 0; i < 3; ++i) {
        // Within one raw unit, which is the two roundings rather than a
        // curve. A square-law stage would put these thousands apart.
        const i32 difference = twice[i] - once[i];
        FL_CHECK_LT(difference, 2);
        FL_CHECK_GT(difference, -2);
    }

    // And the same operands through a square law, so the check above is
    // shown to be capable of separating the two.
    const i32 squared_twice = emittedLightQ16(1, emittedLightQ16(1, 60000));
    const i32 squared_once = emittedLightQ16(1, 60000);
    FL_CHECK_GT(squared_once - squared_twice, 1000);
}

}  // FL_TEST_FILE

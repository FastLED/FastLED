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

}  // FL_TEST_FILE

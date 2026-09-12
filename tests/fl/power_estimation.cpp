
#include "test.h"
#include "FastLED.h"
#include "power_mgt.h"
#include "fl/channels/cled_controller.h"

using namespace fl;

// NOTE: LED controllers accumulate across all TEST_CASEs in this file due to
// FastLED being a singleton. Each addLeds() call adds a new controller to the
// global list. This is intentional FastLED behavior and tests account for it
// by using relative comparisons rather than absolute power values.

// Static LED arrays to ensure they outlive the controllers that reference them
// Each test case uses its own static array to prevent use-after-scope issues
static CRGB gLeds0[10];    // For basic smoke test
static CRGB gLeds1[10];    // For brightness scaling
static CRGB gLeds2[10];    // For no power limiting
static CRGB gLeds3[100];   // For with power limiting
static CRGB gLeds4[10];    // For zero brightness
static CRGB gLeds5[10];    // For high power limit
static CRGB gLeds6[50];    // For brightness scaling with limiting

// Fixture to reset power model to default around each test.
// This prevents power_mgt.cpp tests from affecting power_estimation.cpp tests,
// and the destructor prevents non-linear model state set mid-test from leaking
// into any subsequent test in the process.
struct PowerEstimationFixture {
    PowerEstimationFixture() {
        reset_to_default();
    }
    ~PowerEstimationFixture() {
        reset_to_default();
    }

    static void reset_to_default() {
        // Reset power model to WS2812 @ 5V default (80, 55, 75, 5) with linear
        // response; set_power_model rebuilds the scaling LUTs from the model's
        // exponent field so this is sufficient to restore linear behavior.
        set_power_model(PowerModelRGB());
    }
};

// Simple test to verify the function exists and returns a reasonable value
FL_TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - basic smoke test") {
    fill_solid(gLeds0, 10, CRGB::Black);

    // Initialize FastLED with a single controller
    FastLED.addLeds<WS2812, 0, GRB>(gLeds0, 10);
    FastLED.setBrightness(255);

    // Should not crash
    uint32_t power = FastLED.getEstimatedPowerInMilliWatts();

    // With all LEDs off, power should only include dark LED power (5mW * 10 = 50mW)
    // MCU power is NOT included - caller must add platform-specific MCU power
    FL_REQUIRE(power >= 0);
    FL_REQUIRE(power < 10000);  // Sanity check

    // Typical usage: add MCU power separately based on platform
    const uint32_t mcu_power_mW = 25 * 5;  // 25mA @ 5V = 125mW (Arduino Uno example)
    uint32_t total_power = power + mcu_power_mW;
    FL_REQUIRE(total_power >= 125);
}

// Test brightness scaling
FL_TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - brightness scaling") {
    fill_solid(gLeds1, 10, CRGB(255, 255, 255));  // All white

    FastLED.addLeds<WS2812, 1, GRB>(gLeds1, 10);

    // Full brightness
    FastLED.setBrightness(255);
    uint32_t power_full = FastLED.getEstimatedPowerInMilliWatts();

    // Half brightness
    FastLED.setBrightness(128);
    uint32_t power_half = FastLED.getEstimatedPowerInMilliWatts();

    // Zero brightness
    FastLED.setBrightness(0);
    uint32_t power_zero = FastLED.getEstimatedPowerInMilliWatts();

    // Verify scaling relationship
    FL_REQUIRE(power_full > power_half);
    FL_REQUIRE(power_half > power_zero);

    // Not zero. This asserted `power_zero == 0` and that was the defect in
    // #4156 R4: every controller IC draws its quiescent current whether or not
    // an emitter is lit, so brightness zero is a dark strip and 10 x dark_mW
    // of draw, not an unpowered one. The estimate used to multiply that
    // baseline by brightness along with the emitters, which is also why the
    // limiter could hand back a brightness that went over budget.
    // Controllers accumulate across cases in this file, so the baseline here
    // covers at least this case's ten LEDs and possibly earlier ones.
    const uint32_t baseline_mW = get_power_model().dark_mW * 10u;
    FL_REQUIRE(baseline_mW > 0);
    FL_REQUIRE(power_zero >= baseline_mW);
    FL_REQUIRE(power_zero % get_power_model().dark_mW == 0);
}

// Test power estimation without power limiting
FL_TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - no power limiting") {
    fill_solid(gLeds2, 10, CRGB(255, 255, 255));  // All white

    FastLED.addLeds<WS2812, 2, GRB>(gLeds2, 10);
    FastLED.setBrightness(255);

    // Without power limiting, limited and unlimited should be the same
    uint32_t with_limiter = FastLED.getEstimatedPowerInMilliWatts(true);
    uint32_t without_limiter = FastLED.getEstimatedPowerInMilliWatts(false);

    FL_REQUIRE(with_limiter == without_limiter);
    FL_REQUIRE(with_limiter > 0);  // Should have some power with white LEDs
}

// Test power estimation with power limiting enabled
FL_TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - with power limiting") {
    fill_solid(gLeds3, 100, CRGB(255, 255, 255));  // All white - high power demand

    FastLED.addLeds<WS2812, 3, GRB>(gLeds3, 100);
    FastLED.setBrightness(255);

    // Set a low power limit (1000mW) - should force brightness reduction
    FastLED.setMaxPowerInMilliWatts(1000);

    uint32_t with_limiter = FastLED.getEstimatedPowerInMilliWatts(true);     // Actual power
    uint32_t without_limiter = FastLED.getEstimatedPowerInMilliWatts(false);  // Requested power

    // Limited power should be less than unlimited power due to limiting
    FL_REQUIRE(with_limiter < without_limiter);

    // Limited power should be significantly reduced
    // Note: power limiter includes MCU power (125mW) in calculations, so LED-only
    // limited power will be within ~(limit - MCU) with some rounding tolerance
    FL_REQUIRE(with_limiter < without_limiter * 0.9);  // At least 10% reduction
}

// Test power estimation - edge case with zero brightness
FL_TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - zero brightness") {
    fill_solid(gLeds4, 10, CRGB(255, 255, 255));

    FastLED.addLeds<WS2812, 4, GRB>(gLeds4, 10);
    FastLED.setBrightness(0);
    FastLED.setMaxPowerInMilliWatts(1000);

    uint32_t with_limiter = FastLED.getEstimatedPowerInMilliWatts(true);
    uint32_t without_limiter = FastLED.getEstimatedPowerInMilliWatts(false);

    // Both are the dark-current baseline at zero brightness, not zero: the
    // strip is dark, not unpowered (#4156 R4). The limiter has nothing to do
    // here either, since the baseline is well inside the 1000 mW budget.
    // Controllers accumulate across cases in this file, so this is at least
    // this case's ten LEDs of dark current.
    const uint32_t baseline_mW = get_power_model().dark_mW * 10u;
    FL_REQUIRE(baseline_mW > 0);
    FL_REQUIRE(with_limiter >= baseline_mW);
    FL_REQUIRE(without_limiter == with_limiter);
}

// Test power estimation - power limit high enough to not limit
FL_TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - high power limit (no limiting)") {
    fill_solid(gLeds5, 10, CRGB(255, 255, 255));

    FastLED.addLeds<WS2812, 5, GRB>(gLeds5, 10);
    FastLED.setBrightness(255);

    // Set a very high power limit (100W) - should not cause limiting
    FastLED.setMaxPowerInMilliWatts(100000);

    uint32_t with_limiter = FastLED.getEstimatedPowerInMilliWatts(true);
    uint32_t without_limiter = FastLED.getEstimatedPowerInMilliWatts(false);

    // With high enough limit, both should be very close
    // Allow tolerance for:
    // 1. Integer division rounding in scale32by8 (divides by 256 instead of 255)
    // 2. Controller accumulation from previous tests in this file
    uint32_t diff = (with_limiter > without_limiter) ? (with_limiter - without_limiter) : (without_limiter - with_limiter);
    FL_REQUIRE(diff <= 500);  // Within 500mW tolerance (about 2% for typical cases)
}

// Test power estimation - brightness scaling with limiting
FL_TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - brightness scaling with limiting") {
    fill_solid(gLeds6, 50, CRGB(200, 200, 200));

    FastLED.addLeds<WS2812, 6, GRB>(gLeds6, 50);
    FastLED.setMaxPowerInMilliWatts(5000);  // Higher limit to allow brightness scaling

    // Test at different brightness levels
    FastLED.setBrightness(255);
    uint32_t power_full = FastLED.getEstimatedPowerInMilliWatts(true);

    FastLED.setBrightness(128);
    uint32_t power_half = FastLED.getEstimatedPowerInMilliWatts(true);

    FastLED.setBrightness(64);
    uint32_t power_quarter = FastLED.getEstimatedPowerInMilliWatts(true);

    // Power should scale with brightness (or hit the limit)
    // Use tolerance to account for integer rounding in scale32by8
    int32_t diff_full_half = (int32_t)power_full - (int32_t)power_half;
    int32_t diff_half_quarter = (int32_t)power_half - (int32_t)power_quarter;

    FL_REQUIRE(diff_full_half >= -10);  // Allow 10mW rounding tolerance
    FL_REQUIRE(diff_half_quarter >= -10);

    // All should be reasonably close to power limit
    // Note: getEstimatedPowerInMilliWatts() returns LED-only power (excludes MCU ~125mW)
    // The power limiter includes MCU, so LED power might approach (limit - MCU)
    // Allow tolerance for:
    //   - MCU power offset (~125mW)
    //   - Controller accumulation from previous tests
    //   - Integer rounding in scale32by8
    const uint32_t tolerance = 500;  // 500mW tolerance (~10%)
    FL_REQUIRE(power_full <= 5000 + tolerance);
    FL_REQUIRE(power_half <= 5000 + tolerance);
    FL_REQUIRE(power_quarter <= 5000 + tolerance);
}

FL_TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - non linear scaling raises mid brightness estimate") {
    fill_solid(gLeds1, 10, CRGB(255, 255, 255));

    FastLED.addLeds<WS2812, 7, GRB>(gLeds1, 10);
    FastLED.setBrightness(128);

    uint32_t linear_power = FastLED.getEstimatedPowerInMilliWatts(false);

    PowerModelRGB nonlinear_model = FastLED.getPowerModel();
    nonlinear_model.exponent = 0.87f;
    FastLED.setPowerModel(nonlinear_model);
    uint32_t nonlinear_power = FastLED.getEstimatedPowerInMilliWatts(false);

    FL_REQUIRE(nonlinear_power > linear_power);
}

// ---------------------------------------------------------------------------
// #4342: the estimate reads pre-dither pixels, and dither only ever adds.
//
// `docs/color-pipeline-contracts.md` requires the prepass to "include response
// inversion, current-field policy, and quantization reserve". The reserve is
// unimplemented, and this measures what that costs while it stays that way.
//
// Nothing pinned it before. The figure in #4342 was a one-off measurement; a
// regression that made the gap worse -- or a fix that closed it -- would move
// silently either way.
// ---------------------------------------------------------------------------

namespace {

const int kDitherCycle = 8;

const int kStripLen = 200;

/// Power of a uniform strip as the limiter would score it.
///
/// A strip rather than one LED on purpose. `calculate_unscaled_power_mW`
/// returns whole milliwatts, and a single dim pixel lands on 3 against 8 --
/// a ratio whose precision is the estimator's rounding, not the effect. At
/// 200 LEDs the quantum is a fraction of a percent.
fl::u32 powerOfStrip(CRGB pixel) {
    fl::vector<CRGB> strip;
    for (int i = 0; i < kStripLen; ++i) {
        strip.push_back(pixel);
    }
    return calculate_unscaled_power_mW(strip.data(), kStripLen);
}

/// The strip's draw with dithering off -- the pixel the estimate is built on.
fl::u32 unditheredDraw(CRGB source, fl::u8 brightness) {
    CRGB pixel = source;
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(brightness, brightness, brightness);
    PixelController<RGB> pixels(&pixel, 1, adjustment, DISABLE_DITHER);
    const CRGB emitted(pixels.loadAndScale0(), pixels.loadAndScale1(),
                       pixels.loadAndScale2());
    return powerOfStrip(emitted);
}

/// The largest single-frame draw over a full dither cycle at `brightness`.
///
/// The phase must advance between frames. Building a fresh `PixelController`
/// without advancing it renders the same phase eight times and reports a
/// milder gap -- #4342 records measuring +25% that way before finding +50%.
fl::u32 worstDitheredFrame(CRGB source, fl::u8 brightness) {
    fl::u32 worst = 0;
    for (int frame = 0; frame < kDitherCycle; ++frame) {
        fl::detail::advanceDitherFrame();
        CRGB pixel = source;
        ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
        adjustment.premixed = CRGB(brightness, brightness, brightness);
        PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
        const CRGB emitted(pixels.loadAndScale0(), pixels.loadAndScale1(),
                           pixels.loadAndScale2());
        // Every LED on the strip carries the same source, so the whole strip
        // takes the same dither offset in a given frame.
        const fl::u32 drawn = powerOfStrip(emitted);
        if (drawn > worst) {
            worst = drawn;
        }
    }
    return worst;
}

}  // namespace

FL_TEST_CASE("[#4342] the power estimate misses what dithering adds") {
    // Both sides through `calculate_unscaled_power_mW`, dither off against
    // dither on, on the same 200-LED strip. That is what makes it
    // like-for-like. Powering the source and calling
    // `scale_power_for_brightness` -- what the real limiter does -- treats the
    // estimator's per-LED idle term differently from powering an already
    // scaled pixel, and reports 2.4x where the dither accounts for a fraction
    // of it. #4342 makes the same caveat about its own method.
    const fl::u8 brightness = 16;

    // Measured, at brightness 16, 200 LEDs:
    //
    //   source   dither off   worst frame   ratio
    //        1         1000          1162   1.162
    //        8         1000          1162   1.162
    //       16         1162          1327   1.142
    //       64         1655          1818   1.098
    //
    // The 1000 mW floor is the idle term for 200 dark LEDs, and the dither
    // adds a near-constant ~162 mW on top -- it lifts channels off zero, and
    // how far it lifts them does not depend much on how dim the source is.
    // So the *relative* error is worst exactly where a limiter operates.
    const CRGB dim(8, 8, 8);
    const fl::u32 off = unditheredDraw(dim, brightness);
    const fl::u32 on = worstDitheredFrame(dim, brightness);

    // Dither only ever adds: `dither()` is `b ? qadd8(b, d) : 0`, so it raises
    // a lit channel and never lowers one. The estimate cannot come out high.
    FL_CHECK_GE(on, off);

    // And the excess is not a rounding artifact. 15% of the strip's draw is
    // unaccounted for at this operating point.
    FL_CHECK_GT(on * 100, off * 115);

    // Non-zero, or the comparison is two zeroes agreeing.
    FL_CHECK_GT(off, 0u);
}

FL_TEST_CASE("[#4342] the unaccounted draw matters most where the signal is small") {
    // The shape, and why this is a low-light problem. The dither's addition is
    // roughly fixed; the signal it sits on is not. So the fraction it
    // represents falls as the source rises -- 1.162 at source 8 against 1.098
    // at source 64 -- and a limiter pushing brightness down is walking toward
    // the worse end, not away from it.
    const fl::u8 brightness = 16;

    const fl::u32 dim_off = unditheredDraw(CRGB(8, 8, 8), brightness);
    const fl::u32 dim_on = worstDitheredFrame(CRGB(8, 8, 8), brightness);
    const fl::u32 bright_off = unditheredDraw(CRGB(64, 64, 64), brightness);
    const fl::u32 bright_on = worstDitheredFrame(CRGB(64, 64, 64), brightness);

    const double dim_ratio =
        static_cast<double>(dim_on) / static_cast<double>(dim_off);
    const double bright_ratio =
        static_cast<double>(bright_on) / static_cast<double>(bright_off);

    FL_CHECK_GT(dim_ratio, bright_ratio);

    // Both above one, so the ordering above is between two real excesses
    // rather than between two roundings.
    FL_CHECK_GT(dim_ratio, 1.10);
    FL_CHECK_GT(bright_ratio, 1.05);
}

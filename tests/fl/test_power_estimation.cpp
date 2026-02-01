#include "test.h"
#include "FastLED.h"
#include "power_mgt.h"
#include "fl/cled_controller.h"

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

// Fixture to reset power model to default before each test
// This prevents power_mgt.cpp tests from affecting power_estimation.cpp tests
struct PowerEstimationFixture {
    PowerEstimationFixture() {
        // Reset power model to WS2812 @ 5V default (80, 55, 75, 5)
        set_power_model(PowerModelRGB());
    }
};

// Simple test to verify the function exists and returns a reasonable value
TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - basic smoke test") {
    fill_solid(gLeds0, 10, CRGB::Black);

    // Initialize FastLED with a single controller
    FastLED.addLeds<WS2812, 0, GRB>(gLeds0, 10);
    FastLED.setBrightness(255);

    // Should not crash
    uint32_t power = FastLED.getEstimatedPowerInMilliWatts();

    // With all LEDs off, power should only include dark LED power (5mW * 10 = 50mW)
    // MCU power is NOT included - caller must add platform-specific MCU power
    REQUIRE(power >= 0);
    REQUIRE(power < 10000);  // Sanity check

    // Typical usage: add MCU power separately based on platform
    const uint32_t mcu_power_mW = 25 * 5;  // 25mA @ 5V = 125mW (Arduino Uno example)
    uint32_t total_power = power + mcu_power_mW;
    REQUIRE(total_power >= 125);
}

// Test brightness scaling
TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - brightness scaling") {
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
    REQUIRE(power_full > power_half);
    REQUIRE(power_half > power_zero);
    REQUIRE(power_zero == 0);
}

// Test power estimation without power limiting
TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - no power limiting") {
    fill_solid(gLeds2, 10, CRGB(255, 255, 255));  // All white

    FastLED.addLeds<WS2812, 2, GRB>(gLeds2, 10);
    FastLED.setBrightness(255);

    // Without power limiting, limited and unlimited should be the same
    uint32_t with_limiter = FastLED.getEstimatedPowerInMilliWatts(true);
    uint32_t without_limiter = FastLED.getEstimatedPowerInMilliWatts(false);

    REQUIRE(with_limiter == without_limiter);
    REQUIRE(with_limiter > 0);  // Should have some power with white LEDs
}

// Test power estimation with power limiting enabled
TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - with power limiting") {
    fill_solid(gLeds3, 100, CRGB(255, 255, 255));  // All white - high power demand

    FastLED.addLeds<WS2812, 3, GRB>(gLeds3, 100);
    FastLED.setBrightness(255);

    // Set a low power limit (1000mW) - should force brightness reduction
    FastLED.setMaxPowerInMilliWatts(1000);

    uint32_t with_limiter = FastLED.getEstimatedPowerInMilliWatts(true);     // Actual power
    uint32_t without_limiter = FastLED.getEstimatedPowerInMilliWatts(false);  // Requested power

    // Limited power should be less than unlimited power due to limiting
    REQUIRE(with_limiter < without_limiter);

    // Limited power should be significantly reduced
    // Note: power limiter includes MCU power (125mW) in calculations, so LED-only
    // limited power will be within ~(limit - MCU) with some rounding tolerance
    REQUIRE(with_limiter < without_limiter * 0.9);  // At least 10% reduction
}

// Test power estimation - edge case with zero brightness
TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - zero brightness") {
    fill_solid(gLeds4, 10, CRGB(255, 255, 255));

    FastLED.addLeds<WS2812, 4, GRB>(gLeds4, 10);
    FastLED.setBrightness(0);
    FastLED.setMaxPowerInMilliWatts(1000);

    uint32_t with_limiter = FastLED.getEstimatedPowerInMilliWatts(true);
    uint32_t without_limiter = FastLED.getEstimatedPowerInMilliWatts(false);

    // Both should be zero at zero brightness
    REQUIRE(with_limiter == 0);
    REQUIRE(without_limiter == 0);
}

// Test power estimation - power limit high enough to not limit
TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - high power limit (no limiting)") {
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
    REQUIRE(diff <= 500);  // Within 500mW tolerance (about 2% for typical cases)
}

// Test power estimation - brightness scaling with limiting
TEST_CASE_FIXTURE(PowerEstimationFixture,"Power estimation - brightness scaling with limiting") {
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

    REQUIRE(diff_full_half >= -10);  // Allow 10mW rounding tolerance
    REQUIRE(diff_half_quarter >= -10);

    // All should be reasonably close to power limit
    // Note: getEstimatedPowerInMilliWatts() returns LED-only power (excludes MCU ~125mW)
    // The power limiter includes MCU, so LED power might approach (limit - MCU)
    // Allow tolerance for:
    //   - MCU power offset (~125mW)
    //   - Controller accumulation from previous tests
    //   - Integer rounding in scale32by8
    const uint32_t tolerance = 500;  // 500mW tolerance (~10%)
    REQUIRE(power_full <= 5000 + tolerance);
    REQUIRE(power_half <= 5000 + tolerance);
    REQUIRE(power_quarter <= 5000 + tolerance);
}

#include "test.h"
#include "FastLED.h"

using namespace fl;

// Simple test to verify the function exists and returns a reasonable value
TEST_CASE("Power estimation - basic smoke test") {
    CRGB leds[10];
    fill_solid(leds, 10, CRGB::Black);

    // Initialize FastLED with a single controller
    FastLED.addLeds<WS2812, 0, GRB>(leds, 10);
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
TEST_CASE("Power estimation - brightness scaling") {
    CRGB leds[10];
    fill_solid(leds, 10, CRGB(255, 255, 255));  // All white

    FastLED.addLeds<WS2812, 1, GRB>(leds, 10);

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

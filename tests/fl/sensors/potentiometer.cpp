/// @file potentiometer.cpp
/// @brief Unit tests for Potentiometer class

#include "fl/sensors/potentiometer.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"

using namespace fl;

TEST_CASE("Potentiometer - raw value reading") {
    Potentiometer pot(0);

    // Inject test values directly
    pot.injectTestValue(500);
    CHECK(pot.raw() == 500);

    pot.injectTestValue(750);
    CHECK(pot.raw() == 750);
}

TEST_CASE("Potentiometer - normalized conversion (full range)") {
    Potentiometer pot(0);
    // Default calibration: 0-1023 (10-bit ADC on stub platform)

    // Test minimum
    pot.injectTestValue(0);
    CHECK(pot.normalized() == 0.0f);

    // Test midpoint
    pot.injectTestValue(512);
    CHECK(pot.normalized() == doctest::Approx(512.0f / 1023.0f).epsilon(0.001));

    // Test maximum
    pot.injectTestValue(1023);
    CHECK(pot.normalized() == 1.0f);
}

TEST_CASE("Potentiometer - fractional16 conversion (full range)") {
    Potentiometer pot(0);

    // Test minimum
    pot.injectTestValue(0);
    CHECK(pot.fractional16() == 0);

    // Test midpoint
    pot.injectTestValue(512);
    uint16_t expected = (uint32_t(512) * 65535U) / 1023U;
    CHECK(pot.fractional16() == expected);

    // Test maximum
    pot.injectTestValue(1023);
    CHECK(pot.fractional16() == 65535);
}

TEST_CASE("Potentiometer - calibration range setRange()") {
    Potentiometer pot(0);

    // Set calibration range: 100-900 maps to [0.0, 1.0]
    pot.setRange(100, 900);

    // Test below minimum (should clamp to 0.0)
    pot.injectTestValue(50);
    CHECK(pot.normalized() == 0.0f);

    // Test at minimum
    pot.injectTestValue(100);
    CHECK(pot.normalized() == 0.0f);

    // Test midpoint
    pot.injectTestValue(500);
    float expected = (500.0f - 100.0f) / (900.0f - 100.0f);
    CHECK(pot.normalized() == doctest::Approx(expected).epsilon(0.001));

    // Test at maximum
    pot.injectTestValue(900);
    CHECK(pot.normalized() == 1.0f);

    // Test above maximum (should clamp to 1.0)
    pot.injectTestValue(1023);
    CHECK(pot.normalized() == 1.0f);
}

TEST_CASE("Potentiometer - calibrateMin/calibrateMax") {
    Potentiometer pot(0);

    // Move to minimum position and calibrate
    pot.injectTestValue(150);
    pot.calibrateMin();
    CHECK(pot.getRangeMin() == 150);

    // Move to maximum position and calibrate
    pot.injectTestValue(850);
    pot.calibrateMax();
    CHECK(pot.getRangeMax() == 850);

    // Verify normalized range now uses calibrated values
    pot.injectTestValue(150);
    CHECK(pot.normalized() == 0.0f);

    pot.injectTestValue(850);
    CHECK(pot.normalized() == 1.0f);

    pot.injectTestValue(500);
    float expected = (500.0f - 150.0f) / (850.0f - 150.0f);
    CHECK(pot.normalized() == doctest::Approx(expected).epsilon(0.001));
}

TEST_CASE("Potentiometer - resetCalibration") {
    Potentiometer pot(0);

    // Set custom range
    pot.setRange(200, 800);
    CHECK(pot.getRangeMin() == 200);
    CHECK(pot.getRangeMax() == 800);

    // Reset to full ADC range
    pot.resetCalibration();
    CHECK(pot.getRangeMin() == 0);
    CHECK(pot.getRangeMax() == 1023);  // 10-bit ADC on stub platform
}

TEST_CASE("Potentiometer - fractional16 with calibration") {
    Potentiometer pot(0);
    pot.setRange(100, 900);

    // Test minimum
    pot.injectTestValue(100);
    CHECK(pot.fractional16() == 0);

    // Test maximum
    pot.injectTestValue(900);
    CHECK(pot.fractional16() == 65535);

    // Test midpoint
    pot.injectTestValue(500);
    uint16_t expected = (uint32_t(500 - 100) * 65535U) / (900 - 100);
    CHECK(pot.fractional16() == expected);
}

TEST_CASE("Potentiometer - hysteresis default calculation") {
    Potentiometer pot(0);

    // Default hysteresis: 1% of range or 10, whichever is larger
    // Full range: 0-1023, 1% = 10.23, so should be 10
    uint16_t expected_hyst = 10;
    CHECK(pot.getHysteresis() == expected_hyst);
}

TEST_CASE("Potentiometer - setHysteresis") {
    Potentiometer pot(0);

    pot.setHysteresis(50);
    CHECK(pot.getHysteresis() == 50);
}

TEST_CASE("Potentiometer - setHysteresisPercent") {
    Potentiometer pot(0);
    pot.setRange(0, 1000);

    // 5% of 1000 = 50
    pot.setHysteresisPercent(5.0f);
    CHECK(pot.getHysteresis() == 50);

    // 10% of 1000 = 100
    pot.setHysteresisPercent(10.0f);
    CHECK(pot.getHysteresis() == 100);
}

TEST_CASE("Potentiometer - onChange callback") {
    Potentiometer pot(0);
    pot.setHysteresis(50);  // Require 50 ADC counts change

    int callback_count = 0;
    uint16_t last_raw = 0;

    fl::function<void(Potentiometer&)> callback = [&](Potentiometer& p) {
        callback_count++;
        last_raw = p.raw();
    };
    pot.onChange(callback);

    // Initial value
    pot.injectTestValue(500);
    CHECK(callback_count == 1);  // First change triggers callback
    CHECK(last_raw == 500);

    // Small change (within hysteresis) - should NOT trigger
    pot.injectTestValue(520);
    CHECK(callback_count == 1);  // No change

    // Large change (beyond hysteresis) - should trigger
    pot.injectTestValue(600);
    CHECK(callback_count == 2);
    CHECK(last_raw == 600);
}

TEST_CASE("Potentiometer - onChange normalized callback") {
    Potentiometer pot(0);
    pot.setHysteresis(50);

    int callback_count = 0;
    float last_normalized = 0.0f;

    fl::function<void(float)> callback = [&](float normalized) {
        callback_count++;
        last_normalized = normalized;
    };
    pot.onChange(callback);

    // Initial value
    pot.injectTestValue(512);
    CHECK(callback_count == 1);
    CHECK(last_normalized == doctest::Approx(512.0f / 1023.0f).epsilon(0.001));

    // Large change
    pot.injectTestValue(800);
    CHECK(callback_count == 2);
    CHECK(last_normalized == doctest::Approx(800.0f / 1023.0f).epsilon(0.001));
}

TEST_CASE("Potentiometer - hasChanged flag") {
    Potentiometer pot(0);
    pot.setHysteresis(50);

    // Initial state
    pot.injectTestValue(500);
    CHECK(pot.hasChanged() == true);  // First read counts as change

    // Small change (no trigger)
    pot.injectTestValue(520);
    CHECK(pot.hasChanged() == false);

    // Large change (triggers)
    pot.injectTestValue(600);
    CHECK(pot.hasChanged() == true);
}

TEST_CASE("Potentiometer - callback removal") {
    Potentiometer pot(0);
    pot.setHysteresis(50);

    int callback_count = 0;

    fl::function<void(Potentiometer&)> callback = [&](Potentiometer&) {
        callback_count++;
    };
    int id = pot.onChange(callback);

    // Trigger callback
    pot.injectTestValue(500);
    CHECK(callback_count == 1);

    // Remove callback
    pot.removeOnChange(id);

    // Trigger again - should not fire
    pot.injectTestValue(600);
    CHECK(callback_count == 1);  // Still 1, no increment
}

TEST_CASE("Potentiometer - clamping behavior") {
    Potentiometer pot(0);
    pot.setRange(100, 900);

    // Test values outside range are clamped
    pot.injectTestValue(0);
    CHECK(pot.raw() == 0);  // Raw value unchanged
    CHECK(pot.normalized() == 0.0f);  // But normalized is clamped
    CHECK(pot.fractional16() == 0);

    pot.injectTestValue(1023);
    CHECK(pot.raw() == 1023);
    CHECK(pot.normalized() == 1.0f);  // Clamped to 1.0
    CHECK(pot.fractional16() == 65535);
}

TEST_CASE("Potentiometer - edge case: invalid range") {
    Potentiometer pot(0);

    // Attempt to set invalid range (min >= max)
    pot.setRange(500, 500);  // Equal min/max
    // Should not change from default
    CHECK(pot.getRangeMin() == 0);
    CHECK(pot.getRangeMax() == 1023);

    pot.setRange(600, 400);  // Min > max
    // Should not change
    CHECK(pot.getRangeMin() == 0);
    CHECK(pot.getRangeMax() == 1023);
}

TEST_CASE("Potentiometer - multiple callbacks") {
    Potentiometer pot(0);
    pot.setHysteresis(50);

    int callback1_count = 0;
    int callback2_count = 0;

    fl::function<void(Potentiometer&)> callback1 = [&](Potentiometer&) {
        callback1_count++;
    };
    pot.onChange(callback1);

    fl::function<void(float)> callback2 = [&](float) {
        callback2_count++;
    };
    pot.onChange(callback2);

    // Both callbacks should fire
    pot.injectTestValue(500);
    CHECK(callback1_count == 1);
    CHECK(callback2_count == 1);

    pot.injectTestValue(600);
    CHECK(callback1_count == 2);
    CHECK(callback2_count == 2);
}

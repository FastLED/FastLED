/// @file power_model.cpp
/// Unit tests for PowerModel API (RGB, RGBW, RGBWW)

#include "FastLED.h"
#include "power_mgt.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include "hsv2rgb.h"

FL_TEST_FILE(FL_FILEPATH) {

struct ScopedPowerScalingExponent {
    explicit ScopedPowerScalingExponent(float exponent)
        : previous_model(get_power_model()) {
        PowerModelRGB updated_model = previous_model;
        updated_model.exponent = exponent;
        set_power_model(updated_model);
    }

    ~ScopedPowerScalingExponent() {
        set_power_model(previous_model);
    }

    PowerModelRGB previous_model;
};

FL_TEST_CASE("PowerModelRGB - constructor") {
    PowerModelRGB model(40, 40, 40, 2);
    FL_CHECK(model.red_mW == 40);
    FL_CHECK(model.green_mW == 40);
    FL_CHECK(model.blue_mW == 40);
    FL_CHECK(model.dark_mW == 2);
}

FL_TEST_CASE("PowerModelRGB - default constructor") {
    PowerModelRGB model;
    FL_CHECK(model.red_mW == 80);   // WS2812 @ 5V
    FL_CHECK(model.green_mW == 55);
    FL_CHECK(model.blue_mW == 75);
    FL_CHECK(model.dark_mW == 5);
}

FL_TEST_CASE("PowerModelRGBW - constructor") {
    PowerModelRGBW model(90, 70, 90, 100, 5);
    FL_CHECK(model.red_mW == 90);
    FL_CHECK(model.green_mW == 70);
    FL_CHECK(model.blue_mW == 90);
    FL_CHECK(model.white_mW == 100);
    FL_CHECK(model.dark_mW == 5);
}

FL_TEST_CASE("PowerModelRGBWW - constructor") {
    PowerModelRGBWW model(85, 65, 85, 95, 95, 5);
    FL_CHECK(model.red_mW == 85);
    FL_CHECK(model.green_mW == 65);
    FL_CHECK(model.blue_mW == 85);
    FL_CHECK(model.white_mW == 95);
    FL_CHECK(model.warm_white_mW == 95);
    FL_CHECK(model.dark_mW == 5);
}

FL_TEST_CASE("PowerModelRGBW - toRGB conversion") {
    PowerModelRGBW rgbw(90, 70, 90, 100, 5);
    PowerModelRGB rgb = rgbw.toRGB();

    // Only RGB + dark should be extracted
    FL_CHECK(rgb.red_mW == 90);
    FL_CHECK(rgb.green_mW == 70);
    FL_CHECK(rgb.blue_mW == 90);
    FL_CHECK(rgb.dark_mW == 5);
}

FL_TEST_CASE("PowerModelRGBWW - toRGB conversion folds in W/WW power (#2558 Phase F)") {
    PowerModelRGBWW rgbww(85, 65, 85, 95, 95, 5);
    PowerModelRGB rgb = rgbww.toRGB();

    // (#2558) toRGB() now distributes the white-channel mW evenly across the
    // three RGB channels so the brightness limiter doesn't under-budget when
    // RGBWW strips are in use. Bonus per RGB channel = (white_mW + warm_white_mW) / 3.
    const fl::u8 share = (95 + 95) / 3;  // 63
    FL_CHECK(rgb.red_mW == 85 + share);
    FL_CHECK(rgb.green_mW == 65 + share);
    FL_CHECK(rgb.blue_mW == 85 + share);
    FL_CHECK(rgb.dark_mW == 5);
}

FL_TEST_CASE("set_power_model / get_power_model - RGB") {
    PowerModelRGB custom(50, 50, 50, 3);
    set_power_model(custom);

    PowerModelRGB retrieved = get_power_model();
    FL_CHECK(retrieved.red_mW == 50);
    FL_CHECK(retrieved.green_mW == 50);
    FL_CHECK(retrieved.blue_mW == 50);
    FL_CHECK(retrieved.dark_mW == 3);
}

FL_TEST_CASE("set_power_model - RGBW extracts RGB") {
    PowerModelRGBW rgbw(90, 70, 90, 100, 5);
    set_power_model(rgbw);

    // Should extract only RGB components
    PowerModelRGB retrieved = get_power_model();
    FL_CHECK(retrieved.red_mW == 90);
    FL_CHECK(retrieved.green_mW == 70);
    FL_CHECK(retrieved.blue_mW == 90);
    FL_CHECK(retrieved.dark_mW == 5);
}

FL_TEST_CASE("set_power_model - RGBWW folds in W/WW power (#2558 Phase F)") {
    PowerModelRGBWW rgbww(85, 65, 85, 95, 95, 5);
    set_power_model(rgbww);

    // (#2558) Mirror the new PowerModelRGBWW::toRGB() contract: white-channel
    // power is distributed evenly across RGB rather than dropped.
    const fl::u8 share = (95 + 95) / 3;
    PowerModelRGB retrieved = get_power_model();
    FL_CHECK(retrieved.red_mW == 85 + share);
    FL_CHECK(retrieved.green_mW == 65 + share);
    FL_CHECK(retrieved.blue_mW == 85 + share);
    FL_CHECK(retrieved.dark_mW == 5);
}

FL_TEST_CASE("Power calculation - uses custom model") {
    // Set custom model with easy-to-test values
    set_power_model(PowerModelRGB(40, 40, 40, 2));

    // Create test LED array: all red at max brightness
    CRGB leds[10];
    for (int i = 0; i < 10; i++) {
        leds[i] = CRGB(255, 0, 0);
    }

    // Calculate power: (255 * 40 * 10) >> 8 + (2 * 10)
    // = (102000 >> 8) + 20 = 398 + 20 = 418 mW
    uint32_t power = calculate_unscaled_power_mW(leds, 10);
    FL_CHECK(power >= 413);  // Within tolerance
    FL_CHECK(power <= 423);
}

FL_TEST_CASE("Power calculation - all channels") {
    // Set symmetric model for easier testing
    set_power_model(PowerModelRGB(50, 50, 50, 0));

    // White LEDs (255, 255, 255)
    CRGB leds[5];
    for (int i = 0; i < 5; i++) {
        leds[i] = CRGB(255, 255, 255);
    }

    // Power: ((255 * 50 * 3 channels * 5 LEDs) >> 8) + 0
    // = (191250 >> 8) = 747 mW
    uint32_t power = calculate_unscaled_power_mW(leds, 5);
    FL_CHECK(power >= 742);  // Within tolerance
    FL_CHECK(power <= 752);
}

FL_TEST_CASE("FastLED wrapper - RGB") {
    FastLED.setPowerModel(PowerModelRGB(30, 35, 40, 1));

    PowerModelRGB retrieved = FastLED.getPowerModel();
    FL_CHECK(retrieved.red_mW == 30);
    FL_CHECK(retrieved.green_mW == 35);
    FL_CHECK(retrieved.blue_mW == 40);
    FL_CHECK(retrieved.dark_mW == 1);
}

FL_TEST_CASE("FastLED wrapper - RGBW") {
    // Set RGBW model - should extract RGB only
    FastLED.setPowerModel(PowerModelRGBW(90, 70, 90, 100, 5));

    PowerModelRGB retrieved = FastLED.getPowerModel();
    FL_CHECK(retrieved.red_mW == 90);
    FL_CHECK(retrieved.green_mW == 70);
    FL_CHECK(retrieved.blue_mW == 90);
    FL_CHECK(retrieved.dark_mW == 5);
}

FL_TEST_CASE("Default power model - WS2812 @ 5V") {
    // Reset to default by setting it explicitly
    set_power_model(PowerModelRGB());

    PowerModelRGB current = get_power_model();
    FL_CHECK(current.red_mW == 80);
    FL_CHECK(current.green_mW == 55);
    FL_CHECK(current.blue_mW == 75);
    FL_CHECK(current.dark_mW == 5);
}

FL_TEST_CASE("Power scaling exponent - default is linear") {
    set_power_model(PowerModelRGB(50, 50, 50, 0, 1.0f));
    FL_CHECK(get_power_scaling_exponent() >= 0.999f);
    FL_CHECK(get_power_scaling_exponent() <= 1.001f);

    CRGB leds[1] = {CRGB(128, 0, 0)};
    uint32_t power = calculate_unscaled_power_mW(leds, 1);

    // 128 * 50 >> 8 = 25mW with the default linear scaling model
    FL_CHECK(power >= 24);
    FL_CHECK(power <= 25);
}

FL_TEST_CASE("Power scaling exponent - non linear estimate increases mid brightness demand") {
    // Demonstrates the single-call pattern: channel weights + response curve
    // configured together via PowerModelRGB's exponent field.
    set_power_model(PowerModelRGB(50, 50, 50, 0, 0.87f));
    FL_CHECK(get_power_scaling_exponent() >= 0.869f);
    FL_CHECK(get_power_scaling_exponent() <= 0.871f);

    CRGB leds[1] = {CRGB(128, 0, 0)};
    uint32_t power = calculate_unscaled_power_mW(leds, 1);

    // Non-linear mapping should estimate higher power than the linear 25mW case.
    FL_CHECK(power > 25);

    // Reset to linear for isolation from subsequent tests.
    set_power_model(PowerModelRGB());
}

FL_TEST_CASE("PowerModelRGB - exponent field travels with the model") {
    // Setting the model with a non-linear exponent replaces the two-call pattern
    // (set_power_model + set_power_scaling_exponent) with a single call.
    set_power_model(PowerModelRGB(40, 40, 40, 2, 0.87f));

    PowerModelRGB retrieved = get_power_model();
    FL_CHECK(retrieved.red_mW == 40);
    FL_CHECK(retrieved.green_mW == 40);
    FL_CHECK(retrieved.blue_mW == 40);
    FL_CHECK(retrieved.dark_mW == 2);
    FL_CHECK(retrieved.exponent >= 0.869f);
    FL_CHECK(retrieved.exponent <= 0.871f);
    FL_CHECK(get_power_scaling_exponent() >= 0.869f);
    FL_CHECK(get_power_scaling_exponent() <= 0.871f);

    // Reset to linear default via the same single-call pattern.
    set_power_model(PowerModelRGB());
    FL_CHECK(get_power_scaling_exponent() >= 0.999f);
    FL_CHECK(get_power_scaling_exponent() <= 1.001f);
}

FL_TEST_CASE("Power scaling exponent - limiter becomes more conservative") {
    set_power_model(PowerModelRGB(80, 80, 80, 0, 1.0f));

    CRGB leds[10];
    for (int i = 0; i < 10; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }

    fl::u8 linear_recommended = calculate_max_brightness_for_power_mW(leds, 10, 128, 1250);
    FL_CHECK(linear_recommended == 128);

    {
        ScopedPowerScalingExponent scaling(0.87f);
        fl::u8 nonlinear_recommended = calculate_max_brightness_for_power_mW(leds, 10, 128, 1250);
        FL_CHECK(nonlinear_recommended < 128);
    }
}

} // FL_TEST_FILE

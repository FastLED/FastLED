/// @file power_model.cpp
/// Unit tests for PowerModel API (RGB, RGBW, RGBWW)

#include "FastLED.h"
#include "power_mgt.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "hsv2rgb.h"

TEST_CASE("PowerModelRGB - constructor") {
    PowerModelRGB model(40, 40, 40, 2);
    CHECK(model.red_mW == 40);
    CHECK(model.green_mW == 40);
    CHECK(model.blue_mW == 40);
    CHECK(model.dark_mW == 2);
}

TEST_CASE("PowerModelRGB - default constructor") {
    PowerModelRGB model;
    CHECK(model.red_mW == 80);   // WS2812 @ 5V
    CHECK(model.green_mW == 55);
    CHECK(model.blue_mW == 75);
    CHECK(model.dark_mW == 5);
}

TEST_CASE("PowerModelRGBW - constructor") {
    PowerModelRGBW model(90, 70, 90, 100, 5);
    CHECK(model.red_mW == 90);
    CHECK(model.green_mW == 70);
    CHECK(model.blue_mW == 90);
    CHECK(model.white_mW == 100);
    CHECK(model.dark_mW == 5);
}

TEST_CASE("PowerModelRGBWW - constructor") {
    PowerModelRGBWW model(85, 65, 85, 95, 95, 5);
    CHECK(model.red_mW == 85);
    CHECK(model.green_mW == 65);
    CHECK(model.blue_mW == 85);
    CHECK(model.white_mW == 95);
    CHECK(model.warm_white_mW == 95);
    CHECK(model.dark_mW == 5);
}

TEST_CASE("PowerModelRGBW - toRGB conversion") {
    PowerModelRGBW rgbw(90, 70, 90, 100, 5);
    PowerModelRGB rgb = rgbw.toRGB();

    // Only RGB + dark should be extracted
    CHECK(rgb.red_mW == 90);
    CHECK(rgb.green_mW == 70);
    CHECK(rgb.blue_mW == 90);
    CHECK(rgb.dark_mW == 5);
}

TEST_CASE("PowerModelRGBWW - toRGB conversion") {
    PowerModelRGBWW rgbww(85, 65, 85, 95, 95, 5);
    PowerModelRGB rgb = rgbww.toRGB();

    // Only RGB + dark should be extracted
    CHECK(rgb.red_mW == 85);
    CHECK(rgb.green_mW == 65);
    CHECK(rgb.blue_mW == 85);
    CHECK(rgb.dark_mW == 5);
}

TEST_CASE("set_power_model / get_power_model - RGB") {
    PowerModelRGB custom(50, 50, 50, 3);
    set_power_model(custom);

    PowerModelRGB retrieved = get_power_model();
    CHECK(retrieved.red_mW == 50);
    CHECK(retrieved.green_mW == 50);
    CHECK(retrieved.blue_mW == 50);
    CHECK(retrieved.dark_mW == 3);
}

TEST_CASE("set_power_model - RGBW extracts RGB") {
    PowerModelRGBW rgbw(90, 70, 90, 100, 5);
    set_power_model(rgbw);

    // Should extract only RGB components
    PowerModelRGB retrieved = get_power_model();
    CHECK(retrieved.red_mW == 90);
    CHECK(retrieved.green_mW == 70);
    CHECK(retrieved.blue_mW == 90);
    CHECK(retrieved.dark_mW == 5);
}

TEST_CASE("set_power_model - RGBWW extracts RGB") {
    PowerModelRGBWW rgbww(85, 65, 85, 95, 95, 5);
    set_power_model(rgbww);

    // Should extract only RGB components
    PowerModelRGB retrieved = get_power_model();
    CHECK(retrieved.red_mW == 85);
    CHECK(retrieved.green_mW == 65);
    CHECK(retrieved.blue_mW == 85);
    CHECK(retrieved.dark_mW == 5);
}

TEST_CASE("Power calculation - uses custom model") {
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
    CHECK(power >= 413);  // Within tolerance
    CHECK(power <= 423);
}

TEST_CASE("Power calculation - all channels") {
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
    CHECK(power >= 742);  // Within tolerance
    CHECK(power <= 752);
}

TEST_CASE("FastLED wrapper - RGB") {
    FastLED.setPowerModel(PowerModelRGB(30, 35, 40, 1));

    PowerModelRGB retrieved = FastLED.getPowerModel();
    CHECK(retrieved.red_mW == 30);
    CHECK(retrieved.green_mW == 35);
    CHECK(retrieved.blue_mW == 40);
    CHECK(retrieved.dark_mW == 1);
}

TEST_CASE("FastLED wrapper - RGBW") {
    // Set RGBW model - should extract RGB only
    FastLED.setPowerModel(PowerModelRGBW(90, 70, 90, 100, 5));

    PowerModelRGB retrieved = FastLED.getPowerModel();
    CHECK(retrieved.red_mW == 90);
    CHECK(retrieved.green_mW == 70);
    CHECK(retrieved.blue_mW == 90);
    CHECK(retrieved.dark_mW == 5);
}

TEST_CASE("Default power model - WS2812 @ 5V") {
    // Reset to default by setting it explicitly
    set_power_model(PowerModelRGB());

    PowerModelRGB current = get_power_model();
    CHECK(current.red_mW == 80);
    CHECK(current.green_mW == 55);
    CHECK(current.blue_mW == 75);
    CHECK(current.dark_mW == 5);
}

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

// #4156 R4: "Fixed idle consumption cannot be reduced by multiplying LED
// flux." These pin the three things that finding asked for -- the baseline,
// a budget below it, and the encoded demand against the declared bound.

namespace {

struct ScopedDefaultPowerModel {
    ScopedDefaultPowerModel() : previous_model(get_power_model()) {
        set_power_model(PowerModelRGB());
    }
    ~ScopedDefaultPowerModel() { set_power_model(previous_model); }
    PowerModelRGB previous_model;
};

// What the strip really draws at a brightness: the dark current of every
// controller IC, which no scalar touches, plus the scaled emitter share.
fl::u32 true_demand_mW(fl::span<const CRGB> leds, fl::u8 brightness) {
    const fl::u32 fixed_mW =
        static_cast<fl::u32>(get_power_model().dark_mW) * leds.size();
    const fl::u32 total_mW = calculate_unscaled_power_mW(leds);
    const fl::u32 controllable_mW = total_mW - fixed_mW;
    return fixed_mW + scale_power_for_brightness(controllable_mW, brightness);
}

} // namespace

FL_TEST_CASE("Power limiter - the recommendation stays inside the budget") {
    ScopedDefaultPowerModel guard;
    const int kCount = 300;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    const fl::span<const CRGB> span(leds, kCount);
    const fl::u32 baseline_mW =
        static_cast<fl::u32>(get_power_model().dark_mW) * kCount;

    // Every one of these used to come back over budget, by 5.6% at 20 W and
    // by 73% at 2 W: the whole estimate including the baseline was multiplied
    // by brightness, so lowering brightness "reduced" a draw that is constant.
    const fl::u32 budgets[] = {60000u, 40000u, 20000u, 10000u, 5000u, 3000u, 2000u};
    for (fl::u32 budget : budgets) {
        const fl::u8 recommended =
            calculate_max_brightness_for_power_mW(leds, kCount, 255, budget);
        FL_CHECK_GT(budget, baseline_mW);
        FL_CHECK_LE(true_demand_mW(span, recommended), budget);
    }
}

FL_TEST_CASE("Power limiter - the recommendation is the largest that fits") {
    ScopedDefaultPowerModel guard;
    const int kCount = 300;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    const fl::span<const CRGB> span(leds, kCount);

    // Staying under the budget is trivially satisfied by returning zero. The
    // limiter has to be tight as well as safe, so one step up must not fit.
    const fl::u32 budgets[] = {40000u, 20000u, 10000u, 5000u, 3000u};
    for (fl::u32 budget : budgets) {
        const fl::u8 recommended =
            calculate_max_brightness_for_power_mW(leds, kCount, 255, budget);
        FL_CHECK_GT(recommended, 0);
        FL_CHECK_GT(true_demand_mW(span, static_cast<fl::u8>(recommended + 1)),
                    budget);
    }
}

FL_TEST_CASE("Power limiter - a binding budget is maximal below full brightness") {
    ScopedDefaultPowerModel guard;
    const int kCount = 300;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    const fl::span<const CRGB> span(leds, kCount);

    // Every case above asks at brightness 255, where the scaled target is 255
    // and a ratio taken against it happens to be right. Below full brightness
    // it is not: `controllable_mW` is the demand at *full* brightness, so
    // scaling the headroom ratio by the request answers "what fraction of the
    // request fits" rather than "what brightness fits", and the step-down can
    // only make that smaller. The answer must still be the largest that fits.
    const fl::u8 targets[] = {200, 128, 64, 32};
    const fl::u32 budgets[] = {40000u, 20000u, 10000u, 5000u, 3000u};
    for (fl::u8 target : targets) {
        for (fl::u32 budget : budgets) {
            const fl::u8 recommended =
                calculate_max_brightness_for_power_mW(leds, kCount, target, budget);
            FL_CHECK_LE(recommended, target);
            FL_CHECK_LE(true_demand_mW(span, recommended), budget);
            if (recommended < target) {
                FL_CHECK_GT(
                    true_demand_mW(span, static_cast<fl::u8>(recommended + 1)),
                    budget);
            }
        }
    }
}

FL_TEST_CASE("Power limiter - zero brightness still draws the dark current") {
    ScopedDefaultPowerModel guard;
    const int kCount = 300;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    // The baseline is the whole point of the finding: at brightness zero the
    // strip is dark and still drawing 1500 mW on this model.
    const fl::u32 baseline_mW =
        static_cast<fl::u32>(get_power_model().dark_mW) * kCount;
    FL_CHECK_EQ(true_demand_mW(fl::span<const CRGB>(leds, kCount), 0),
                baseline_mW);
    FL_CHECK_GT(baseline_mW, 0);
}

FL_TEST_CASE("Power limiter - a budget under the baseline turns the strip off") {
    ScopedDefaultPowerModel guard;
    const int kCount = 300;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    const fl::u32 baseline_mW =
        static_cast<fl::u32>(get_power_model().dark_mW) * kCount;

    // No brightness meets a budget below the dark current, so the only honest
    // answer is zero. It used to return a lit strip and claim the budget was
    // met -- 1000 mW asked for, 2480 mW drawn.
    const fl::u32 budgets[] = {baseline_mW - 1, baseline_mW / 2, 1u};
    for (fl::u32 budget : budgets) {
        FL_CHECK_EQ(calculate_max_brightness_for_power_mW(leds, kCount, 255, budget),
                    0);
    }
    // Exactly at the baseline nothing can be lit either, since any emitter
    // adds to it.
    FL_CHECK_EQ(
        calculate_max_brightness_for_power_mW(leds, kCount, 255, baseline_mW), 0);
}

FL_TEST_CASE("Power limiter - an all-dark strip over budget is still answerable") {
    ScopedDefaultPowerModel guard;
    const int kCount = 300;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB::Black;
    }
    // Black leaves nothing for brightness to scale, so the controllable share
    // is zero. Reaching the ratio with that as its denominator divides by
    // zero, and a budget under the baseline is the one path that gets there.
    const fl::u32 baseline_mW =
        static_cast<fl::u32>(get_power_model().dark_mW) * kCount;
    FL_CHECK_EQ(calculate_unscaled_power_mW(leds, kCount), baseline_mW);
    FL_CHECK_EQ(
        calculate_max_brightness_for_power_mW(leds, kCount, 255, baseline_mW / 2),
        0);
    // And under a budget it does fit, the request is untouched.
    FL_CHECK_EQ(
        calculate_max_brightness_for_power_mW(leds, kCount, 255, baseline_mW * 2),
        255);
}

FL_TEST_CASE("Power limiter - a budget over demand leaves brightness alone") {
    ScopedDefaultPowerModel guard;
    const int kCount = 60;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(64, 64, 64);
    }
    // Nothing above should have made the limiter pessimistic in the ordinary
    // case, where the budget is not binding at all.
    FL_CHECK_EQ(calculate_max_brightness_for_power_mW(leds, kCount, 200, 1000000u),
                200);
}

} // FL_TEST_FILE

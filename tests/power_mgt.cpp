/// @file power_model.cpp
/// Unit tests for PowerModel API (RGB, RGBW, RGBWW)

#include "FastLED.h"
#include "power_mgt.h"
#include "fl/gfx/pipeline.h"
#include "fl/gfx/colorimetric_response.h"
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
    ScopedDefaultPowerModel()
        : previous_model(get_power_model()),
          previous_white_mW(get_white_emitter_mW()) {
        set_power_model(PowerModelRGB());
    }
    ~ScopedDefaultPowerModel() {
        // Restores both halves, for the reason on ScopedRgbwPowerModel below:
        // the RGB setter retracts a white declaration by design, so restoring
        // through it alone loses one that was standing on entry.
        if (previous_white_mW != 0) {
            set_power_model(PowerModelRGBW(
                previous_model.red_mW, previous_model.green_mW,
                previous_model.blue_mW, previous_white_mW,
                previous_model.dark_mW, previous_model.exponent));
        } else {
            set_power_model(previous_model);
        }
    }
    PowerModelRGB previous_model;
    fl::u8 previous_white_mW;
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

// ---------------------------------------------------------------------------
// #4156 R3: the estimate must cover the emitters the strip actually lights.
// ---------------------------------------------------------------------------

namespace {

// The four-emitter draw, computed the long way: run the same conversion the
// encoder runs, then charge each diode at its declared rate. Deliberately not
// the function under test -- if both were the same code the cases below would
// only be asserting that a function equals itself.
fl::u32 four_emitter_mW(fl::span<const CRGB> leds, const fl::Rgbw& rgbw,
                        const PowerModelRGBW& model) {
    fl::u32 r32 = 0, g32 = 0, b32 = 0, w32 = 0;
    for (fl::size i = 0; i < leds.size(); ++i) {
        fl::u8 r = 0, g = 0, b = 0, w = 0;
        fl::rgb_2_rgbw(rgbw, leds[i].r, leds[i].g, leds[i].b, 255, 255, 255,
                       &r, &g, &b, &w);
        r32 += r;
        g32 += g;
        b32 += b;
        w32 += w;
    }
    return ((r32 * model.red_mW) >> 8) + ((g32 * model.green_mW) >> 8) +
           ((b32 * model.blue_mW) >> 8) + ((w32 * model.white_mW) >> 8) +
           static_cast<fl::u32>(model.dark_mW) * leds.size();
}

struct ScopedRgbwPowerModel {
    explicit ScopedRgbwPowerModel(const PowerModelRGBW& model)
        : previous_model(get_power_model()),
          previous_white_mW(get_white_emitter_mW()) {
        set_power_model(model);
    }
    ~ScopedRgbwPowerModel() {
        // Both halves, not just the RGB one. The RGB setter deliberately
        // retracts a white declaration -- that is the contract a case below
        // asserts -- so restoring through it alone would leave the process
        // with no white emitter regardless of what it had on entry, and make
        // any case that ran afterwards depend on the order it ran in.
        if (previous_white_mW != 0) {
            set_power_model(PowerModelRGBW(
                previous_model.red_mW, previous_model.green_mW,
                previous_model.blue_mW, previous_white_mW,
                previous_model.dark_mW, previous_model.exponent));
        } else {
            set_power_model(previous_model);
        }
    }
    PowerModelRGB previous_model;
    fl::u8 previous_white_mW;
};

/// A registered controller, for the two entry points that walk the controller
/// list rather than taking a buffer.
///
/// Scoped rather than added through `FastLED.addLeds<>`: `~CLEDController`
/// calls `removeFromDrawList()` on large-memory targets, so this leaves the
/// global list exactly as it found it. There is no API to unregister an
/// `addLeds` controller, and a leaked one would silently join every later
/// case's traversal.
class RegisteredController : public CLEDController {
  public:
    void showColor(const CRGB&, int, fl::u8) FL_NO_EXCEPT override {}
    void show(const CRGB*, int, fl::u8) FL_NO_EXCEPT override {}
    void init() FL_NO_EXCEPT override {}
};

} // namespace

FL_TEST_CASE("Power model - an RGBW declaration keeps its white emitter") {
    // `set_power_model(PowerModelRGBW)` used to route through `toRGB()`,
    // which drops `white_mW`. The API accepted the number and threw it away,
    // so every RGBW budget was computed over three of four diodes.
    ScopedRgbwPowerModel guard(PowerModelRGBW(90, 70, 90, 100, 5));
    FL_CHECK_EQ(get_white_emitter_mW(), 100);
    // And the RGB half still lands where it was declared.
    FL_CHECK_EQ(get_power_model().red_mW, 90);
    FL_CHECK_EQ(get_power_model().blue_mW, 90);

    // Declaring an RGB model afterwards retracts the white emitter: three
    // emitters is what an RGB model says the strip has.
    set_power_model(PowerModelRGB(80, 55, 75, 5));
    FL_CHECK_EQ(get_white_emitter_mW(), 0);
}

FL_TEST_CASE("Power model - changing the exponent does not retract the white emitter") {
    // `set_power_scaling_exponent` reinstalls the RGB model to carry the new
    // exponent. Routing that through the public RGB setter would clear the
    // white declaration as a side effect of an unrelated call.
    ScopedRgbwPowerModel guard(PowerModelRGBW(90, 70, 90, 100, 5));
    set_power_scaling_exponent(1.0f);
    FL_CHECK_EQ(get_white_emitter_mW(), 100);
}

FL_TEST_CASE("Power estimate - the source triple is not the RGBW strip's demand") {
    // The measurement behind R3, at the size that makes it concrete. Source
    // white on 300 SK6812-class pixels, the shipped default RGBW model.
    ScopedRgbwPowerModel guard{PowerModelRGBW()};
    const PowerModelRGBW model;
    const int kCount = 300;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    const fl::span<const CRGB> span(leds, kCount);
    const fl::u32 source_triple_mW = calculate_unscaled_power_mW(span);

    struct Case {
        fl::RGBW_MODE mode;
        bool source_triple_under_counts;
    };
    const Case cases[] = {
        // Lights every diode at the source level: the largest draw of the
        // four modes, and the one the source triple most under-states.
        {fl::RGBW_MODE::kRGBWMaxBrightness, true},
        // Adds white on top of a reduced RGB residual; still over the triple.
        {fl::RGBW_MODE::kRGBWBoostedWhite, true},
        // Moves the neutral into the white diode, so the strip draws far
        // *less* than the triple. Wrong in the other direction: a strip
        // dimmed against a budget it was never near.
        {fl::RGBW_MODE::kRGBWExactColors, false},
    };

    for (const auto& c : cases) {
        const fl::Rgbw rgbw(fl::kRGBWDefaultColorTemp, c.mode);
        const fl::u32 truth = four_emitter_mW(span, rgbw, model);
        const fl::u32 measured = calculate_unscaled_power_mW(span, rgbw);

        // The estimate tracks the four-emitter truth, not the source triple.
        // Within a milliwatt per LED of rounding across the two sum orders.
        FL_CHECK_LE(measured > truth ? measured - truth : truth - measured,
                    static_cast<fl::u32>(kCount));

        // And the gap it closes is not a rounding-scale gap. Measured on
        // these 300 pixels the smallest of the three is `kRGBWBoostedWhite`
        // at 6.5% of the true draw; `kRGBWMaxBrightness` is 28% and
        // `kRGBWExactColors` is 150%. A twentieth is under all three and far
        // over anything the two sum orders could differ by.
        const fl::u32 gap = source_triple_mW > truth ? source_triple_mW - truth
                                                     : truth - source_triple_mW;
        FL_CHECK_GT(gap, truth / 20);
        FL_CHECK_EQ(truth > source_triple_mW, c.source_triple_under_counts);
    }
}

FL_TEST_CASE("Power estimate - the white diode is charged only where there is one") {
    // The same model must not inflate a plain RGB controller on the same
    // sketch. An inactive Rgbw is the three-emitter answer exactly.
    //
    // Note what this does and does not pin. It pins the answer. It cannot
    // distinguish the implementation's `!rgbw.active()` short-circuit from
    // running the conversion anyway, because the inactive mode dispatches to
    // null-white and returns the input unchanged -- deleting that branch
    // leaves this case passing. The branch is there for the per-pixel cost,
    // and is documented as such rather than as a guard.
    ScopedRgbwPowerModel guard{PowerModelRGBW()};
    const int kCount = 60;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(200, 140, 90);
    }
    const fl::span<const CRGB> span(leds, kCount);
    FL_CHECK_EQ(calculate_unscaled_power_mW(span, fl::RgbwInvalid::value()),
                calculate_unscaled_power_mW(span));
}

FL_TEST_CASE("Power estimate - an undeclared white emitter falls back, it does not guess") {
    // With only an RGB model declared there is no white draw to charge. The
    // estimate stays the three-emitter figure rather than inventing a number
    // for the fourth diode, which is the state that produced R3 in the first
    // place -- so the fallback is the documented behaviour, not silence.
    ScopedDefaultPowerModel guard;
    const int kCount = 60;
    CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    const fl::span<const CRGB> span(leds, kCount);
    const fl::Rgbw rgbw(fl::kRGBWDefaultColorTemp,
                        fl::RGBW_MODE::kRGBWMaxBrightness);
    FL_CHECK_EQ(get_white_emitter_mW(), 0);
    FL_CHECK_EQ(calculate_unscaled_power_mW(span, rgbw),
                calculate_unscaled_power_mW(span));
}

FL_TEST_CASE("Power limiter - an RGBW budget is met by the emitters, not by three of them") {
    // The end-to-end case, through the entry point a sketch actually uses.
    //
    // An earlier version of this compared estimator values and never called
    // the limiter, so reverting the traversal site in
    // `calculate_max_brightness_for_power_mW` to the three-emitter overload
    // left every case here passing. This one registers a controller and goes
    // through the list-walking overload, which is the only thing that
    // exercises `pCur->getRgbw()`.
    ScopedRgbwPowerModel guard{PowerModelRGBW()};
    const PowerModelRGBW model;
    const int kCount = 300;
    static CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    const fl::span<const CRGB> span(leds, kCount);
    const fl::Rgbw rgbw(fl::kRGBWDefaultColorTemp,
                        fl::RGBW_MODE::kRGBWMaxBrightness);

    RegisteredController controller;
    controller.setLeds(leds, kCount);
    controller.setRgbw(rgbw);

    const fl::u32 truth_mW = four_emitter_mW(span, rgbw, model);
    const fl::u32 source_triple_mW = calculate_unscaled_power_mW(span);

    // A budget between the two figures: the source triple says the strip fits
    // inside it at full brightness, the four-emitter truth says it does not.
    // That interval is exactly the region the old estimate got wrong, and it
    // is non-empty only because the two disagree.
    FL_REQUIRE(source_triple_mW < truth_mW);
    const fl::u32 budget_mW = (source_triple_mW + truth_mW) / 2;

    const fl::u8 recommended =
        calculate_max_brightness_for_power_mW(255, budget_mW);

    // The three-emitter figure would have said the whole strip fits at full
    // brightness. It does not, so the limiter must come down off 255.
    FL_CHECK_LT(int(recommended), 255);

    // And what it recommends must actually fit, measured against the demand
    // the strip really draws rather than the one the estimator used to report.
    // The controller's dark current is the part no scalar reduces; the rest
    // is what the recommendation scales.
    const fl::u32 fixed_mW = static_cast<fl::u32>(model.dark_mW) * kCount;
    const fl::u32 controllable_mW = truth_mW - fixed_mW;
    const fl::u32 drawn_mW =
        fixed_mW + scale_power_for_brightness(controllable_mW, recommended);
    FL_CHECK_LE(drawn_mW, budget_mW);
}

FL_TEST_CASE("Power estimate - the reported total walks controllers with their own RGBW setting") {
    // The second entry point that walks the list.
    // `CFastLED::getEstimatedPowerInMilliWatts` has its own traversal, and
    // reverting *its* call to the three-emitter overload also left every case
    // here passing before this one existed.
    ScopedRgbwPowerModel guard{PowerModelRGBW()};
    const PowerModelRGBW model;
    const int kCount = 60;
    static CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(255, 255, 255);
    }
    const fl::span<const CRGB> span(leds, kCount);
    const fl::Rgbw rgbw(fl::kRGBWDefaultColorTemp,
                        fl::RGBW_MODE::kRGBWMaxBrightness);

    RegisteredController controller;
    controller.setLeds(leds, kCount);
    controller.setRgbw(rgbw);

    const fl::u8 previous_brightness = FastLED.getBrightness();
    FastLED.setBrightness(255);
    // Without a limiter installed the report is the unscaled demand, which is
    // what makes it comparable to the four-emitter figure directly.
    const fl::u32 reported_mW = FastLED.getEstimatedPowerInMilliWatts(false);
    FastLED.setBrightness(previous_brightness);

    const fl::u32 truth_mW = four_emitter_mW(span, rgbw, model);
    const fl::u32 source_triple_mW = calculate_unscaled_power_mW(span);

    // Within a milliwatt per LED of the four-emitter truth, across the two
    // sum orders -- and nowhere near the three-emitter figure it used to
    // report, which is 28% low on this mode.
    FL_CHECK_LE(reported_mW > truth_mW ? reported_mW - truth_mW
                                       : truth_mW - reported_mW,
                static_cast<fl::u32>(kCount));
    FL_CHECK_GT(reported_mW, source_triple_mW);
}

FL_TEST_CASE("Power model - a scoped guard puts back the white emitter it found") {
    // What makes the guards above safe to nest, and why they save two things
    // rather than one. The RGB setter retracts a white declaration on purpose
    // -- a case above asserts exactly that -- so a guard restoring through it
    // alone would leave the process with no white emitter whatever it had on
    // entry, and every case that ran afterwards would depend on its position
    // in the file.
    ScopedDefaultPowerModel outer;
    set_power_model(PowerModelRGBW(90, 70, 90, 77, 5));
    FL_REQUIRE(get_white_emitter_mW() == 77);

    {
        ScopedRgbwPowerModel inner{PowerModelRGBW(10, 10, 10, 20, 1)};
        FL_CHECK_EQ(get_white_emitter_mW(), 20);
    }
    FL_CHECK_EQ(get_white_emitter_mW(), 77);

    {
        ScopedDefaultPowerModel inner;
        FL_CHECK_EQ(get_white_emitter_mW(), 0);  // an RGB model has no fourth diode
    }
    FL_CHECK_EQ(get_white_emitter_mW(), 77);

    // And leave the process as the file found it: `outer` restores the RGB
    // model, and this retracts the white this case declared.
    set_power_model(PowerModelRGB());
}

// ---------------------------------------------------------------------------
// #4156 R3: what the limiter can see of a managed channel's demand.
// ---------------------------------------------------------------------------

namespace {

/// The three-emitter device the colour-pipeline tests use: sRGB primaries at
/// unit luminance each.
fl::EmitterProfile pipelineDevice() {
    fl::EmitterProfile p = {};
    p.xy_r[0] = 0.6400f; p.xy_r[1] = 0.3300f;
    p.xy_g[0] = 0.3000f; p.xy_g[1] = 0.6000f;
    p.xy_b[0] = 0.1500f; p.xy_b[1] = 0.0600f;
    p.lum_r = 1.0f; p.lum_g = 1.0f; p.lum_b = 1.0f;
    p.native_code_depth = 8;
    return p;
}

/// Demand from a solved drive triple, in the same Q16-scaled units as
/// `estimatedFromSourceCodes` below so the two are comparable.
fl::i64 drivenDemandQ16(const fl::i32 (&drives)[3], const PowerModelRGB& model) {
    return static_cast<fl::i64>(drives[0]) * model.red_mW +
           static_cast<fl::i64>(drives[1]) * model.green_mW +
           static_cast<fl::i64>(drives[2]) * model.blue_mW;
}

/// What `calculate_unscaled_power_mW` charges, per pixel, before the dark
/// current: the source code read straight off the CRGB array.
///
/// The `>> 8` is the estimator's own, and is per channel and truncating --
/// `power_mgt.cpp.hpp` weights each channel by its emitter cost and then
/// shifts, so a full red code against an 80 mW emitter is charged 79 mW, not
/// 80. Dividing the weighted sum by 255 instead would model an estimator
/// about 0.4% more generous than the real one, which is the wrong direction
/// for a test whose subject is whether the real one ever charges too little.
///
/// `map_power_value` is identity here: its LUT is only non-identity once a
/// power-scaling exponent is set, and these cases run the default model.
fl::i64 estimatedFromSourceCodes(fl::u8 r, fl::u8 g, fl::u8 b,
                                 const PowerModelRGB& model) {
    const fl::i64 red = (static_cast<fl::i64>(r) * model.red_mW) >> 8;
    const fl::i64 green = (static_cast<fl::i64>(g) * model.green_mW) >> 8;
    const fl::i64 blue = (static_cast<fl::i64>(b) * model.blue_mW) >> 8;
    return (red + green + blue) * 65536;
}

} // namespace

FL_TEST_CASE("R3 - the limiter never under-charges a managed channel") {
    // The half of #4156 R3 that #4284 did not cover. That one fixed the RGBW
    // allocation; this is the profile conversion.
    //
    // R3 says accurate demand depends on it, and offers two ways out:
    // "specify two-pass streaming **or a demonstrably conservative
    // estimator**". This measures which of those the code already has.
    //
    // The estimator reads the source CRGB. A channel with a profile bound
    // emits `processPixelQ16`'s solved drives instead, and the two are not
    // the same number: the decode linearises sRGB, so code 128 asks for
    // 0.216 of full light rather than 0.502, and the solve then works in the
    // device's own emitter scale.
    //
    // The direction is what decides whether the bound is safe.
    ScopedDefaultPowerModel guard;
    const PowerModelRGB model = get_power_model();
    const fl::SourceProfile kSources[] = {fl::SourceProfile::srgbBt709(),
                                          fl::SourceProfile::displayP3(),
                                          fl::SourceProfile::bt2020()};
    int under_estimates = 0;
    int samples = 0;
    for (const auto& source : kSources) {
        fl::StreamingPipelineQ16 pipeline;
        FL_REQUIRE(buildStreamingPipelineQ16(source, pipelineDevice(),
                                             fl::GamutPolicy::ChromaCompress,
                                             &pipeline));
        for (int r = 0; r <= 255; r += 17) {
            for (int g = 0; g <= 255; g += 17) {
                for (int b = 0; b <= 255; b += 17) {
                    fl::i32 drives[3];
                    processPixelQ16(pipeline, static_cast<fl::u8>(r),
                                    static_cast<fl::u8>(g),
                                    static_cast<fl::u8>(b), drives);
                    const fl::i64 driven = drivenDemandQ16(drives, model);
                    const fl::i64 estimated = estimatedFromSourceCodes(
                        static_cast<fl::u8>(r), static_cast<fl::u8>(g),
                        static_cast<fl::u8>(b), model);
                    ++samples;
                    if (driven > estimated) {
                        ++under_estimates;
                    }
                }
            }
        }
    }

    // 12,288 samples across three source profiles, and the estimate is never
    // below the demand. So the electrical bound is not broken by the profile
    // conversion: R3's "demonstrably conservative estimator" is what the code
    // already has, in direction.
    FL_CHECK_EQ(samples, 12288);
    FL_CHECK_EQ(under_estimates, 0);
}

FL_TEST_CASE("R3 - and conservatism costs between 1.37x and 129x of the budget") {
    // The other half of the answer, and the reason the prepass is still
    // worth building. Safe is not the same as usable: every one of these is
    // headroom a managed strip under a power cap gives up.
    ScopedDefaultPowerModel guard;
    const PowerModelRGB model = get_power_model();
    const fl::SourceProfile kSources[] = {fl::SourceProfile::srgbBt709(),
                                          fl::SourceProfile::displayP3(),
                                          fl::SourceProfile::bt2020()};
    fl::i64 worst_ratio_milli = 0;
    fl::i64 least_ratio_milli = 1000000;
    for (const auto& source : kSources) {
        fl::StreamingPipelineQ16 pipeline;
        FL_REQUIRE(buildStreamingPipelineQ16(source, pipelineDevice(),
                                             fl::GamutPolicy::ChromaCompress,
                                             &pipeline));
        for (int r = 0; r <= 255; r += 17) {
            for (int g = 0; g <= 255; g += 17) {
                for (int b = 0; b <= 255; b += 17) {
                    fl::i32 drives[3];
                    processPixelQ16(pipeline, static_cast<fl::u8>(r),
                                    static_cast<fl::u8>(g),
                                    static_cast<fl::u8>(b), drives);
                    const fl::i64 driven = drivenDemandQ16(drives, model);
                    if (driven <= 0) {
                        continue;  // nothing to take a ratio against
                    }
                    const fl::i64 estimated = estimatedFromSourceCodes(
                        static_cast<fl::u8>(r), static_cast<fl::u8>(g),
                        static_cast<fl::u8>(b), model);
                    const fl::i64 ratio_milli = estimated * 1000 / driven;
                    if (ratio_milli > worst_ratio_milli) {
                        worst_ratio_milli = ratio_milli;
                    }
                    if (ratio_milli < least_ratio_milli) {
                        least_ratio_milli = ratio_milli;
                    }
                }
            }
        }
    }

    // Measured: least 1.372x, worst 129.453x. Even at its most accurate the
    // estimate is 37% high, and at its worst a strip is charged 129 times
    // what it draws.
    //
    // Bounded rather than pinned, because these are the *current* figures and
    // the prepass R3 asks for should move them a long way. What must not
    // happen is the least ratio dropping below 1.0 -- that is the case above,
    // and it is the safety property.
    FL_CHECK_GT(least_ratio_milli, 1000);   // never under, restated as a ratio
    FL_CHECK_LT(least_ratio_milli, 1500);
    FL_CHECK_GT(worst_ratio_milli, 100000); // two orders, not a rounding effect
}

} // FL_TEST_FILE

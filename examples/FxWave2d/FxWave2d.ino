

/*
This demo is best viewed using the FastLED compiler.
Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

#include <Arduino.h>
#include <FastLED.h>

#include "fl/math_macros.h"
#include "fl/time_alpha.h"
#include "fl/ui.h"
#include "fx/2d/blend.h"
#include "fx/2d/wave.h"

using namespace fl;

#define HEIGHT 64
#define WIDTH 64
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];

UITitle title("FxWave2D Demo");
UIDescription description("Advanced layered and blended wave effects.");

UIButton button("Trigger");
UIButton buttonFancy("Trigger Fancy");
UICheckbox autoTrigger("Auto Trigger", true);
UISlider triggerSpeed("Trigger Speed", .5f, 0.0f, 1.0f, 0.01f);
UICheckbox easeModeSqrt("Ease Mode Sqrt", false);
UISlider blurAmount("Global Blur Amount", 0, 0, 172, 1);
UISlider blurPasses("Global Blur Passes", 1, 1, 10, 1);
UISlider superSample("SuperSampleExponent", 1.f, 0.f, 3.f, 1.f);

UISlider speedUpper("Wave Upper: Speed", 0.12f, 0.0f, 1.0f);
UISlider dampeningUpper("Wave Upper: Dampening", 8.9f, 0.0f, 20.0f, 0.1f);
UICheckbox halfDuplexUpper("Wave Upper: Half Duplex", true);
UISlider blurAmountUpper("Wave Upper: Blur Amount", 95, 0, 172, 1);
UISlider blurPassesUpper("Wave Upper: Blur Passes", 1, 1, 10, 1);

UISlider speedLower("Wave Lower: Speed", 0.26f, 0.0f, 1.0f);
UISlider dampeningLower("Wave Lower: Dampening", 9.0f, 0.0f, 20.0f, 0.1f);
UICheckbox halfDuplexLower("Wave Lower: Half Duplex", true);
UISlider blurAmountLower("Wave Lower: Blur Amount", 0, 0, 172, 1);
UISlider blurPassesLower("Wave Lower: Blur Passes", 1, 1, 10, 1);

UISlider fancySpeed("Fancy Speed", 796, 0, 1000, 1);
UISlider fancyIntensity("Fancy Intensity", 32, 1, 255, 1);
UISlider fancyParticleSpan("Fancy Particle Span", 0.06f, 0.01f, 0.2f, 0.01f);

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   // Black
    32,  0,   0,   70,  // Dark blue
    128, 20,  57,  255, // Electric blue
    255, 255, 255, 255  // White
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    0,   0,   0,   0,   // black
    8,   128, 64,  64,  // green
    16,  255, 222, 222, // red
    64,  255, 255, 255, // white
    255, 255, 255, 255  // white
};

XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);
XYMap xyRect(WIDTH, HEIGHT, false);

WaveFx::Args CreateArgsLower() {
    WaveFx::Args out;
    out.factor = SuperSample::SUPER_SAMPLE_2X;
    out.half_duplex = true;
    out.auto_updates = true;
    out.speed = 0.18f;
    out.dampening = 9.0f;
    out.crgbMap = WaveCrgbGradientMapPtr::New(electricBlueFirePal);
    return out;
}

WaveFx::Args CreateArgsUpper() {
    WaveFx::Args out;
    out.factor = SuperSample::SUPER_SAMPLE_2X;
    out.half_duplex = true;
    out.auto_updates = true;
    out.speed = 0.25f;
    out.dampening = 3.0f;
    out.crgbMap = WaveCrgbGradientMapPtr::New(electricGreenFirePal);
    return out;
}

WaveFx waveFxLower(xyRect, CreateArgsLower());

WaveFx waveFxUpper(xyRect, CreateArgsUpper());

Blend2d fxBlend(xyMap);

void setup() {
    Serial.begin(115200);
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(screenmap);
    fxBlend.add(waveFxLower);
    fxBlend.add(waveFxUpper);
}

SuperSample getSuperSample() {
    switch (int(superSample)) {
    case 0:
        return SuperSample::SUPER_SAMPLE_NONE;
    case 1:
        return SuperSample::SUPER_SAMPLE_2X;
    case 2:
        return SuperSample::SUPER_SAMPLE_4X;
    case 3:
        return SuperSample::SUPER_SAMPLE_8X;
    default:
        return SuperSample::SUPER_SAMPLE_NONE;
    }
}

void triggerRipple() {
    float perc = .15f;
    uint8_t min_x = perc * WIDTH;
    uint8_t max_x = (1 - perc) * WIDTH;
    uint8_t min_y = perc * HEIGHT;
    uint8_t max_y = (1 - perc) * HEIGHT;
    int x = random(min_x, max_x);
    int y = random(min_y, max_y);
    waveFxLower.setf(x, y, 1);
    waveFxUpper.setf(x, y, 1);
}

void applyFancyEffect(uint32_t now, bool button_active) {
    uint32_t total =
        map(fancySpeed.as<uint32_t>(), 0, fancySpeed.getMax(), 1000, 100);
    static TimeRamp pointTransition = TimeRamp(total, 0, 0);

    if (button_active) {
        pointTransition.trigger(now, total, 0, 0);
    }

    if (!pointTransition.isActive(now)) {
        // no need to draw
        return;
    }
    int mid_x = WIDTH / 2;
    int mid_y = HEIGHT / 2;
    // now make a cross
    int amount = WIDTH / 2;
    int start_x = mid_x - amount;
    int end_x = mid_x + amount;
    int start_y = mid_y - amount;
    int end_y = mid_y + amount;
    int curr_alpha = pointTransition.update8(now);
    int left_x = map(curr_alpha, 0, 255, mid_x, start_x);
    int down_y = map(curr_alpha, 0, 255, mid_y, start_y);
    int right_x = map(curr_alpha, 0, 255, mid_x, end_x);
    int up_y = map(curr_alpha, 0, 255, mid_y, end_y);

    float curr_alpha_f = curr_alpha / 255.0f;

    float valuef = (1.0f - curr_alpha_f) * fancyIntensity.value() / 255.0f;

    int span = fancyParticleSpan.value() * WIDTH;
    for (int x = left_x - span; x < left_x + span; x++) {
        waveFxLower.addf(x, mid_y, valuef);
        waveFxUpper.addf(x, mid_y, valuef);
    }

    for (int x = right_x - span; x < right_x + span; x++) {
        waveFxLower.addf(x, mid_y, valuef);
        waveFxUpper.addf(x, mid_y, valuef);
    }

    for (int y = down_y - span; y < down_y + span; y++) {
        waveFxLower.addf(mid_x, y, valuef);
        waveFxUpper.addf(mid_x, y, valuef);
    }

    for (int y = up_y - span; y < up_y + span; y++) {
        waveFxLower.addf(mid_x, y, valuef);
        waveFxUpper.addf(mid_x, y, valuef);
    }
}

struct ui_state {
    bool button = false;
    bool bigButton = false;
};

ui_state ui() {
    U8EasingFunction easeMode = easeModeSqrt
                                    ? U8EasingFunction::WAVE_U8_MODE_SQRT
                                    : U8EasingFunction::WAVE_U8_MODE_LINEAR;
    waveFxLower.setSpeed(speedLower);
    waveFxLower.setDampening(dampeningLower);
    waveFxLower.setHalfDuplex(halfDuplexLower);
    waveFxLower.setSuperSample(getSuperSample());
    waveFxLower.setEasingMode(easeMode);

    waveFxUpper.setSpeed(speedUpper);
    waveFxUpper.setDampening(dampeningUpper);
    waveFxUpper.setHalfDuplex(halfDuplexUpper);
    waveFxUpper.setSuperSample(getSuperSample());
    waveFxUpper.setEasingMode(easeMode);
    fxBlend.setGlobalBlurAmount(blurAmount);
    fxBlend.setGlobalBlurPasses(blurPasses);

    Blend2dParams lower_params = {
        .blur_amount = blurAmountLower,
        .blur_passes = blurPassesLower,
    };

    Blend2dParams upper_params = {
        .blur_amount = blurAmountUpper,
        .blur_passes = blurPassesUpper,
    };

    fxBlend.setParams(waveFxLower, lower_params);
    fxBlend.setParams(waveFxUpper, upper_params);
    ui_state state{
        .button = button,
        .bigButton = buttonFancy,
    };
    return state;
}

void processAutoTrigger(uint32_t now) {
    static uint32_t nextTrigger = 0;
    uint32_t trigger_delta = nextTrigger - now;
    if (trigger_delta > 10000) {
        // rolled over!
        trigger_delta = 0;
    }
    if (autoTrigger) {
        if (now >= nextTrigger) {
            triggerRipple();
            float speed = 1.0f - triggerSpeed.value();
            uint32_t min_rand = 400 * speed;
            uint32_t max_rand = 2000 * speed;

            uint32_t min = MIN(min_rand, max_rand);
            uint32_t max = MAX(min_rand, max_rand);
            if (min == max) {
                max += 1;
            }
            nextTrigger = now + random(min, max);
        }
    }
}

void loop() {
    // Your code here
    uint32_t now = millis();
    ui_state state = ui();
    if (state.button) {
        triggerRipple();
    }
    applyFancyEffect(now, state.bigButton);
    processAutoTrigger(now);
    Fx::DrawContext ctx(now, leds);
    fxBlend.draw(ctx);
    FastLED.show();
}
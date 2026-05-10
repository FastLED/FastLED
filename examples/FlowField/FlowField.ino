// @filter: (memory is large)

/// @file    FlowField.ino
/// @brief   2D flow field visualization: emitters paint color, noise advects it
/// @example FlowField.ino
///
/// Concept by Stefan Petrick. Emitters inject color onto a float-precision 2D
/// grid, then a noise-driven flow field advects (transports) those colors with
/// bilinear interpolation, creating emergent fluid-like patterns.
///
/// Original C++ implementation by 4wheeljive (ColorTrails project).
/// Distilled into a self-contained FastLED example.
///
/// This sketch is fully compatible with the FastLED web compiler. To use it:
/// 1. Install FastLED: `pip install fastled`
/// 2. cd into this examples directory
/// 3. Run: `fastled`
/// 4. A browser window will open with the preview.

// UIDescription: Flow field visualization with noise-driven advection, creating
// fluid-like patterns from color emitters.

#include <FastLED.h>
#include "fl/ui/ui.h"
#include "fl/fx/2d/flowfield.h"

// -- Matrix config --
#define WIDTH 64
#define HEIGHT 64
#define NUM_LEDS (WIDTH * HEIGHT)
#define DATA_PIN 2
#define BRIGHTNESS 255
#define SERPENTINE true

CRGB leds[NUM_LEDS];
fl::XYMap xyMap(WIDTH, HEIGHT, SERPENTINE);

// -- UI controls --
fl::UITitle title("FlowFields");
fl::UIDescription description(
    "Flow field visualization with noise-driven advection, creating fluid-like patterns from color emitters. "
    "Concept by Stefan Petrick, Initial C++ implementation by 4wheeljive. FastLED port adaptation + fixed point "
    "optmization by Zach Vorhies."
);

fl::UISlider flowSpeedX("X Speed", 0.10f, -2.0f, 2.0f, 0.01f);
fl::UISlider flowAmpX("X Amplitude", 1.0f, 0.0f, 2.0f, 0.01f);
fl::UISlider flowFreqX("X Frequency", 0.33f, 0.05f, 4.0f, 0.01f);
fl::UIGroup flowXGroup("Flow X", flowSpeedX, flowAmpX, flowFreqX);

fl::UISlider flowSpeedY("Y Speed", 0.10f, -2.0f, 2.0f, 0.01f);
fl::UISlider flowAmpY("Y Amplitude", 1.0f, 0.0f, 2.0f, 0.01f);
fl::UISlider flowFreqY("Y Frequency", 0.32f, 0.05f, 4.0f, 0.01f);
fl::UIGroup flowYGroup("Flow Y", flowSpeedY, flowAmpY, flowFreqY);

fl::UISlider endpointSpeed("Endpoint Speed", 0.80f, 0.05f, 2.0f, 0.01f);
fl::UISlider colorShift("Color Shift", 0.04f, 0.0f, 0.5f, 0.01f);
fl::UISlider persistence("Trail Half-Life (s)", 0.86f, 0.05f, 5.0f, 0.01f);
fl::UISlider flowShift("Pixel Shift", 1.8f, 0.5f, 4.0f, 0.1f);
fl::UISlider numDots("Dots", 3, 1, 5, 1);
fl::UIDropdown emitterMode("Emitter Mode", {"Lissajous", "Dots", "Both"});
fl::UIGroup appearanceGroup("Appearance", endpointSpeed, colorShift, persistence, flowShift, numDots, emitterMode);

fl::UIButton noisePunch("NoisePunch");

fl::UIDropdown computeMode("Compute Mode", {"Float", "Fixed-Point (Fast)"});
fl::UICheckbox showFlowVectors("Show Flow Vectors", false);
fl::UIGroup debugGroup("Debug", computeMode, showFlowVectors);

fl::FlowFieldFloat flowFieldFloat(xyMap);
fl::FlowFieldFP flowFieldFP(xyMap);

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS)
        .setScreenMap(xyMap);
    FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
    fl::Fx::DrawContext ctx(millis(), leds);

    // Use base class reference for uniform parameter setting + draw
    fl::FlowField &fx = (computeMode.as_int() == 0)
        ? static_cast<fl::FlowField &>(flowFieldFloat)
        : static_cast<fl::FlowField &>(flowFieldFP);

    fx.setFlowSpeedX(flowSpeedX);
    fx.setFlowAmplitudeX(flowAmpX);
    fx.setNoiseFrequencyX(flowFreqX);
    fx.setFlowSpeedY(flowSpeedY);
    fx.setFlowAmplitudeY(flowAmpY);
    fx.setNoiseFrequencyY(flowFreqY);
    fx.setEndpointSpeed(endpointSpeed);
    fx.setColorShift(colorShift);
    fx.setPersistence(persistence);
    fx.setFlowShift(flowShift);
    fx.setDotCount(numDots.as<int>());
    fx.setEmitterMode(emitterMode.as_int());
    fx.setShowFlowVectors(showFlowVectors);

    if (noisePunch.clicked()) {
        // 50% X axis, 50% Y axis; random position; random sign
        bool pickX = random8() < 128;
        float sign = (random8() < 128) ? 1.0f : -1.0f;
        if (pickX) {
            float cx = random8(0, fx.getWidth());
            float w = fx.getWidth() * 0.4f;
            fx.noisePunchX(cx, w, sign);
        } else {
            float cy = random8(0, fx.getHeight());
            float h = fx.getHeight() * 0.4f;
            fx.noisePunchY(cy, h, sign);
        }
    }

    fx.draw(ctx);
    FastLED.show();
}

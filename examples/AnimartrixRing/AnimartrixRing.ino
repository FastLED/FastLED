// @filter: (memory is large)

/// @file    AnimartrixRing.ino
/// @brief   Sample a circular LED ring out of a 2D Animartrix grid.
/// @example AnimartrixRing.ino
///
/// This is the *minimal* demo of the technique: render Animartrix into a
/// rectangular grid, then sample a circle out of that grid with a ScreenMap and
/// Fx2dTo1d. Nothing else. No audio, no state machine, no orchestration.
///
///   Animartrix (16x16 grid) -> ScreenMap (circle of NUM_LEDS points)
///                           -> Fx2dTo1d (bilinear sample) -> 1D ring
///
/// Pick an animation from the dropdown, scrub the speed slider, watch how each
/// Animartrix pattern reads once it has been wrapped around a ring. That is the
/// whole point of this sketch.
///
/// Looking for the audio-reactive version? That moved to `examples/MoodRing/`,
/// which is the product-track sketch (sound classification, per-state visual
/// banks, mood mapping). See issue #2256.
///
/// This sketch is fully compatible with the FastLED web compiler:
/// 1. `pip install fastled`
/// 2. cd into this directory
/// 3. run `fastled`

// Use SPI-based WS2812 driver instead of RMT on ESP32
#define FASTLED_ESP32_USE_CLOCKLESS_SPI

// FastLED.h must be included first to trigger precompiled headers for FastLED's
// build system
#include "FastLED.h"

#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/fx2d_to_1d.h"
#include "fl/fx/fx_engine.h"
#include "fl/ui/ui.h"

#include "auto_brightness.h"
#include "ring_screenmap.h"

FASTLED_TITLE("AnimartrixRing");

#define NUM_LEDS 244

#ifndef PIN_DATA
#define PIN_DATA 3 // ESP32C6 has this random pin available on the break out.
#endif             // PIN_DATA

#define BRIGHTNESS 8

// Grid dimensions for Animartrix sampling
#define GRID_WIDTH 16
#define GRID_HEIGHT 16

// 0.15 cm or 1.5mm -- appropriate for a dense LED rope.
#define LED_DIAMETER 0.15f

CRGB leds[NUM_LEDS];

// Animartrix 2D effect: the source image the ring samples from.
XYMap xymap = XYMap::constructRectangularGrid(GRID_WIDTH, GRID_HEIGHT);
auto animartrix =
    fl::make_shared<fl::Animartrix>(xymap, fl::AnimartrixAnim::SLOW_FADE);

// ScreenMap for the ring: places each LED on a circle inside the grid.
fl::ScreenMap screenmap =
    makeRingScreenMap(NUM_LEDS, GRID_WIDTH, GRID_HEIGHT, LED_DIAMETER);

// The 2D-to-1D sampling effect: bilinear-samples the grid at each ring point.
auto fx2dTo1d = fl::make_shared<fl::Fx2dTo1d>(NUM_LEDS, animartrix, screenmap,
                                              fl::Fx2dTo1d::BILINEAR);

// FxEngine for the 1D strip
fl::FxEngine fxEngine(NUM_LEDS);

// UI controls. The dropdown order matches fl::AnimartrixAnim exactly, so the
// selected index can be handed straight to Animartrix::fxSet().
fl::UITitle title("AnimartrixRing");
fl::UIDescription description(
    "Samples a circular ring out of a 2D Animartrix grid. @author of fx is "
    "StefanPetrick");

fl::UIDropdown fxIndex("Animartrix - index", {
    "RGB_BLOBS5", "RGB_BLOBS4", "RGB_BLOBS3", "RGB_BLOBS2", "RGB_BLOBS",
    "POLAR_WAVES", "SLOW_FADE", "ZOOM2", "ZOOM", "HOT_BLOB",
    "SPIRALUS2", "SPIRALUS", "YVES", "SCALEDEMO1", "LAVA1",
    "CALEIDO3", "CALEIDO2", "CALEIDO1", "DISTANCE_EXPERIMENT", "CENTER_FIELD",
    "WAVES", "CHASING_SPIRALS", "ROTATING_BLOB", "RINGS", "COMPLEX_KALEIDO",
    "COMPLEX_KALEIDO_2", "COMPLEX_KALEIDO_3", "COMPLEX_KALEIDO_4", "COMPLEX_KALEIDO_5", "COMPLEX_KALEIDO_6",
    "WATER", "PARAMETRIC_WATER", "MODULE_EXPERIMENT1", "MODULE_EXPERIMENT2", "MODULE_EXPERIMENT3",
    "MODULE_EXPERIMENT4", "MODULE_EXPERIMENT5", "MODULE_EXPERIMENT6", "MODULE_EXPERIMENT7", "MODULE_EXPERIMENT8",
    "MODULE_EXPERIMENT9", "MODULE_EXPERIMENT10", "MODULE_EXPERIMENT_SM1", "MODULE_EXPERIMENT_SM2", "MODULE_EXPERIMENT_SM3",
    "MODULE_EXPERIMENT_SM4", "MODULE_EXPERIMENT_SM5", "MODULE_EXPERIMENT_SM6", "MODULE_EXPERIMENT_SM8", "MODULE_EXPERIMENT_SM9",
    "MODULE_EXPERIMENT_SM10", "FLUFFY_BLOBS"
});
fl::UISlider timeSpeed("Time Speed", 1, -10, 10, .1);
fl::UISlider brightness("Brightness", BRIGHTNESS, 0, 255, 1);
fl::UICheckbox autoBrightness("Auto Brightness", true);
fl::UISlider autoBrightnessMax("Auto Brightness Max", 84, 0, 255, 1);
fl::UISlider autoBrightnessLowThreshold("Auto Brightness Low Threshold", 8, 0,
                                        100, 1);
fl::UISlider autoBrightnessHighThreshold("Auto Brightness High Threshold", 22,
                                         0, 100, 1);

void setup() {
    Serial.begin(115200);

    // Setup LED strip
    FastLED.addLeds<WS2812, PIN_DATA>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenmap);
    FastLED.setBrightness(brightness.value());

    // Add the 2D-to-1D effect to FxEngine
    fxEngine.addFx(fx2dTo1d);

    // Dropdown index maps 1:1 onto fl::AnimartrixAnim.
    fxIndex.onChanged([](fl::UIDropdown &dropdown) {
        animartrix->fxSet(dropdown.as_int());
    });

    Serial.println("AnimartrixRing setup complete");
}

void loop() {
    fxEngine.setSpeed(timeSpeed.value());

    // Draw the effect
    fxEngine.draw(millis(), leds);

    // Calculate final brightness
    uint8_t finalBrightness;
    if (autoBrightness.value()) {
        float avgBri = getAverageBrightness(leds, NUM_LEDS);
        finalBrightness = applyBrightnessCompression(
            avgBri, static_cast<uint8_t>(autoBrightnessMax.value()),
            autoBrightnessLowThreshold.value(),
            autoBrightnessHighThreshold.value());
    } else {
        finalBrightness = static_cast<uint8_t>(brightness.value());
    }

    FastLED.setBrightness(finalBrightness);
    FastLED.show();
}

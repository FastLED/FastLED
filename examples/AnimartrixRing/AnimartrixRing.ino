// @filter: (memory is high)

// AnimartrixRing: Sample a circle from an Animartrix rectangular grid
// This example generates a rectangular animation grid and samples a circular
// region from it to display on a ring of LEDs using Fx2dTo1d.

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include <FastLED.h>
#include "fl/math_macros.h"
#include "fl/screenmap.h"
#include "fl/ui.h"
#include "fx/2d/animartrix.hpp"
#include "fx/fx2d_to_1d.h"
#include "fx/fx_engine.h"

#ifndef TWO_PI
#define TWO_PI 6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359
#endif

#define NUM_LEDS 60
#define DATA_PIN 3
#define BRIGHTNESS 128

// Grid dimensions for Animartrix sampling
#define GRID_WIDTH 32
#define GRID_HEIGHT 32

CRGB leds[NUM_LEDS];

// Animartrix 2D effect
XYMap xymap = XYMap::constructRectangularGrid(GRID_WIDTH, GRID_HEIGHT);
auto animartrix = fl::make_shared<fl::Animartrix>(xymap, fl::RGB_BLOBS5);
int currentAnimationIndex = 0;

// ScreenMap for the ring - defines circular sampling positions using a lambda
fl::ScreenMap screenmap = fl::ScreenMap(NUM_LEDS, 0.5f, [](int index, fl::vec2f& pt_out) {
    float centerX = GRID_WIDTH / 2.0f;
    float centerY = GRID_HEIGHT / 2.0f;
    float radius = min(GRID_WIDTH, GRID_HEIGHT) / 2.0f - 1;
    float angle = (TWO_PI * index) / NUM_LEDS;
    pt_out.x = centerX + cos(angle) * radius;
    pt_out.y = centerY + sin(angle) * radius;
});

// Create the 2D-to-1D sampling effect
auto fx2dTo1d = fl::make_shared<fl::Fx2dTo1d>(
    NUM_LEDS,
    animartrix,
    screenmap,
    fl::Fx2dTo1d::BILINEAR
);

// FxEngine for the 1D strip
fl::FxEngine fxEngine(NUM_LEDS);

// Helper function to get animation names for dropdown
fl::vector<fl::string> getAnimationNames() {
    fl::vector<fl::pair<int, fl::string>> animList = fl::Animartrix::getAnimationList();
    fl::vector<fl::string> names;
    for (const auto& item : animList) {
        names.push_back(item.second);
    }
    return names;
}

// Store animation names in a static variable so they persist
static fl::vector<fl::string> animationNames = getAnimationNames();

// UI controls
fl::UIDropdown animationSelector("Animation", animationNames);
fl::UISlider timeSpeed("Time Speed", 1, -10, 10, .1);
fl::UISlider brightness("Brightness", BRIGHTNESS, 0, 255, 1);
fl::UICheckbox autoBrightness("Auto Brightness", true);
fl::UISlider autoBrightnessMax("Auto Brightness Max", 84, 0, 255, 1);
fl::UISlider autoBrightnessLowThreshold("Auto Brightness Low Threshold", 8, 0, 100, 1);
fl::UISlider autoBrightnessHighThreshold("Auto Brightness High Threshold", 22, 0, 100, 1);

// Calculate average brightness percentage from LED array
float getAverageBrightness(CRGB* leds, int numLeds) {
    uint32_t total = 0;
    for (int i = 0; i < numLeds; i++) {
        total += leds[i].r + leds[i].g + leds[i].b;
    }
    float avgValue = float(total) / float(numLeds * 3);
    return (avgValue / 255.0f) * 100.0f; // Return as percentage
}

// Apply compression curve: input brightness % -> output brightness multiplier
uint8_t applyBrightnessCompression(float inputBrightnessPercent, uint8_t maxBrightness, float lowThreshold, float highThreshold) {
    float maxBrightnessPercent = (maxBrightness / 255.0f) * 100.0f;
    if (inputBrightnessPercent < lowThreshold) {
        // Below lowThreshold: full brightness (100%)
        return 255;
    } else if (inputBrightnessPercent < highThreshold) {
        // lowThreshold-highThreshold: linear dampening from 100% down to maxBrightnessPercent
        float range = highThreshold - lowThreshold;
        float progress = (inputBrightnessPercent - lowThreshold) / range; // 0 to 1
        float targetPercent = 100.0f - (progress * (100.0f - maxBrightnessPercent));
        return static_cast<uint8_t>((targetPercent / 100.0f) * 255.0f);
    } else {
        // Above highThreshold: cap to maxBrightness
        return maxBrightness;
    }
}

void setup() {
    Serial.begin(115200);

    // Setup LED strip
    fl::ScreenMap screenMapLocal(screenmap);
    screenMapLocal.setDiameter(0.5);
    FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenMapLocal);
    FastLED.setBrightness(brightness);

    // Add the 2D-to-1D effect to FxEngine
    fxEngine.addFx(fx2dTo1d);

    // Setup animation selector callback
    animationSelector.onChanged([](fl::UIDropdown &dropdown) {
        int index = dropdown.as_int();
        animartrix->fxSet(index);
        Serial.print("Animation changed to: ");
        Serial.println(index);
    });

    Serial.println("AnimartrixRing setup complete");
}

void loop() {
    // Update animation
    fxEngine.setSpeed(timeSpeed);

    // Draw the effect
    fxEngine.draw(millis(), leds);

    // Calculate and apply auto brightness if enabled
    if (autoBrightness) {
        float avgBri = getAverageBrightness(leds, NUM_LEDS);
        uint8_t newBri = applyBrightnessCompression(
            avgBri,
            static_cast<uint8_t>(autoBrightnessMax),
            autoBrightnessLowThreshold,
            autoBrightnessHighThreshold
        );
        FastLED.setBrightness(newBri);
    } else {
        FastLED.setBrightness(static_cast<uint8_t>(brightness));
    }

    // Show the LEDs
    FastLED.show();
}

// AnimartrixRing: Sample a circle from an Animartrix rectangular grid
// This example generates a rectangular animation grid and samples a circular
// region from it to display on a ring of LEDs using Fx2dTo1d.

#include "fl/sketch_macros.h"
#if !SKETCH_HAS_LOTS_OF_MEMORY
#include "platforms/sketch_fake.hpp"
#else

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

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    fxEngine.addFx(fx2dTo1d);

    // Print available Animartrix animations
    Serial.println("Available Animartrix animations:");
    for (int i = 0; i < fl::getAnimartrixCount(); i++) {
        fl::AnimartrixAnimInfo info = fl::getAnimartrixInfo(i);
        Serial.print(i);
        Serial.print(": ");
        Serial.println(info.name);
    }
}

void loop() {
    static int lastSelectedIndex = -1;

    // Check if the dropdown selection has changed
    int selectedIndex = animationSelector.as_int();
    if (selectedIndex != lastSelectedIndex) {
        currentAnimationIndex = selectedIndex;
        animartrix->fxSet(currentAnimationIndex);

        // Print the current animation name
        fl::AnimartrixAnimInfo info = fl::getAnimartrixInfo(currentAnimationIndex);
        Serial.print("Switching to: ");
        Serial.println(info.name);

        lastSelectedIndex = selectedIndex;
    }

    // Update brightness
    FastLED.setBrightness(brightness.as_int());

    // Draw the effect (Fx2dTo1d handles 2D rendering and sampling)
    fxEngine.setSpeed(timeSpeed);
    fxEngine.draw(millis(), leds);

    FastLED.show();
}

#endif  // SKETCH_HAS_LOTS_OF_MEMORY

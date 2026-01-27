/// @file    FxParticles.ino
/// @brief   Particle system with power-based lifecycle
/// @example FxParticles.ino
///
/// fl::UIDescription: Particle system with power-based lifecycle. Particles spawn with energy and gradually slow, dim, and saturate as they exhaust. Features overdraw rendering for smooth trails.
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <FastLED.h>

#include "fl/fx/1d/particles.h"
#include "fl/screenmap.h"
#include "fl/ui.h"


#define NUM_LEDS 210
#define PARTICLE_MAX 10
#define PARTICLE_GROUPS 2


#define DATA_PIN 3

fl::CRGB leds[NUM_LEDS];

// UI Controls
fl::UISlider uiSpeed("Avg Speed", 1.0, 0.1, 3.0, 0.1);
fl::UISlider uiFadeAmount("Trail Length", 2, 0, 10, 1);
fl::UISlider uiBrightness("Brightness", 64, 0, 255, 1);
fl::UISlider uiLifetime("Avg Lifetime (sec)", 4.0, 0.5, 8.0, 0.5);
fl::UISlider uiSpawnInterval("Spawn Interval (sec)", 2.0, 0.1, 10.0, 0.1);
fl::UISlider uiOverdrawCount("Overdraw Count", 20, 1, 50, 1);
fl::UICheckbox uiCyclical("Cyclical", true);
fl::UICheckbox uiAutoSpawn("Auto Spawn", true);
fl::UIButton uiSpawnButton("Spawn Particle");

// Create particles effect with adjusted parameters for memory-constrained platforms
fl::Particles1d particles(NUM_LEDS, PARTICLE_MAX, PARTICLE_GROUPS);

// External spawn management
uint32_t lastSpawnTime = 0;

void setup() {
    fl::ScreenMap screenMap = fl::ScreenMap::Circle(NUM_LEDS, 5.0, 5.0);
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setScreenMap(screenMap);
    FastLED.setBrightness(64);
}

void loop() {
    uint32_t now = fl::millis();

    // Update effect parameters from UI
    FastLED.setBrightness(uiBrightness.as<int>());
    particles.setSpeed(uiSpeed.as<float>());
    particles.setFadeRate(uiFadeAmount.as<int>());
    particles.setLifetime(uiLifetime.as<float>() * 1000);
    particles.setOverdrawCount(uiOverdrawCount.as<int>());
    particles.setCyclical(uiCyclical.value());

    // Manual spawn
    if (uiSpawnButton.clicked()) {
        particles.spawnRandomParticle();
    }

    // Auto spawn (external control)
    if (uiAutoSpawn.value() && now - lastSpawnTime >= uiSpawnInterval.as<float>() * 1000) {
        particles.spawnRandomParticle();
        lastSpawnTime = now;
    }

    // Draw particles
    particles.draw(fl::Fx::DrawContext(now, leds));

    FastLED.show();
    delay(20);
}

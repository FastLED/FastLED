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
#include "fl/audio_reactive.h"
#include "fx/2d/animartrix.hpp"
#include "fx/fx2d_to_1d.h"
#include "fx/fx_engine.h"
#include "fx/audio/audio_processor.h"

#ifndef TWO_PI
#define TWO_PI 6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359
#endif

#define NUM_LEDS 144
#define DATA_PIN 4
#define BRIGHTNESS 8

// Grid dimensions for Animartrix sampling
#define GRID_WIDTH 32
#define GRID_HEIGHT 32

CRGB leds[NUM_LEDS];

// Animartrix 2D effect
XYMap xymap = XYMap::constructRectangularGrid(GRID_WIDTH, GRID_HEIGHT);
auto animartrix = fl::make_shared<fl::Animartrix>(xymap, fl::RGB_BLOBS5);
int currentAnimationIndex = 0;

// ScreenMap for the ring - defines circular sampling positions using a lambda
fl::ScreenMap screenmap = fl::ScreenMap(NUM_LEDS, 0.15f, [](int index, fl::vec2f& pt_out) {
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

// Audio UI controls
fl::UIAudio audio("Audio Input");
fl::UICheckbox enableAudioReactive("Enable Audio Reactive", false);
fl::UICheckbox audioAffectsSpeed("Audio Affects Speed", true);
fl::UICheckbox audioAffectsBrightness("Audio Affects Brightness", false);
fl::UISlider audioSpeedMultiplier("Audio Speed Multiplier", 5.0, 0.0, 20.0, 0.5);
fl::UISlider audioBrightnessMultiplier("Audio Brightness Multiplier", 1.5, 0.0, 3.0, 0.1);
fl::UISlider audioSensitivity("Audio Sensitivity", 128, 0, 255, 1);

// Audio reactive processor
fl::AudioReactive audioReactive;

// Audio processor for downbeat detection
fl::AudioProcessor audioProcessor;
bool flashWhiteThisFrame = false;

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
    screenMapLocal.setDiameter(0.15);  // 0.15 cm or 1.5mm - appropriate for dense 144 LED rope
    FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenMapLocal);
    FastLED.setBrightness(brightness.value());

    // Add the 2D-to-1D effect to FxEngine
    fxEngine.addFx(fx2dTo1d);

    // Setup animation selector callback
    animationSelector.onChanged([](fl::UIDropdown &dropdown) {
        int index = dropdown.as_int();
        animartrix->fxSet(index);
        Serial.print("Animation changed to: ");
        Serial.println(index);
    });

    // Initialize audio reactive processor
    fl::AudioReactiveConfig audioConfig;
    audioConfig.gain = 128;
    audioConfig.sensitivity = audioSensitivity.as_int();
    audioConfig.agcEnabled = true;
    audioConfig.sampleRate = 22050;
    audioReactive.begin(audioConfig);

    // Setup downbeat detector callback
    audioProcessor.onDownbeat([](){
        flashWhiteThisFrame = true;
    });

    Serial.println("AnimartrixRing setup complete");
}

void loop() {
    // Process audio if enabled
    float audioSpeedFactor = 1.0f;
    float audioBrightnessFactor = 1.0f;

    if (enableAudioReactive.value()) {
        // Process audio samples
        fl::AudioSample sample = audio.next();
        if (sample.isValid()) {
            audioReactive.processSample(sample);

            const fl::AudioData& audioData = audioReactive.getSmoothedData();

            // Map volume to speed (0-1 range, scaled by multiplier)
            if (audioAffectsSpeed.value()) {
                float normalizedVolume = audioData.volume / 255.0f;
                audioSpeedFactor = 1.0f + (normalizedVolume * audioSpeedMultiplier.value());
            }

            // Map volume to brightness
            if (audioAffectsBrightness.value()) {
                float normalizedVolume = audioData.volume / 255.0f;
                audioBrightnessFactor = fl::fl_max(0.3f, normalizedVolume * audioBrightnessMultiplier.value());
            }

            // Process audio for downbeat detection
            audioProcessor.update(sample);
        }
    }

    // Update animation with audio-reactive speed
    float effectiveSpeed = timeSpeed.value() * audioSpeedFactor;
    fxEngine.setSpeed(effectiveSpeed);

    // Draw the effect
    fxEngine.draw(millis(), leds);

    // Flash white on downbeat
    if (flashWhiteThisFrame) {
        fill_solid(leds, NUM_LEDS, CRGB::White);
        flashWhiteThisFrame = false;
    }

    // Calculate final brightness
    uint8_t finalBrightness;
    if (autoBrightness.value()) {
        float avgBri = getAverageBrightness(leds, NUM_LEDS);
        finalBrightness = applyBrightnessCompression(
            avgBri,
            static_cast<uint8_t>(autoBrightnessMax.value()),
            autoBrightnessLowThreshold.value(),
            autoBrightnessHighThreshold.value()
        );
    } else {
        finalBrightness = static_cast<uint8_t>(brightness.value());
    }

    // Apply audio brightness multiplier if enabled
    if (enableAudioReactive.value() && audioAffectsBrightness.value()) {
        finalBrightness = static_cast<uint8_t>(finalBrightness * audioBrightnessFactor);
    }

    FastLED.setBrightness(finalBrightness);

    // Show the LEDs
    FastLED.show();
}

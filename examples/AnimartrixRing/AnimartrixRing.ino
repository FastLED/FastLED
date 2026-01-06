// @filter: (memory is high)

// AnimartrixRing: Sample a circle from an Animartrix rectangular grid
// This example generates a rectangular animation grid and samples a circular
// region from it to display on a ring of LEDs using Fx2dTo1d.

// Use SPI-based WS2812 driver instead of RMT on ESP32
#define FASTLED_ESP32_USE_CLOCKLESS_SPI

// FastLED.h must be included first to trigger precompiled headers for FastLED's
// build system
#include "FastLED.h"

#include "fl/audio_reactive.h"
#include "fl/math_macros.h"
#include "fl/screenmap.h"
#include "fl/ui.h"
#include "fl/stl/math.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/audio/audio_processor.h"
#include "fl/fx/fx2d_to_1d.h"
#include "fl/fx/fx_engine.h"
#include <FastLED.h>

#ifndef TWO_PI
#define TWO_PI                                                                 \
    6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359
#endif

#define NUM_LEDS 244

#ifndef PIN_DATA
#define PIN_DATA 3 // ESP32C6 has this random pin available on the break out.
#endif             // PIN_DATA

#define BRIGHTNESS 8

// Grid dimensions for Animartrix sampling
#define GRID_WIDTH 16
#define GRID_HEIGHT 16

CRGB leds[NUM_LEDS];

// Animartrix 2D effect
XYMap xymap = XYMap::constructRectangularGrid(GRID_WIDTH, GRID_HEIGHT);
auto animartrix = fl::make_shared<fl::Animartrix>(xymap, fl::RGB_BLOBS5);
int currentAnimationIndex = 0;

// ScreenMap for the ring - defines circular sampling positions using a lambda
fl::ScreenMap screenmap =
    fl::ScreenMap(NUM_LEDS, 0.15f, [](int index, fl::vec2f &pt_out) {
        float centerX = GRID_WIDTH / 2.0f;
        float centerY = GRID_HEIGHT / 2.0f;
        float radius = fl::fl_min(GRID_WIDTH, GRID_HEIGHT) / 2.0f - 1;
        float angle = (TWO_PI * index) / NUM_LEDS;
        pt_out.x = centerX + fl::cos(angle) * radius;
        pt_out.y = centerY + fl::sin(angle) * radius;
    });

// Create the 2D-to-1D sampling effect
auto fx2dTo1d = fl::make_shared<fl::Fx2dTo1d>(NUM_LEDS, animartrix, screenmap,
                                              fl::Fx2dTo1d::BILINEAR);

// FxEngine for the 1D strip
fl::FxEngine fxEngine(NUM_LEDS);

// Helper function to get animation names for dropdown
fl::vector<fl::string> getAnimationNames() {
    fl::vector<fl::pair<int, fl::string>> animList =
        fl::Animartrix::getAnimationList();
    fl::vector<fl::string> names;
    for (const auto &item : animList) {
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
fl::UISlider autoBrightnessLowThreshold("Auto Brightness Low Threshold", 8, 0,
                                        100, 1);
fl::UISlider autoBrightnessHighThreshold("Auto Brightness High Threshold", 22,
                                         0, 100, 1);

// Audio UI controls
fl::UIAudio audio("Audio Input");
fl::UICheckbox enableAudioReactive("Enable Audio Reactive", false);
fl::UICheckbox audioAffectsSpeed("Audio Affects Speed", true);
fl::UICheckbox audioAffectsBrightness("Audio Affects Brightness", false);
fl::UISlider audioSpeedMultiplier("Audio Speed Multiplier", 5.0, 0.0, 20.0,
                                  0.5);
fl::UISlider audioBrightnessMultiplier("Audio Brightness Multiplier", 1.5, 0.0,
                                       3.0, 0.1);
fl::UISlider audioSensitivity("Audio Sensitivity", 128, 0, 255, 1);

// Downbeat darkness UI controls
fl::UICheckbox enableDownbeatDarkness("Enable Downbeat Darkness", true);
fl::UISlider downbeatDarknessAmount("Downbeat Darkness Amount", 0.3, 0.0, 1.0, 0.05);
fl::UISlider darknessFadeDuration("Darkness Fade Duration (ms)", 300, 50, 1000, 50);

// Audio reactive processor
fl::AudioReactive audioReactive;

// Audio processor for downbeat detection
fl::AudioProcessor audioProcessor;

// Downbeat darkness state
bool darknessTrigger = false;
float darknessAmount = 1.0f; // 1.0 = no darkness, 0.0 = full darkness
unsigned long lastDarknessTime = 0;

// Audio timeout tracking
unsigned long lastValidAudioTime = 0;
const unsigned long AUDIO_TIMEOUT_MS = 1000;

// Calculate average brightness percentage from LED array
float getAverageBrightness(CRGB *leds, int numLeds) {
    uint32_t total = 0;
    for (int i = 0; i < numLeds; i++) {
        total += leds[i].r + leds[i].g + leds[i].b;
    }
    float avgValue = float(total) / float(numLeds * 3);
    return (avgValue / 255.0f) * 100.0f; // Return as percentage
}

// Apply compression curve: input brightness % -> output brightness multiplier
uint8_t applyBrightnessCompression(float inputBrightnessPercent,
                                   uint8_t maxBrightness, float lowThreshold,
                                   float highThreshold) {
    float maxBrightnessPercent = (maxBrightness / 255.0f) * 100.0f;
    if (inputBrightnessPercent < lowThreshold) {
        // Below lowThreshold: full brightness (100%)
        return 255;
    } else if (inputBrightnessPercent < highThreshold) {
        // lowThreshold-highThreshold: linear dampening from 100% down to
        // maxBrightnessPercent
        float range = highThreshold - lowThreshold;
        float progress =
            (inputBrightnessPercent - lowThreshold) / range; // 0 to 1
        float targetPercent =
            100.0f - (progress * (100.0f - maxBrightnessPercent));
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
    screenMapLocal.setDiameter(
        0.15); // 0.15 cm or 1.5mm - appropriate for dense 144 LED rope
    FastLED.addLeds<WS2812, PIN_DATA>(leds, NUM_LEDS)
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
    audioProcessor.onDownbeat([]() {
        darknessTrigger = true;
        FL_WARN("Downbeat detected! Triggering darkness effect");
    });

    Serial.println("AnimartrixRing setup complete");
    FL_WARN("Setup complete - Audio Reactive: " << enableAudioReactive.value()
            << ", Downbeat Darkness: " << enableDownbeatDarkness.value());
}

void loop() {
    // Process audio if enabled
    float audioSpeedFactor = 1.0f;
    float audioBrightnessFactor = 1.0f;

    EVERY_N_MILLISECONDS(3000) {
        FL_WARN("Audio Reactive Enabled: " << enableAudioReactive.value()
                << ", Downbeat Darkness Enabled: " << enableDownbeatDarkness.value());
    }

    // Track audio reactive state transitions
    static bool previousAudioReactiveState = false;

    if (enableAudioReactive.value()) {
        // Initialize timer when first enabled
        if (!previousAudioReactiveState) {
            lastValidAudioTime = millis();
            FL_WARN("Audio Reactive ENABLED - starting audio processing (1000ms timeout)");
            previousAudioReactiveState = true;
        }

        // Process audio samples
        fl::AudioSample sample = audio.next();
        if (sample.isValid()) {
            lastValidAudioTime = millis(); // Update last valid audio timestamp
            audioReactive.processSample(sample);

            const fl::AudioData &audioData = audioReactive.getSmoothedData();
            EVERY_N_MILLISECONDS(2000) {
                FL_WARN("Audio processing active - Volume: " << (int)audioData.volume);
            }

            // Map volume to speed (0-1 range, scaled by multiplier)
            if (audioAffectsSpeed.value()) {
                float normalizedVolume = audioData.volume / 255.0f;
                audioSpeedFactor =
                    1.0f + (normalizedVolume * audioSpeedMultiplier.value());
            }

            // Map volume to brightness
            if (audioAffectsBrightness.value()) {
                float normalizedVolume = audioData.volume / 255.0f;
                audioBrightnessFactor = fl::fl_max(
                    0.3f, normalizedVolume * audioBrightnessMultiplier.value());
            }

            // Process audio for downbeat detection
            audioProcessor.update(sample);
        } else {
            // Check for audio timeout - auto-disable if no valid audio for 1000ms
            unsigned long currentTime = millis();
            if (lastValidAudioTime > 0 && (currentTime - lastValidAudioTime) > AUDIO_TIMEOUT_MS) {
                FL_WARN("No valid audio for " << AUDIO_TIMEOUT_MS << "ms - AUTO-DISABLING Audio Reactive");
                enableAudioReactive = false; // Use assignment operator
                lastValidAudioTime = 0; // Reset timer
            } else {
                EVERY_N_MILLISECONDS(5000) {
                    FL_WARN("Audio sample is INVALID - check microphone/audio input");
                }
            }
        }
    } else {
        // Reset state when disabled
        if (previousAudioReactiveState) {
            FL_WARN("Audio Reactive DISABLED");
            previousAudioReactiveState = false;
        }

        EVERY_N_MILLISECONDS(5000) {
            FL_WARN("Audio Reactive is DISABLED - enable in UI to use downbeat detection");
        }
    }

    // Update animation with audio-reactive speed
    float effectiveSpeed = timeSpeed.value() * audioSpeedFactor;
    fxEngine.setSpeed(effectiveSpeed);

    // Draw the effect
    fxEngine.draw(millis(), leds);

    // Apply downbeat darkness effect if enabled
    if (enableDownbeatDarkness.value() && enableAudioReactive.value()) {
        static bool firstRun = true;
        if (firstRun) {
            FL_WARN("Downbeat darkness ACTIVE - waiting for downbeats...");
            firstRun = false;
        }

        unsigned long currentTime = millis();

        // Trigger darkness on downbeat
        if (darknessTrigger) {
            darknessAmount = 1.0f - downbeatDarknessAmount.value(); // Convert darkness to brightness multiplier
            lastDarknessTime = currentTime;
            darknessTrigger = false;
            FL_WARN("Applying darkness: " << darknessAmount << " (configured darkness: " << downbeatDarknessAmount.value() << ")");
        }

        // Fade back to normal brightness using exponential curve
        if (darknessAmount < 1.0f) {
            unsigned long elapsed = currentTime - lastDarknessTime;
            float fadeDuration = darknessFadeDuration.value();

            if (elapsed < fadeDuration) {
                // Exponential fade: smoother and more natural
                float progress = elapsed / fadeDuration; // 0 to 1
                float targetDarkness = 1.0f - downbeatDarknessAmount.value();
                darknessAmount = targetDarkness + (1.0f - targetDarkness) * (1.0f - fl::exp(-5.0f * progress));
            } else {
                darknessAmount = 1.0f; // Fully recovered
            }
        }

        // Apply darkness to LEDs (scale brightness)
        if (darknessAmount < 1.0f) {
            uint8_t scaleFactor = static_cast<uint8_t>(darknessAmount * 255.0f);
            FL_WARN("Scaling LEDs by " << scaleFactor << "/255 (darknessAmount: " << darknessAmount << ")");
            for (int i = 0; i < NUM_LEDS; i++) {
                leds[i].nscale8(scaleFactor);
            }
        }
    } else {
        EVERY_N_MILLISECONDS(5000) {
            FL_WARN("Downbeat darkness NOT active - DownbeatDarkness: "
                    << enableDownbeatDarkness.value() << ", AudioReactive: "
                    << enableAudioReactive.value());
        }
    }

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

    // Apply audio brightness multiplier if enabled
    if (enableAudioReactive.value() && audioAffectsBrightness.value()) {
        finalBrightness =
            static_cast<uint8_t>(finalBrightness * audioBrightnessFactor);
    }

    FastLED.setBrightness(finalBrightness);

    FastLED.show();
}

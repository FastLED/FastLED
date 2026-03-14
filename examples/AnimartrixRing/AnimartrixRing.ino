// @filter: (memory is high)

// AnimartrixRing: Sample a circle from an Animartrix rectangular grid
// Uses AudioProcessor's VibeDetector for self-normalizing bass/mid/treb
// levels. Bass level warps animation speed via FxEngine's TimeWarp.

// Use SPI-based WS2812 driver instead of RMT on ESP32
#define FASTLED_ESP32_USE_CLOCKLESS_SPI

// FastLED.h must be included first to trigger precompiled headers for FastLED's
// build system
#include "FastLED.h"

#include "fl/math_macros.h"
#include "fl/gfx/screenmap.h"
#include "fl/ui.h"
#include "fl/stl/math.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/audio/audio_processor.h"
#include "fl/audio/detectors/vibe.h"
#include "fl/fx/fx2d_to_1d.h"
#include "fl/fx/fx_engine.h"
#include <FastLED.h>

FASTLED_TITLE("AnimartrixRing");

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
        float radius = fl::min(GRID_WIDTH, GRID_HEIGHT) / 2.0f - 1;
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
fl::UICheckbox enableVibeReactive("Enable Vibe Reactive", false);
fl::UISlider vibeSpeedMultiplier("Vibe Speed Multiplier", 3.0, 0.0, 10.0, 0.1);
fl::UISlider vibeBaseSpeed("Vibe Base Speed", 1.0, 0.0, 5.0, 0.1);

// AudioProcessor with VibeDetector (initialized in setup via FastLED.add or fallback)
fl::shared_ptr<fl::AudioProcessor> gAudioProcessor;
bool gAutoPump = false;

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
        return 255;
    } else if (inputBrightnessPercent < highThreshold) {
        float range = highThreshold - lowThreshold;
        float progress =
            (inputBrightnessPercent - lowThreshold) / range; // 0 to 1
        float targetPercent =
            100.0f - (progress * (100.0f - maxBrightnessPercent));
        return static_cast<uint8_t>((targetPercent / 100.0f) * 255.0f);
    } else {
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
    });

    // Route audio through FastLED.add() for auto-pump when available
    auto input = audio.audioInput();
    if (input) {
        gAudioProcessor = FastLED.add(input);
        gAutoPump = true;
        printf("AnimartrixRing: Audio routed via FastLED.add() (auto-pump)\n");
    }
    if (!gAudioProcessor) {
        gAudioProcessor = fl::make_shared<fl::AudioProcessor>();
        printf("AnimartrixRing: Audio using manual pump (fallback)\n");
    }

    // Hook VibeDetector bass level to FxEngine timewarp.
    // onVibeLevels fires every frame with self-normalizing levels:
    //   bass ~1.0 = average, >1.0 = louder than normal, <1.0 = quieter
    // We map bass level directly to animation speed so beats accelerate
    // the animation.
    gAudioProcessor->onVibeLevels([](const fl::VibeLevels &vibe) {
        if (!enableVibeReactive.value()) {
            return;
        }
        // Print beat/mid/treble levels and spike flags each frame
        printf("Vibe: bass=%.2f mid=%.2f treb=%.2f | spikes: bass=%d mid=%d treb=%d\n",
               vibe.bass, vibe.mid, vibe.treb,
               vibe.bassSpike, vibe.midSpike, vibe.trebSpike);

        // bass hovers around 1.0; scale it into a speed multiplier
        float bassBoost = (vibe.bass - 1.0f) * vibeSpeedMultiplier.value();
        float speed = vibeBaseSpeed.value() + bassBoost;
        // Combine with the user's manual time speed slider
        speed *= timeSpeed.value();
        fxEngine.setSpeed(speed);
    });

    // Log spike events
    gAudioProcessor->onVibeBassSpike([]() {
        printf(">>> BASS SPIKE!\n");
    });
    gAudioProcessor->onVibeMidSpike([]() {
        printf(">>> MID SPIKE!\n");
    });
    gAudioProcessor->onVibeTrebSpike([]() {
        printf(">>> TREB SPIKE!\n");
    });

    Serial.println("AnimartrixRing setup complete");
}

void loop() {
    // When auto-pump is not available, manually drain audio and feed processor
    if (!gAutoPump) {
        fl::AudioSample sample = audio.next();
        if (sample.isValid()) {
            static uint32_t sAudioSamples = 0;
            sAudioSamples++;
            if (sAudioSamples == 1) {
                printf("AnimartrixRing: First audio sample received! "
                       "enableVibeReactive=%d\n",
                       (int)enableVibeReactive.value());
            } else if (sAudioSamples % 172 == 0) {
                printf("AnimartrixRing: %u audio samples processed, "
                       "enableVibeReactive=%d\n",
                       (unsigned)sAudioSamples,
                       (int)enableVibeReactive.value());
            }
            if (enableVibeReactive.value()) {
                gAudioProcessor->update(sample);
            }
        }
    }
    if (!enableVibeReactive.value()) {
        fxEngine.setSpeed(timeSpeed.value());
    }

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

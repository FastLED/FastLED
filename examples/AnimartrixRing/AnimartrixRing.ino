// @filter: (memory is large)

// AnimartrixRing: Sample a circle from an Animartrix rectangular grid.
//
// Audio model (issue #2713):
//
//   audio -> SoundOrchestrator { Silence | Disorganized | BpmLocked }
//         -> per-state Animartrix bank
//         -> per-state audio->visual mapping (energy, bass, kick, downbeat, ...)
//
// Time warp is preserved as a *secondary* effect inside the Disorganized state
// (vibe.bass nudges engine speed within a bounded span). It is no longer the
// primary audio interaction.

// Use SPI-based WS2812 driver instead of RMT on ESP32
#define FASTLED_ESP32_USE_CLOCKLESS_SPI

// FastLED.h must be included first to trigger precompiled headers for FastLED's
// build system
#include "FastLED.h"

#if defined(FL_IS_TEENSY)
// Keep fbuild's library scanner aware of PJRC Audio sources for Teensy.
#include <Audio.h>
#endif

#include "fl/math/math.h"
#include "fl/math/screenmap.h"
#include "fl/ui/ui.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/audio/audio_processor.h"
#include "fl/audio/detector/vibe.h"
#include "fl/fx/fx2d_to_1d.h"
#include "fl/fx/fx_engine.h"
#include <FastLED.h>

#include "auto_brightness.h"
#include "sound_orchestrator.h"

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
auto animartrix = fl::make_shared<fl::Animartrix>(xymap, fl::AnimartrixAnim::SLOW_FADE);

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

// UI controls
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
fl::UICheckbox enableOrchestrator("Enable Sound Orchestrator", false);
fl::UISlider orchestratorDwellMs("Orchestrator Min Dwell (ms)", 1500, 200, 5000, 100);
fl::UISlider orchestratorHysteresisMs("Orchestrator Hysteresis (ms)", 400, 0, 2000, 50);

// Processor + orchestrator (initialized in setup)
fl::shared_ptr<fl::audio::Processor> gAudioProcessor;
fl::shared_ptr<animartrix_ring::SoundOrchestrator> gOrchestrator;
bool gAutoPump = false;

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

    // Route audio through FastLED.add() for auto-pump when available.
    // gAutoPump may only be set true when FastLED.add() actually returned a
    // live processor -- otherwise loop() would skip the manual pump path and
    // the orchestrator would never see any samples.
    auto input = audio.audioInput();
    if (input) {
        gAudioProcessor = FastLED.add(input);
        if (gAudioProcessor) {
            gAutoPump = true;
            printf("AnimartrixRing: Audio routed via FastLED.add() (auto-pump)\n");
        }
    }
    if (!gAudioProcessor) {
        gAudioProcessor = fl::make_shared<fl::audio::Processor>();
        gAutoPump = false;
        printf("AnimartrixRing: Audio using manual pump (fallback)\n");
    }

    // Build the 3-state orchestrator. It owns nothing: it just polls the
    // Processor, asks the Animartrix to switch banks, and pokes the FxEngine
    // speed on every frame.
    gOrchestrator = fl::make_shared<animartrix_ring::SoundOrchestrator>(
        gAudioProcessor, animartrix, &fxEngine);
    gOrchestrator->begin();

    // Log state transitions so a developer can see the classifier in action.
    static animartrix_ring::SoundState sLastState =
        animartrix_ring::SoundState::Silence;
    (void)sLastState;

    Serial.println("AnimartrixRing setup complete (3-state orchestrator)");
}

void loop() {
    // Manual audio pump fallback (e.g. WASM / when FastLED.add() didn't take).
    if (!gAutoPump) {
        fl::audio::Sample sample = audio.next();
        if (sample.isValid() && enableOrchestrator.value()) {
            gAudioProcessor->update(sample);
        }
    }

    // Per-frame orchestrator tick. When disabled, fall back to plain manual
    // speed so the sketch still behaves like a non-audio Animartrix demo.
    if (enableOrchestrator.value()) {
        // Apply UI overrides cheaply on every tick.
        animartrix_ring::OrchestratorConfig cfg = gOrchestrator->config();
        cfg.minDwellMs = static_cast<fl::u32>(orchestratorDwellMs.value());
        cfg.classifierHysteresisMs =
            static_cast<fl::u32>(orchestratorHysteresisMs.value());
        gOrchestrator->setConfig(cfg);

        const fl::u32 now = millis();
        gOrchestrator->tick(now, timeSpeed.value());

        // Log state transitions.
        static animartrix_ring::SoundState sLast =
            animartrix_ring::SoundState::Silence;
        if (gOrchestrator->state() != sLast) {
            sLast = gOrchestrator->state();
            printf("AnimartrixRing: state -> %s (engine speed=%.2f)\n",
                   animartrix_ring::toString(sLast),
                   gOrchestrator->lastEngineSpeed());
        }
    } else {
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

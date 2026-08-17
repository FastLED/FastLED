// FL_AGENT_ALLOW_NEW_EXAMPLE
// @filter: (memory is large)

/// @file    MoodRing.ino
/// @brief   A lighting toy that listens to the room and lights the room back.
/// @example MoodRing.ino
///
/// This is the *product* sketch. Where `examples/AnimartrixRing/` demonstrates
/// one technique (sample a ring out of a 2D Animartrix grid) with nothing else
/// attached, MoodRing is the full stack that sits on top of that technique:
///
///   Mic -> fl::audio::Processor      (silence / energy / eq / tempo / beat)
///       -> SoundOrchestrator         (Silence | Disorganized | BpmLocked)
///       -> per-state Animartrix bank (calm / spectrum / beat-geometry)
///       -> ring
///
/// The input is not "music" -- it is the room, whatever it sounds like.
/// Conversation, silence, clatter, a song coming on. The orchestrator
/// classifies the sound environment and each class gets its own visual
/// strategy, with hysteresis and minimum dwell times so the state machine does
/// not chatter on borderline audio.
///
/// Design doc and roadmap: https://github.com/FastLED/FastLED/issues/2256
///
/// Landed so far:
///   * 3-state classifier + per-state visual banks + per-state audio mapping
///
/// Still to land (see #2256):
///   * VisualControlBus -- one struct of derived signals both engines consume
///   * engine-agnostic overlay compositor (trails / pulse / sector / sparkle)
///   * mood-quadrant bias from MoodAnalyzer (valence x arousal)
///   * a second engine (NoiseRing) behind the same bus, with cross-fade
///   * patch schema + URL serialization, curated presets
///
/// This sketch is fully compatible with the FastLED web compiler:
/// 1. `pip install fastled`
/// 2. cd into this directory
/// 3. run `fastled`, then grant microphone access (or drag in a .wav)

// Use SPI-based WS2812 driver instead of RMT on ESP32
#define FASTLED_ESP32_USE_CLOCKLESS_SPI

// FastLED.h must be included first to trigger precompiled headers for FastLED's
// build system
#include "FastLED.h"

#if defined(FL_IS_TEENSY)
// Keep fbuild's library scanner aware of PJRC Audio sources for Teensy.
#include <Audio.h>
#endif

#include "fl/audio/audio_processor.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/fx2d_to_1d.h"
#include "fl/fx/fx_engine.h"
#include "fl/ui/ui.h"

#include "auto_brightness.h"
#include "ring_screenmap.h"
#include "sound_orchestrator.h"

FASTLED_TITLE("MoodRing");

#define NUM_LEDS 244

#ifndef PIN_DATA
#define PIN_DATA 3 // ESP32C6 has this random pin available on the break out.
#endif             // PIN_DATA

#define BRIGHTNESS 8

// Grid dimensions the Animartrix engine renders into before the ring samples it.
#define GRID_WIDTH 16
#define GRID_HEIGHT 16

// 0.15 cm or 1.5mm -- appropriate for a dense LED rope.
#define LED_DIAMETER 0.15f

CRGB leds[NUM_LEDS];

// Animartrix 2D effect. The orchestrator switches which animation is active.
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

// Visual UI controls
fl::UITitle title("MoodRing");
fl::UIDescription description(
    "A ring that listens to the room and lights the room back. Grant mic "
    "access, or drag in a .wav.");

fl::UISlider timeSpeed("Time Speed", 1, -10, 10, .1);
fl::UISlider brightness("Brightness", BRIGHTNESS, 0, 255, 1);
fl::UICheckbox autoBrightness("Auto Brightness", true);
fl::UISlider autoBrightnessMax("Auto Brightness Max", 84, 0, 255, 1);
fl::UISlider autoBrightnessLowThreshold("Auto Brightness Low Threshold", 8, 0,
                                        100, 1);
fl::UISlider autoBrightnessHighThreshold("Auto Brightness High Threshold", 22,
                                         0, 100, 1);

// Audio UI controls. The orchestrator is the point of this sketch, so it is on
// by default -- a room-sensing toy that boots deaf is not the product.
fl::UIAudio audio("Audio Input");
fl::UICheckbox enableOrchestrator("Enable Sound Orchestrator", true);
fl::UISlider orchestratorDwellMs("Orchestrator Min Dwell (ms)", 1500, 200, 5000,
                                 100);
fl::UISlider orchestratorHysteresisMs("Orchestrator Hysteresis (ms)", 400, 0,
                                      2000, 50);

// Processor + orchestrator (initialized in setup)
fl::shared_ptr<fl::audio::Processor> gAudioProcessor;
fl::shared_ptr<mood_ring::SoundOrchestrator> gOrchestrator;
bool gAutoPump = false;

void setup() {
    Serial.begin(115200);

    // Setup LED strip
    FastLED.addLeds<WS2812, PIN_DATA>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenmap);
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
            printf("MoodRing: Audio routed via FastLED.add() (auto-pump)\n");
        }
    }
    if (!gAudioProcessor) {
        gAudioProcessor = fl::make_shared<fl::audio::Processor>();
        gAutoPump = false;
        printf("MoodRing: Audio using manual pump (fallback)\n");
    }

    // Build the 3-state orchestrator. It owns nothing: it just polls the
    // Processor, asks the Animartrix to switch banks, and pokes the FxEngine
    // speed on every frame.
    gOrchestrator = fl::make_shared<mood_ring::SoundOrchestrator>(
        gAudioProcessor, animartrix, &fxEngine);
    gOrchestrator->begin();

    Serial.println("MoodRing setup complete (3-state orchestrator)");
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
        mood_ring::OrchestratorConfig cfg = gOrchestrator->config();
        cfg.minDwellMs = static_cast<fl::u32>(orchestratorDwellMs.value());
        cfg.classifierHysteresisMs =
            static_cast<fl::u32>(orchestratorHysteresisMs.value());
        gOrchestrator->setConfig(cfg);

        const fl::u32 now = millis();
        gOrchestrator->tick(now, timeSpeed.value());

        // Log state transitions so the classifier is observable in the field.
        static mood_ring::SoundState sLast = mood_ring::SoundState::Silence;
        if (gOrchestrator->state() != sLast) {
            sLast = gOrchestrator->state();
            printf("MoodRing: state -> %s (engine speed=%.2f)\n",
                   mood_ring::toString(sLast),
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

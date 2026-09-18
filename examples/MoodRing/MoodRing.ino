// @filter: (memory is large)

/// @file    MoodRing.ino
/// @brief   A ring that listens to the room and lights the room back.
/// @example MoodRing.ino
///
/// The input is not "music". It is the room: conversation, silence,
/// laughter, clatter, a song coming on. The sketch is two halves with one
/// struct between them:
///
///   Mic -> fl::audio::Processor
///       -> RoomListener  : fills RoomSense (presence, energy, punch, bands,
///                          groove confidence, beat phase, mood) and three
///                          regime weights that always sum to one:
///                          calm / flow / groove
///       -> MoodPainter   : renders Breath, Flow and Groove layers straight
///                          onto the ring, mixes them by weight, then trails
///                          and beat pulses
///       -> ring
///
/// There is no state machine. The weights move with asymmetric attack and
/// release, so the ring wakes on the first sound, settles slowly into calm,
/// and cross-fades between looks because the mix is continuous.
///
/// Lineage and the round-by-round design notes: DESIGN.md next to this file.
/// Design thread: https://github.com/FastLED/FastLED/issues/2256
///
/// Web compiler:
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
#include "fl/stl/stdio.h"
#include "fl/ui/ui.h"

#include "auto_brightness.h"
#include "mood_painter.h"
#include "ring_screenmap.h"
#include "room_sense.h"

FASTLED_TITLE("MoodRing");

#define NUM_LEDS 244

#ifndef PIN_DATA
#define PIN_DATA 3 // ESP32C6 has this random pin available on the break out.
#endif             // PIN_DATA

#define BRIGHTNESS 8

// The ScreenMap only positions LEDs for the web preview; the painter draws
// directly on the 1D ring. 16x16 is the preview grid, not a render target.
#define GRID_WIDTH 16
#define GRID_HEIGHT 16

// 0.15 cm or 1.5mm -- appropriate for a dense LED rope.
#define LED_DIAMETER 0.15f

CRGB leds[NUM_LEDS];

fl::ScreenMap screenmap =
    makeRingScreenMap(NUM_LEDS, GRID_WIDTH, GRID_HEIGHT, LED_DIAMETER);

// Title comes from FASTLED_TITLE above -- do not declare a second fl::UITitle.
fl::UIDescription description(
    "A ring that listens to the room and lights the room back. Grant mic "
    "access, or drag in a .wav.");

// Brightness.
fl::UISlider brightness("Brightness", BRIGHTNESS, 0, 255, 1);
fl::UICheckbox autoBrightness("Auto Brightness", true);
fl::UISlider autoBrightnessMax("Auto Brightness Max", 84, 0, 255, 1);
fl::UISlider autoBrightnessLowThreshold("Auto Brightness Low Threshold", 8, 0,
                                        100, 1);
fl::UISlider autoBrightnessHighThreshold("Auto Brightness High Threshold", 22,
                                         0, 100, 1);

// Listening. On by default: a room-sensing toy that boots deaf is not the
// product.
fl::UIAudio audio("Audio Input");
fl::UICheckbox listen("Listen", true);
fl::UISlider presenceReleaseMs("Settle Into Calm (ms)", 900, 200, 4000, 50);
fl::UISlider grooveAttackMs("Trust A Beat After (ms)", 800, 100, 4000, 50);
fl::UISlider grooveEnter("Groove Enter Confidence", 0.60f, 0.1f, 1.0f, 0.01f);
fl::UISlider grooveExit("Groove Exit Confidence", 0.35f, 0.0f, 0.9f, 0.01f);
fl::UISlider punchGain("Punch Gain", 1.5f, 0.0f, 3.0f, 0.05f);

// Painting. Zeroing a layer gain is how you check the other two in
// isolation; turning trails and pulses off leaves the raw mix.
fl::UISlider breathGain("Layer: Breath", 1.0f, 0.0f, 1.0f, 0.05f);
fl::UISlider flowGain("Layer: Flow", 1.0f, 0.0f, 1.0f, 0.05f);
fl::UISlider grooveGain("Layer: Groove", 1.0f, 0.0f, 1.0f, 0.05f);
fl::UICheckbox trails("Trails", true);
fl::UISlider trailOverride("Trail Keep Override", -1.0f, -1.0f, 0.98f, 0.02f);
fl::UICheckbox pulses("Beat Pulses", true);
fl::UISlider pulseOrigin("Pulse Origin (turns)", 0.0f, 0.0f, 1.0f, 0.01f);
fl::UICheckbox debugPrint("Debug: Print Sense", false);

// Wiring (initialized in setup).
fl::shared_ptr<fl::audio::Processor> gProcessor;
fl::shared_ptr<mood_ring::RoomListener> gListener;
mood_ring::MoodPainter gPainter;
bool gAutoPump = false;

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812, PIN_DATA>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenmap);
    FastLED.setBrightness(brightness.value());

    // Route audio through FastLED.add() for auto-pump when available.
    // gAutoPump may only be true when FastLED.add() returned a live
    // processor; otherwise loop() would skip the manual pump and the
    // listener would never see a sample.
    auto input = audio.audioInput();
    if (input) {
        gProcessor = FastLED.add(input);
        if (gProcessor) {
            gAutoPump = true;
            fl::printf("MoodRing: audio routed via FastLED.add() (auto-pump)\n");
        }
    }
    if (!gProcessor) {
        gProcessor = fl::make_shared<fl::audio::Processor>();
        gAutoPump = false;
        fl::printf("MoodRing: audio using manual pump (fallback)\n");
    }

    gListener = fl::make_shared<mood_ring::RoomListener>(gProcessor);
    gListener->begin();

    Serial.println("MoodRing setup complete");
}

static void applyUiTuning() {
    mood_ring::ListenerTuning &lt = gListener->tuning;
    lt.presenceReleaseMs = presenceReleaseMs.value();
    lt.grooveAttackMs = grooveAttackMs.value();
    lt.grooveEnter = grooveEnter.value();
    lt.grooveExit = grooveExit.value();
    lt.punchGain = punchGain.value();

    mood_ring::PainterTuning &pt = gPainter.tuning;
    pt.breathGain = breathGain.value();
    pt.flowGain = flowGain.value();
    pt.grooveGain = grooveGain.value();
    pt.trails = trails.value();
    pt.trailOverride = trailOverride.value();
    pt.pulses = pulses.value();
    pt.pulseOrigin = pulseOrigin.value();
}

static void printSense(const mood_ring::RoomSense &s) {
    fl::printf("MoodRing %s c/f/g=%.2f/%.2f/%.2f pres=%.2f en=%.2f tr=%+.2f "
               "punch=%.2f shim=%.2f bands=%.2f/%.2f/%.2f bpm=%.0f conf=%.2f "
               "warm=%.2f ar=%.2f pulses=%d\n",
               gListener->regimeName(), s.calm, s.flow, s.groove, s.presence,
               s.energy, s.trend, s.punch, s.shimmer, s.low, s.mid, s.high, s.bpm,
               s.grooveConfidence, s.warmth, s.arousal,
               gPainter.activePulseCount());
}

void loop() {
    const fl::u32 now = millis();

    // Manual audio pump fallback (e.g. WASM / when FastLED.add() didn't take).
    if (!gAutoPump) {
        fl::audio::Sample sample = audio.next();
        if (sample.isValid() && listen.value()) {
            gProcessor->update(sample);
        }
    }

    applyUiTuning();

    // Listen, then paint. With listening off every signal eases to calm at
    // its normal release rate, so the sketch fades into a slow ambient lamp
    // rather than cutting or going dark.
    if (listen.value()) {
        gListener->update(now);
    } else {
        gListener->rest(now);
    }
    const mood_ring::RoomSense &sense = gListener->sense();
    gPainter.draw(sense, now, leds);

    // Log regime changes so the listener is observable in the field.
    static const char *sLastRegime = "";
    const char *regime = gListener->regimeName();
    if (regime != sLastRegime) {
        sLastRegime = regime;
        fl::printf("MoodRing: regime -> %s\n", regime);
    }
    if (debugPrint.value()) {
        static fl::u32 sFrame = 0;
        if ((sFrame++ % 30) == 0) printSense(sense);
    }

    // Brightness.
    uint8_t finalBrightness;
    if (autoBrightness.value()) {
        const float avg = getAverageBrightness(leds);
        finalBrightness = applyBrightnessCompression(
            avg, static_cast<uint8_t>(autoBrightnessMax.value()),
            autoBrightnessLowThreshold.value(),
            autoBrightnessHighThreshold.value());
    } else {
        finalBrightness = static_cast<uint8_t>(brightness.value());
    }
    FastLED.setBrightness(finalBrightness);
    FastLED.show();
}

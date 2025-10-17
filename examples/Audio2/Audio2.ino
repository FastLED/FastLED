/// @file    Audio2.ino
/// @brief   Audio System v2.0 demonstration with multiple detectors
/// @example Audio2.ino

#include <FastLED.h>
#include "fl/ui.h"
#include "fl/audio/audio_processor.h"

using namespace fl;

#define LED_PIN 3
#define NUM_LEDS 60
#define BRIGHTNESS 128

CRGB leds[NUM_LEDS];
AudioProcessor audioProcessor;

// UI Elements
UIAudio audio("Audio Input");
UITitle title("Audio System v2.0 Demo");
UIDescription description("Real-time audio analysis with beat, frequency, buildup, and drop detection");
UICheckbox enableBeat("Enable Beat Detection", true);
UICheckbox enableFreq("Enable Frequency Bands", true);
UICheckbox enableBuildup("Enable Buildup Detection", true);
UICheckbox enableDrop("Enable Drop Detection", true);

// State variables
float currentBeat = 0.0f;
float currentBass = 0.0f;
float currentMid = 0.0f;
float currentTreble = 0.0f;
float currentEnergy = 0.0f;
uint16_t currentBPM = 0;
bool buildupActive = false;
bool dropActive = false;

void setup() {
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);

    // Enable beat detection
    audioProcessor.enableBeatDetection([](float confidence) {
        currentBeat = confidence;
    });

    // Enable frequency bands
    audioProcessor.enableFrequencyBands([](float bass, float mid, float treble) {
        currentBass = bass;
        currentMid = mid;
        currentTreble = treble;
    });

    // Enable energy analyzer
    audioProcessor.enableEnergyAnalyzer([](float energy, float rms) {
        currentEnergy = energy;
    });

    // Enable tempo analyzer
    audioProcessor.enableTempoAnalyzer([](uint16_t bpm, float confidence) {
        currentBPM = bpm;
    });

    // Enable buildup detector
    audioProcessor.enableBuildupDetector([](float tension, bool active) {
        buildupActive = active;
    });

    // Enable drop detector
    audioProcessor.enableDropDetector([](float impact, bool active) {
        dropActive = active;
    });
}

void loop() {
    // Process audio samples from UIAudio
    while (AudioSample sample = audio.next()) {
        // Process each PCM sample through the audio processor
        for (size_t i = 0; i < sample.pcm().size(); i++) {
            audioProcessor.update(sample.pcm()[i]);
        }
    }

    // Render visualization based on audio features
    renderVisualization();

    FastLED.show();
}

void renderVisualization() {
    // Clear LEDs with fade
    fadeToBlackBy(leds, NUM_LEDS, 20);

    // Special effect for drops (if enabled)
    if (enableDrop && dropActive) {
        fill_solid(leds, NUM_LEDS, CRGB::White);
        return;
    }

    // Special effect for buildups (if enabled)
    if (enableBuildup && buildupActive) {
        uint8_t hue = beatsin8(60, 0, 255);
        fill_rainbow(leds, NUM_LEDS, hue, 5);
        return;
    }

    // Normal mode: frequency-reactive visualization (if enabled)
    if (enableFreq) {
        // Bass (red) - bottom third
        uint8_t bassLeds = NUM_LEDS / 3;
        for (uint8_t i = 0; i < bassLeds; i++) {
            uint8_t brightness = currentBass * 255 * (bassLeds - i) / bassLeds;
            leds[i] = CRGB(brightness, 0, 0);
        }

        // Mids (green) - middle third
        uint8_t midStart = bassLeds;
        uint8_t midLeds = NUM_LEDS / 3;
        for (uint8_t i = 0; i < midLeds; i++) {
            uint8_t brightness = currentMid * 255 * (midLeds - i) / midLeds;
            leds[midStart + i] = CRGB(0, brightness, 0);
        }

        // Treble (blue) - top third
        uint8_t trebleStart = midStart + midLeds;
        uint8_t trebleLeds = NUM_LEDS - trebleStart;
        for (uint8_t i = 0; i < trebleLeds; i++) {
            uint8_t brightness = currentTreble * 255 * (trebleLeds - i) / trebleLeds;
            leds[trebleStart + i] = CRGB(0, 0, brightness);
        }
    }

    // Beat flash overlay (if enabled)
    if (enableBeat && currentBeat > 0.5f) {
        uint8_t beatBrightness = currentBeat * 255;
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            leds[i] += CRGB(beatBrightness / 3, beatBrightness / 3, beatBrightness / 3);
        }
    }
}

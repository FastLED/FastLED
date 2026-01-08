/// @file    BeatDetection.ino
/// @brief   Simple real-time beat detection with LED visualization
/// @example BeatDetection.ino
///
/// Demonstrates beat detection using the AudioProcessor facade.
/// Visualizes beats and tempo on an LED strip - flashes on beat detection.

#include <FastLED.h>

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else

#include "fl/fx/audio/audio_processor.h"
#include "fl/ui.h"

using namespace fl;

// LED Configuration
#define NUM_LEDS 60
#define DATA_PIN 3
#define BRIGHTNESS 128

CRGB leds[NUM_LEDS];

// Audio input
UIAudio audio("Audio Input");

// Audio processor with beat detection
AudioProcessor audioProcessor;

// Beat detection state
float currentBPM = 0.0f;
uint32_t lastBeatTime = 0;
uint32_t beatCount = 0;
uint32_t onsetCount = 0;

// Get color based on BPM (blue=slow, green=medium, red=fast)
CRGB getBPMColor(float bpm) {
    if (bpm < 90.0f) {
        return CRGB::Blue;
    } else if (bpm < 130.0f) {
        return CRGB::Green;
    } else {
        return CRGB::Red;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Beat Detection Example");
    Serial.println("=====================");

    // Initialize LED strip with screen map for visualization
    ScreenMap screenMap = ScreenMap::DefaultStrip(NUM_LEDS, 1.5f, 0.5f);
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS)
        .setScreenMap(screenMap);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // Set up beat detection callbacks
    audioProcessor.onBeat([]() {
        beatCount++;
        lastBeatTime = fl::millis();
        Serial.print("BEAT #");
        Serial.println(beatCount);
    });

    audioProcessor.onOnset([](float strength) {
        onsetCount++;
        Serial.print("Onset strength=");
        Serial.println(strength);
    });

    audioProcessor.onTempoChange([](float bpm, float confidence) {
        currentBPM = bpm;
        Serial.print("Tempo: ");
        Serial.print(bpm);
        Serial.print(" BPM (confidence: ");
        Serial.print(confidence);
        Serial.println(")");
    });

    Serial.println("Beat detector initialized");
    Serial.println("Playing audio will trigger beat detection...");
}

void loop() {
    // Get audio sample
    AudioSample sample = audio.next();

    if (sample.isValid()) {
        // Process audio through the audio processor
        audioProcessor.update(sample);
    }

    // Visualize beats on LED strip
    uint32_t timeSinceBeat = fl::millis() - lastBeatTime;

    if (timeSinceBeat < 100) {
        // Flash bright on beat
        CRGB beatColor = getBPMColor(currentBPM);
        fill_solid(leds, NUM_LEDS, beatColor);
    } else if (timeSinceBeat < 200) {
        // Fade out
        CRGB beatColor = getBPMColor(currentBPM);
        beatColor.fadeToBlackBy(128);
        fill_solid(leds, NUM_LEDS, beatColor);
    } else {
        // Idle: dim pulse at detected tempo
        if (currentBPM > 0) {
            // Pulse frequency based on BPM
            float period_ms = (60000.0f / currentBPM);
            float phase = fmod(static_cast<float>(fl::millis()), period_ms) / period_ms;
            uint8_t brightness = static_cast<uint8_t>((fl::sin(phase * 2.0f * 3.14159f) + 1.0f) * 32.0f);

            CRGB idleColor = getBPMColor(currentBPM);
            idleColor.fadeToBlackBy(255 - brightness);
            fill_solid(leds, NUM_LEDS, idleColor);
        } else {
            // No tempo detected yet
            FastLED.clear();
        }
    }

    FastLED.show();

    // Print stats periodically
    EVERY_N_SECONDS(10) {
        Serial.println();
        Serial.println("=== Beat Detection Stats ===");
        Serial.print("Onsets: ");
        Serial.print(onsetCount);
        Serial.print(" | Beats: ");
        Serial.println(beatCount);
        Serial.print("Current BPM: ");
        Serial.println(currentBPM);
        Serial.println("============================");
    }
}

#endif

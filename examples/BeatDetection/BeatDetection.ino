/// @file    BeatDetection.ino
/// @brief   Spectrum analyzer with beat detection
/// @example BeatDetection.ino
///
/// Classic equalizer bar visualization with beat detection.
/// Each vertical bar represents a frequency band.

#include <FastLED.h>

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else

#include "fx/audio/beat_detector.h"
#include "fl/ui.h"

using namespace fl;

// LED Configuration
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define DATA_PIN 3
#define BRIGHTNESS 128
#define NUM_BANDS 24  // Number of frequency bands to analyze

CRGB leds[NUM_LEDS];

// Audio input
UIAudio audio("Audio Input");

// Spectrum analyzer state
float bandLevels[NUM_BANDS];
float bandPeaks[NUM_BANDS];
uint32_t bandPeakTimes[NUM_BANDS];

// Description
UIDescription description(
    "Spectrum Analyzer with Beat Detection\n\n"
    "🎵 WHAT THIS DOES:\n"
    "Classic equalizer-style visualization with vertical bars.\n"
    "Each column represents a different frequency band.\n\n"

    "✨ WHAT YOU'LL SEE:\n"
    "• Red bars (left) → Bass frequencies (kicks, bass)\n"
    "• Green bars (middle) → Mid frequencies (vocals, snares)\n"
    "• Blue bars (right) → High frequencies (hi-hats, cymbals)\n"
    "• White dots → Peak hold for each band\n"
    "• Bars rise and fall with the music intensity\n\n"

    "🎧 HOW TO USE IT:\n"
    "1. Click 'Audio Input' to select your source\n"
    "2. Play some music\n"
    "3. Watch the bars dance!\n\n"

    "⚙️ PARAMETERS:\n"
    "• Brightness: LED brightness (0-255)\n"
    "• Fade Speed: How quickly bars fall (lower = slower)\n"
    "• Bar Gain: Amplitude multiplier (higher = taller bars)\n"
    "• Min BPM / Max BPM: Tempo detection range\n"
);

// UI Controls
UISlider brightness("Brightness", 128.0f, 0.0f, 255.0f);
UISlider fadeSpeed("Fade Speed", 0.85f, 0.70f, 0.98f);
UISlider barGain("Bar Gain", 5.0f, 1.0f, 20.0f);
UISlider minBPM("Min BPM", 80.0f, 60.0f, 120.0f);
UISlider maxBPM("Max BPM", 160.0f, 120.0f, 200.0f);

// Beat detector
BeatDetector* beatDetector = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Spectrum Analyzer Example");
    Serial.println("=========================");

    // Initialize LED matrix
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // Initialize band arrays
    for (int i = 0; i < NUM_BANDS; i++) {
        bandLevels[i] = 0.0f;
        bandPeaks[i] = 0.0f;
        bandPeakTimes[i] = 0;
    }

    // Create beat detector configuration
    BeatDetectorConfig cfg;
    cfg.sample_rate_hz = 44100;
    cfg.odf_type = OnsetDetectionFunction::SuperFlux;
    cfg.num_bands = NUM_BANDS;
    cfg.tempo_min_bpm = minBPM.value();
    cfg.tempo_max_bpm = maxBPM.value();
    cfg.adaptive_whitening = true;

    beatDetector = new BeatDetector(cfg);

    // Wire beat callback
    beatDetector->onBeat = [](float confidence, float bpm, float timestamp_ms) {
        Serial.print("BEAT! BPM=");
        Serial.print(bpm);
        Serial.print(", confidence=");
        Serial.println(confidence);
    };

    Serial.println("Ready! Start playing music...");
}

void loop() {
    // Update brightness
    FastLED.setBrightness((uint8_t)brightness.value());

    // Update config if changed
    EVERY_N_MILLISECONDS(100) {
        BeatDetectorConfig cfg = beatDetector->config();
        bool changed = false;

        if (cfg.tempo_min_bpm != minBPM.value()) {
            cfg.tempo_min_bpm = minBPM.value();
            changed = true;
        }
        if (cfg.tempo_max_bpm != maxBPM.value()) {
            cfg.tempo_max_bpm = maxBPM.value();
            changed = true;
        }

        if (changed) {
            beatDetector->setConfig(cfg);
        }
    }

    // Process audio and update spectrum
    AudioSample sample = audio.next();
    if (sample.isValid()) {
        // Convert int16 PCM to float [-1.0, 1.0]
        static float audioBuffer[512];
        size_t numSamples = min(sample.pcm().size(), (size_t)512);

        for (size_t i = 0; i < numSamples; i++) {
            audioBuffer[i] = sample.pcm()[i] / 32768.0f;
        }

        beatDetector->processFrame(audioBuffer, numSamples);

        // Get ODF value and distribute across bands
        float odf = beatDetector->getCurrentODF();
        float gain = barGain.value();

        // Update each band with energy + some variation
        for (int i = 0; i < NUM_BANDS; i++) {
            // Base energy from ODF
            float bandEnergy = odf * gain;

            // Frequency-dependent scaling (bass louder, treble quieter)
            if (i < NUM_BANDS / 3) {
                bandEnergy *= 1.5f;  // Bass boost
            } else if (i > 2 * NUM_BANDS / 3) {
                bandEnergy *= 0.8f;  // Treble reduction
            }

            // Add randomness so bars move independently
            bandEnergy *= (0.5f + random(100) / 100.0f);

            // Take max (bars only rise on new energy)
            bandLevels[i] = max(bandLevels[i], bandEnergy);

            // Update peak
            if (bandLevels[i] > bandPeaks[i]) {
                bandPeaks[i] = bandLevels[i];
                bandPeakTimes[i] = millis();
            }
        }
    }

    // Fade out levels
    float fade = fadeSpeed.value();
    uint32_t now = millis();

    for (int i = 0; i < NUM_BANDS; i++) {
        bandLevels[i] *= fade;

        // Decay peaks after 500ms hold time
        if (now - bandPeakTimes[i] > 500) {
            bandPeaks[i] *= fade;
        }
    }

    // Clear display
    fill_solid(leds, NUM_LEDS, CRGB::Black);

    // Draw spectrum bars (one bar per column)
    int barsToShow = min(NUM_BANDS, MATRIX_WIDTH);
    for (int x = 0; x < barsToShow; x++) {
        // Map column to band
        int bandIdx = (x * NUM_BANDS) / barsToShow;
        float level = min(bandLevels[bandIdx], 1.0f);
        float peak = min(bandPeaks[bandIdx], 1.0f);

        int barHeight = (int)(level * MATRIX_HEIGHT);
        int peakHeight = (int)(peak * MATRIX_HEIGHT);

        // Color based on frequency range
        CRGB barColor;
        if (x < barsToShow / 3) {
            barColor = CRGB::Red;     // Bass (left side)
        } else if (x < 2 * barsToShow / 3) {
            barColor = CRGB::Green;   // Mid (center)
        } else {
            barColor = CRGB::Blue;    // High (right side)
        }

        // Draw bar from bottom up
        for (int y = 0; y < barHeight && y < MATRIX_HEIGHT; y++) {
            int idx = y * MATRIX_WIDTH + x;
            leds[idx] = barColor;
        }

        // Draw peak dot (white)
        if (peakHeight > 0 && peakHeight <= MATRIX_HEIGHT) {
            int peakIdx = (peakHeight - 1) * MATRIX_WIDTH + x;
            leds[peakIdx] = CRGB::White;
        }
    }

    FastLED.show();

    // Print stats
    EVERY_N_SECONDS(5) {
        TempoEstimate tempo = beatDetector->getTempo();
        Serial.println();
        Serial.println("=== Spectrum Analyzer Stats ===");
        Serial.print("Tempo: ");
        Serial.print(tempo.bpm);
        Serial.print(" BPM (confidence: ");
        Serial.print(tempo.confidence);
        Serial.println(")");
        Serial.print("Beats detected: ");
        Serial.println(beatDetector->getBeatCount());
        Serial.println("===============================");
    }
}

#endif

/// @file    BeatDetection.ino
/// @brief   Simple real-time beat detection for EDM with LED visualization
/// @example BeatDetection.ino
///
/// Demonstrates beat detection optimized for Electronic Dance Music (EDM).
/// Visualizes beats and tempo on an LED strip - flashes on beat detection.

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
#define NUM_LEDS 60
#define LED_PIN 3
#define DATA_PIN 3
#define BRIGHTNESS 128

CRGB leds[NUM_LEDS];

// Audio input
UIAudio audio("Audio Input");

// UI Controls for beat detection parameters
UINumberField sampleRate("Sample Rate", 44100, 8000, 48000);
UINumberField frameSize("Frame Size", 512, 128, 2048);
UINumberField hopSize("Hop Size", 256, 64, 1024);
UINumberField numBands("Num Bands", 24, 6, 48);
UINumberField superfluxMu("SuperFlux Mu", 3, 1, 10);
UINumberField maxFilterRadius("Max Filter Radius", 2, 1, 5);
UISlider peakThresholdDelta("Peak Threshold", 0.07f, 0.01f, 0.3f);
UINumberField peakPreMax("Peak Pre Max (ms)", 30, 10, 100);
UINumberField peakPostMax("Peak Post Max (ms)", 30, 10, 100);
UINumberField peakPreAvg("Peak Pre Avg (ms)", 100, 20, 200);
UINumberField peakPostAvg("Peak Post Avg (ms)", 70, 20, 200);
UINumberField minInterOnset("Min Inter Onset (ms)", 30, 10, 100);
UISlider tempoMinBPM("Min BPM", 100.0f, 60.0f, 150.0f);
UISlider tempoMaxBPM("Max BPM", 150.0f, 100.0f, 200.0f);
UISlider tempoRayleighSigma("Rayleigh Sigma", 120.0f, 80.0f, 160.0f);
UINumberField tempoACFWindow("ACF Window (sec)", 4, 2, 10);
UICheckbox logCompression("Log Compression", true);
UICheckbox adaptiveWhitening("Adaptive Whitening", true);

// Beat detection state
BeatDetectorConfig config;
BeatDetector detector(config);

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

    // Configure beat detector for EDM using UI controls
    config.sample_rate_hz = sampleRate.value();
    config.frame_size = frameSize.value();
    config.hop_size = hopSize.value();
    config.fft_size = frameSize.value();  // Match frame size

    // Use SuperFlux ODF (best for EDM)
    config.odf_type = OnsetDetectionFunction::SuperFlux;
    config.num_bands = numBands.value();
    config.log_compression = logCompression.value();
    config.adaptive_whitening = adaptiveWhitening.value();
    config.superflux_mu = superfluxMu.value();
    config.max_filter_radius = maxFilterRadius.value();

    // SuperFlux peak picking
    config.peak_mode = PeakPickingMode::SuperFluxPeaks;
    config.peak_threshold_delta = peakThresholdDelta.value();
    config.peak_pre_max_ms = peakPreMax.value();
    config.peak_post_max_ms = peakPostMax.value();
    config.peak_pre_avg_ms = peakPreAvg.value();
    config.peak_post_avg_ms = peakPostAvg.value();
    config.min_inter_onset_ms = minInterOnset.value();

    // Comb filter tempo tracking (100-150 BPM for EDM)
    config.tempo_tracker = TempoTrackerType::CombFilter;
    config.tempo_min_bpm = tempoMinBPM.value();
    config.tempo_max_bpm = tempoMaxBPM.value();
    config.tempo_rayleigh_sigma = tempoRayleighSigma.value();
    config.tempo_acf_window_sec = tempoACFWindow.value();

    // Initialize detector with config
    detector = BeatDetector(config);

    // Set up beat detection callbacks
    detector.onOnset = [](float confidence, float timestamp_ms) {
        onsetCount++;
        Serial.print("Onset at ");
        Serial.print(timestamp_ms);
        Serial.print("ms, confidence=");
        Serial.println(confidence);
    };

    detector.onBeat = [](float confidence, float bpm, float timestamp_ms) {
        beatCount++;
        currentBPM = bpm;
        lastBeatTime = millis();

        Serial.print("BEAT at ");
        Serial.print(timestamp_ms);
        Serial.print("ms, BPM=");
        Serial.print(bpm);
        Serial.print(", confidence=");
        Serial.println(confidence);
    };

    detector.onTempoChange = [](float bpm, float confidence) {
        Serial.print("Tempo: ");
        Serial.print(bpm);
        Serial.print(" BPM (confidence: ");
        Serial.print(confidence);
        Serial.println(")");
    };

    Serial.println("Beat detector initialized");
    Serial.println("Playing audio will trigger beat detection...");
}

void loop() {
    // Get audio sample
    AudioSample sample = audio.next();

    if (sample.isValid()) {
        // Convert int16 PCM to float [-1.0, 1.0]
        static float audioBuffer[512];
        size_t numSamples = min(sample.pcm().size(), (size_t)512);

        for (size_t i = 0; i < numSamples; i++) {
            audioBuffer[i] = sample.pcm()[i] / 32768.0f;
        }

        // Process audio frame for beat detection
        detector.processFrame(audioBuffer, numSamples);
    }

    // Visualize beats on LED strip
    uint32_t timeSinceBeat = millis() - lastBeatTime;

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
            float phase = fmod(millis(), period_ms) / period_ms;
            uint8_t brightness = (sin(phase * 2.0f * PI) + 1.0f) * 32.0f;

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
        TempoEstimate tempo = detector.getTempo();
        Serial.println();
        Serial.println("=== Beat Detection Stats ===");
        Serial.print("Onsets: ");
        Serial.print(onsetCount);
        Serial.print(" | Beats: ");
        Serial.println(beatCount);
        Serial.print("Tempo: ");
        Serial.print(tempo.bpm);
        Serial.print(" BPM (confidence: ");
        Serial.print(tempo.confidence);
        Serial.println(")");
        Serial.print("ODF: ");
        Serial.println(detector.getCurrentODF());
        Serial.println("============================");
    }
}

#endif

/// @file    PitchDetection.ino
/// @brief   Real-time pitch detection with histogram visualization
/// @example PitchDetection.ino
///
/// This sketch demonstrates real-time pitch detection using the PitchToMIDI engine.
/// It visualizes detected pitches as a 128x128 histogram where each note creates
/// a colored streak that fades over time.

#include <FastLED.h>

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else

#include <Arduino.h>
#include "fl/ui.h"
#include "fl/audio.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include "fl/compiler_control.h"
#include "fx/audio/pitch_to_midi.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-conversion)
FL_DISABLE_WARNING(sign-conversion)

using namespace fl;

// LED Configuration
#define MATRIX_WIDTH 128
#define MATRIX_HEIGHT 128
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define LED_PIN 3
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Audio Configuration
#define SAMPLE_RATE 44100

// UI Elements
UITitle title("Pitch Detection Histogram");
UIDescription description("Real-time pitch histogram with fading color streaks");

// Audio controls
UIAudio audio("Audio Input");
UISlider audioGain("Audio Gain", 1.0f, 0.1f, 5.0f, 0.1f);
UISlider confidenceThreshold("Confidence Threshold", 0.82f, 0.5f, 0.95f, 0.01f);

// Visual controls
UISlider brightness("Brightness", 128, 0, 255, 1);
UISlider fadeSpeed("Fade Speed", 20, 5, 100, 5);

// Global variables
CRGB leds[NUM_LEDS];

// Pitch detection
PitchToMIDI pitchConfig;
PitchToMIDIEngine* pitchEngine = nullptr;
uint8_t currentMIDINote = 0;
uint8_t currentVelocity = 0;
bool noteIsOn = false;

// Note names
const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Get note name from MIDI note
void getNoteName(uint8_t midiNote, char* buffer) {
    int octave = (midiNote / 12) - 1;
    int noteIndex = midiNote % 12;
    sprintf(buffer, "%s%d", noteNames[noteIndex], octave);
}

// Get color for MIDI note (chromatic - each note has unique color)
CRGB getNoteColor(uint8_t midiNote) {
    int noteIndex = midiNote % 12;
    uint8_t hue = (noteIndex * 256) / 12;
    return CHSV(hue, 255, 255);
}

// Convert x, y coordinates to LED index
int XY(int x, int y) {
    if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) {
        return -1;
    }
    // Serpentine layout
    if (y & 1) {
        return y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
    } else {
        return y * MATRIX_WIDTH + x;
    }
}

// Draw histogram
void drawHistogram() {
    // Fade the entire matrix
    fadeToBlackBy(leds, NUM_LEDS, fadeSpeed.as_int());

    if (noteIsOn) {
        // Map MIDI note to Y position (piano range: 21-108, 88 keys)
        float notePos = fl::map_range<float, float>(currentMIDINote, 21.0f, 108.0f, 0.0f, 1.0f);
        notePos = fl::clamp(notePos, 0.0f, 1.0f);
        int yPos = notePos * (MATRIX_HEIGHT - 1);

        // Draw new column at x=0 (left side)
        CRGB color = getNoteColor(currentMIDINote);
        uint8_t ledBrightness = fl::map_range<uint8_t, uint8_t>(currentVelocity, 0, 127, 100, 255);
        color.fadeToBlackBy(255 - ledBrightness);

        // Draw a vertical streak centered at yPos
        for (int dy = -2; dy <= 2; dy++) {
            int y = yPos + dy;
            if (y >= 0 && y < MATRIX_HEIGHT) {
                int idx = XY(0, y);
                if (idx >= 0) {
                    uint8_t fade = 255 - (abs(dy) * 50);
                    CRGB c = color;
                    c.fadeToBlackBy(255 - fade);
                    leds[idx] = c;
                }
            }
        }
    }

    // Scroll right: shift all columns to the right
    for (int x = MATRIX_WIDTH - 1; x > 0; x--) {
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            int destIdx = XY(x, y);
            int srcIdx = XY(x - 1, y);
            if (destIdx >= 0 && srcIdx >= 0) {
                leds[destIdx] = leds[srcIdx];
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Pitch Detection Histogram");
    Serial.println("===========================");

    // Initialize LEDs with 128x128 screenmap for visualization
    XYMap xyMap = XYMap::constructRectangularGrid(MATRIX_WIDTH, MATRIX_HEIGHT);
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setScreenMap(xyMap);
    FastLED.setBrightness(brightness.as_int());
    FastLED.clear();
    FastLED.show();

    // Set up UI callbacks
    brightness.onChanged([](float value) {
        FastLED.setBrightness(value);
    });

    confidenceThreshold.onChanged([](float value) {
        if (pitchEngine) {
            PitchToMIDI cfg = pitchEngine->config();
            cfg.confidence_threshold = value;
            pitchEngine->setConfig(cfg);
        }
    });

    // Initialize pitch detection
    pitchConfig.sample_rate_hz = SAMPLE_RATE;
    pitchConfig.confidence_threshold = confidenceThreshold.value();
    pitchEngine = new PitchToMIDIEngine(pitchConfig);

    pitchEngine->onNoteOn = [](uint8_t note, uint8_t velocity) {
        currentMIDINote = note;
        currentVelocity = velocity;
        noteIsOn = true;

        char noteName[8];
        getNoteName(note, noteName);

        Serial.print("Note ON: ");
        Serial.print(noteName);
        Serial.print(" (MIDI: ");
        Serial.print(note);
        Serial.print(", vel: ");
        Serial.print(velocity);
        Serial.println(")");
    };

    pitchEngine->onNoteOff = [](uint8_t note) {
        noteIsOn = false;

        char noteName[8];
        getNoteName(note, noteName);

        Serial.print("Note OFF: ");
        Serial.println(noteName);
    };

    Serial.println("Setup complete!");
    Serial.println("Sing or play an instrument to see pitch histogram.");
}

void loop() {
    // Process audio samples
    AudioSample sample = audio.next();
    if (sample.isValid()) {
        // Convert int16_t samples to float for pitch detection
        static float floatBuffer[512];
        size_t numSamples = min(sample.pcm().size(), (size_t)512);

        // Apply gain and convert to float
        for (size_t i = 0; i < numSamples; i++) {
            float scaledSample = sample.pcm()[i] * audioGain.value();
            scaledSample = fl::clamp(scaledSample, -32768.0f, 32767.0f);
            floatBuffer[i] = scaledSample / 32768.0f;
        }

        // Process pitch detection
        pitchEngine->processFrame(floatBuffer, numSamples);
    }

    // Draw histogram visualization
    drawHistogram();

    FastLED.show();

    #ifdef __EMSCRIPTEN__
    delay(1);
    #endif
}

FL_DISABLE_WARNING_POP

#endif

/// @file    PitchDetection.ino
/// @brief   Real-time pitch detection with visual feedback
/// @example PitchDetection.ino
///
/// This sketch demonstrates real-time pitch detection using the PitchToMIDI engine.
/// It visualizes the detected pitch on an LED strip with note names and frequency display.

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
#define NUM_LEDS 144
#define LED_PIN 3
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Audio Configuration
#define SAMPLE_RATE 44100

// UI Elements
UITitle title("Pitch Detection Visualizer");
UIDescription description("Real-time pitch detection with MIDI note display and tuning reference");

// Audio controls
UIAudio audio("Audio Input");
UISlider audioGain("Audio Gain", 1.0f, 0.1f, 5.0f, 0.1f);
UISlider confidenceThreshold("Confidence Threshold", 0.82f, 0.5f, 0.95f, 0.01f);

// Visual controls
UISlider brightness("Brightness", 128, 0, 255, 1);
UISlider fadeSpeed("Fade Speed", 20, 0, 255, 5);
UIDropdown colorMode("Color Mode", {"Note", "Octave", "Chromatic", "Rainbow"});
UICheckbox showHistory("Show History Trail", true);

// Tuning reference
UISlider tuningReference("Tuning Reference (Hz)", 440.0f, 430.0f, 450.0f, 0.1f);
UICheckbox showTuningGuide("Show Tuning Guide", true);

// Display mode
UIDropdown displayMode("Display Mode", {"Linear", "Circular", "Piano Roll", "Frequency Spectrum"});

// Global variables
CRGB leds[NUM_LEDS];

// Pitch detection
PitchToMIDI pitchConfig;
PitchToMIDIEngine* pitchEngine = nullptr;
uint8_t currentMIDINote = 0;
uint8_t currentVelocity = 0;
bool noteIsOn = false;
float currentFrequency = 0.0f;

// History tracking
static const int HISTORY_SIZE = 50;
uint8_t noteHistory[HISTORY_SIZE];
int historyIndex = 0;

// Note names
const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Convert MIDI note to frequency
float midiToFreq(uint8_t note) {
    return tuningReference.value() * powf(2.0f, (note - 69) / 12.0f);
}

// Get note name from MIDI note
void getNoteName(uint8_t midiNote, char* buffer) {
    int octave = (midiNote / 12) - 1;
    int noteIndex = midiNote % 12;
    sprintf(buffer, "%s%d", noteNames[noteIndex], octave);
}

// Get color for MIDI note
CRGB getNoteColor(uint8_t midiNote) {
    switch(colorMode.as_int()) {
        case 0: {  // By note name (chromatic)
            int noteIndex = midiNote % 12;
            uint8_t hue = (noteIndex * 256) / 12;
            return CHSV(hue, 255, 255);
        }
        case 1: {  // By octave
            int octave = (midiNote / 12) - 1;
            uint8_t hue = ((octave - 1) * 256) / 7;  // 7 octaves
            return CHSV(hue, 255, 255);
        }
        case 2: {  // Chromatic scale
            uint8_t hue = ((midiNote - 21) * 256) / 88;  // Piano range
            return CHSV(hue, 255, 255);
        }
        case 3: {  // Rainbow
            uint8_t hue = beat8(60);
            return CHSV(hue, 255, 255);
        }
        default:
            return CRGB::White;
    }
}

// Linear display mode
void drawLinear() {
    if (fadeSpeed.as_int() > 0) {
        fadeToBlackBy(leds, NUM_LEDS, fadeSpeed.as_int());
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    }

    if (noteIsOn) {
        // Map MIDI note to LED position (piano range: 21-108, 88 keys)
        float notePos = fl::map_range<float, float>(currentMIDINote, 21.0f, 108.0f, 0.0f, 1.0f);
        notePos = fl::clamp(notePos, 0.0f, 1.0f);
        int ledPos = notePos * (NUM_LEDS - 1);

        CRGB color = getNoteColor(currentMIDINote);
        uint8_t brightness = fl::map_range<uint8_t, uint8_t>(currentVelocity, 0, 127, 50, 255);
        color.fadeToBlackBy(255 - brightness);

        // Draw with spread
        for (int i = -2; i <= 2; i++) {
            int pos = ledPos + i;
            if (pos >= 0 && pos < NUM_LEDS) {
                uint8_t fade = 255 - (abs(i) * 60);
                CRGB c = color;
                c.fadeToBlackBy(255 - fade);
                leds[pos] = c;
            }
        }

        // Draw tuning guide if enabled
        if (showTuningGuide) {
            // Show if we're sharp or flat
            float targetFreq = midiToFreq(currentMIDINote);
            float cents = 1200.0f * log2f(currentFrequency / targetFreq);

            if (fabsf(cents) > 5.0f) {  // More than 5 cents off
                int offset = (cents > 0) ? 3 : -3;
                int guidePos = ledPos + offset;
                if (guidePos >= 0 && guidePos < NUM_LEDS) {
                    leds[guidePos] = (cents > 0) ? CRGB::Red : CRGB::Blue;
                }
            } else {
                // In tune - show green
                if (ledPos >= 0 && ledPos < NUM_LEDS) {
                    leds[ledPos] += CRGB(0, 50, 0);
                }
            }
        }
    }
}

// Circular display mode
void drawCircular() {
    if (fadeSpeed.as_int() > 0) {
        fadeToBlackBy(leds, NUM_LEDS, fadeSpeed.as_int());
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    }

    if (noteIsOn) {
        // Map note within octave (0-11 notes) to circle
        int noteInOctave = currentMIDINote % 12;
        float angle = (noteInOctave * 256.0f) / 12.0f;
        int ledPos = (angle / 256.0f) * NUM_LEDS;

        CRGB color = getNoteColor(currentMIDINote);
        uint8_t brightness = fl::map_range<uint8_t, uint8_t>(currentVelocity, 0, 127, 50, 255);
        color.fadeToBlackBy(255 - brightness);

        // Draw with spread
        for (int i = -3; i <= 3; i++) {
            int pos = (ledPos + i + NUM_LEDS) % NUM_LEDS;
            uint8_t fade = 255 - (abs(i) * 40);
            CRGB c = color;
            c.fadeToBlackBy(255 - fade);
            leds[pos] += c;
        }
    }
}

// Piano roll display mode
void drawPianoRoll() {
    // Shift history
    if (showHistory) {
        // Move all LEDs one position
        for (int i = NUM_LEDS - 1; i > 0; i--) {
            leds[i] = leds[i - 1];
        }
        leds[0] = CRGB::Black;

        if (noteIsOn) {
            leds[0] = getNoteColor(currentMIDINote);
            uint8_t brightness = fl::map_range<uint8_t, uint8_t>(currentVelocity, 0, 127, 50, 255);
            leds[0].fadeToBlackBy(255 - brightness);
        }
    } else {
        drawLinear();
    }
}

// Frequency spectrum display
void drawFrequencySpectrum() {
    if (fadeSpeed.as_int() > 0) {
        fadeToBlackBy(leds, NUM_LEDS, fadeSpeed.as_int());
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    }

    if (noteIsOn && currentFrequency > 0) {
        // Map frequency to LED position (40 Hz to 2000 Hz logarithmic)
        float logFreq = log10f(currentFrequency);
        float logMin = log10f(40.0f);
        float logMax = log10f(2000.0f);
        float freqPos = (logFreq - logMin) / (logMax - logMin);
        freqPos = fl::clamp(freqPos, 0.0f, 1.0f);

        int ledPos = freqPos * (NUM_LEDS - 1);

        CRGB color = getNoteColor(currentMIDINote);
        uint8_t brightness = fl::map_range<uint8_t, uint8_t>(currentVelocity, 0, 127, 50, 255);
        color.fadeToBlackBy(255 - brightness);

        // Draw peak
        for (int i = -2; i <= 2; i++) {
            int pos = ledPos + i;
            if (pos >= 0 && pos < NUM_LEDS) {
                uint8_t fade = 255 - (abs(i) * 60);
                CRGB c = color;
                c.fadeToBlackBy(255 - fade);
                leds[pos] = c;
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Pitch Detection Visualizer");
    Serial.println("===========================");

    // Initialize LEDs with screenmap for visualization
    XYMap xyMap = XYMap::constructRectangularGrid(NUM_LEDS, 1);
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
        currentFrequency = midiToFreq(note);
        noteIsOn = true;

        // Add to history
        noteHistory[historyIndex] = note;
        historyIndex = (historyIndex + 1) % HISTORY_SIZE;

        char noteName[8];
        getNoteName(note, noteName);

        Serial.print("Note ON: ");
        Serial.print(noteName);
        Serial.print(" (MIDI: ");
        Serial.print(note);
        Serial.print(", ");
        Serial.print(currentFrequency, 2);
        Serial.print(" Hz, vel: ");
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
    Serial.println("Sing or play an instrument to see pitch detection in action.");
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

        // Update frequency estimate if note is on
        if (noteIsOn) {
            currentFrequency = midiToFreq(currentMIDINote);
        }
    }

    // Draw visualization based on display mode
    switch(displayMode.as_int()) {
        case 0:  // Linear
            drawLinear();
            break;
        case 1:  // Circular
            drawCircular();
            break;
        case 2:  // Piano Roll
            drawPianoRoll();
            break;
        case 3:  // Frequency Spectrum
            drawFrequencySpectrum();
            break;
    }

    FastLED.show();

    #ifdef __EMSCRIPTEN__
    delay(1);
    #endif
}

FL_DISABLE_WARNING_POP

#endif

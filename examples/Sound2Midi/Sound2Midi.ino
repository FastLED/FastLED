/// @file    Sound2Midi.ino
/// @brief   Real-time sound to MIDI conversion with histogram visualization
/// @example Sound2Midi.ino
///
/// This sketch demonstrates real-time sound to MIDI conversion using the SoundToMIDI engine.
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
#include "fx/audio/sound_to_midi.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(float-conversion)
FL_DISABLE_WARNING(sign-conversion)

using namespace fl;

// LED Configuration
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 32
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define LED_PIN 3
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Audio Configuration
#define SAMPLE_RATE 44100

// UI Elements
UITitle title("Sound to MIDI Histogram");
UIDescription description(
    "Real-time sound to MIDI conversion with histogram visualization.\n\n"
    "## Audio Settings\n"
    "**Audio Gain** (0.1-5.0): Amplifies input signal. Increase for quiet sources, decrease for loud ones to prevent clipping.\n"
    "**Confidence Threshold** (0.5-0.95): Minimum confidence to trigger note detection. Higher = fewer false positives. Recommended: 0.75-0.85 for instruments, 0.65-0.75 for voice.\n\n"
    "## Stability Controls (Anti-Jitter)\n"
    "**Semitone Threshold** (1-5): Minimum pitch change to register new note. 1=responsive/may flicker, 2-3=balanced, 4-5=stable/less responsive.\n"
    "**Note Change Hold Frames** (1-20): Consecutive frames needed before switching notes. Lower=faster/responsive, higher=stable/slower.\n"
    "**Median Filter Size** (1-11): Smooths pitch outliers. 1=no filtering, 3-5=balanced, 7-11=maximum smoothing.\n\n"
    "## Modes\n"
    "**Polyphonic Mode**: OFF=single note (monophonic), ON=multiple simultaneous notes (up to 16).\n"
    "**One Frame Mode**: OFF=notes sustain, ON=notes flash once (impulse/trigger mode).\n\n"
    "## Visual\n"
    "**Brightness** (0-255): Overall LED brightness.\n"
    "**Fade Speed** (5-100): Trail fade rate. Higher=faster fade/shorter trails.\n"
    "**Test White**: Button to test LEDs with white while pressed.\n\n"
    "## Visualization\n"
    "Notes shown as colored streaks: vertical position=pitch (low to high), color=chromatic mapping (12 semitones), brightness=velocity. New notes appear left, scroll right.\n\n"
    "## Tuning Presets\n"
    "**Simple Melodies**: Semitone 1-2, Hold 2-4, Filter 3, Confidence 0.75-0.8\n"
    "**Fast Passages**: Semitone 1, Hold 1-2, Filter 1-3, Confidence 0.7-0.75\n"
    "**Sustained Notes**: Semitone 2-3, Hold 5-10, Filter 5-7, Confidence 0.75-0.85\n"
    "**Noisy Environments**: Semitone 2-3, Hold 5-8, Filter 5-7, Confidence 0.85-0.9"
);

// Audio controls
UIAudio audio("Audio Input");
UISlider audioGain("Audio Gain", 1.0f, 0.1f, 5.0f, 0.1f);
UISlider confidenceThreshold("Confidence Threshold", 0.75f, 0.5f, 0.95f, 0.01f);

// Stability controls (anti-jitter)
// Optimized for simple melodies like "Mary Had a Little Lamb"
// - Lower semitone threshold for well-separated notes
// - Shorter hold frames for faster note transitions
// - Smaller median filter for more responsive detection
UISlider semitoneThreshold("Semitone Threshold", 1, 1, 5, 1);
UISlider noteChangeHoldFrames("Note Change Hold", 3, 1, 20, 1);
UISlider medianFilterSize("Median Filter Size", 3, 1, 11, 1);

// Visual controls
UISlider brightness("Brightness", 128, 0, 255, 1);
UISlider fadeSpeed("Fade Speed", 20, 5, 100, 5);
UIButton testWhite("Test All White");

// Polyphonic mode
UICheckbox polyphonicMode("Polyphonic Mode", false);
UICheckbox oneFrameMode("One Frame Mode", false);

// Global variables
CRGB leds[NUM_LEDS];

// Pitch detection
SoundToMIDI pitchConfig;
SoundToMIDIBase* pitchEngine = nullptr;

// Monophonic state
uint8_t currentMIDINote = 0;
uint8_t currentVelocity = 0;
bool noteIsOn = false;
bool noteJustHit = false;  // For one-frame mode visualization

// Polyphonic state (track up to 16 simultaneous notes)
struct ActiveNote {
    uint8_t midiNote;
    uint8_t velocity;
    bool active;
    bool justHit;  // For one-frame mode visualization
};
ActiveNote activeNotes[16];
int numActiveNotes = 0;

bool anyNoteDetected = false;
uint32_t lastNoteTime = 0;

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
    // Rectangular layout
    return y * MATRIX_WIDTH + x;
}

// Draw checkerboard pattern
void drawCheckerboard() {
    const int checkerSize = 8;
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            int idx = XY(x, y);
            if (idx >= 0) {
                bool isRed = ((x / checkerSize) + (y / checkerSize)) & 1;
                leds[idx] = isRed ? CRGB(20, 0, 0) : CRGB(0, 0, 0);
            }
        }
    }
}

// Draw histogram
void drawHistogram() {
    // If no note has been detected for 2 seconds, show checkerboard
    if (!anyNoteDetected || (millis() - lastNoteTime > 2000)) {
        drawCheckerboard();
        return;
    }

    // Fade the entire matrix
    fadeToBlackBy(leds, NUM_LEDS, fadeSpeed.as_int());

    if (polyphonicMode.value()) {
        // Polyphonic mode: draw all active notes
        for (int i = 0; i < numActiveNotes; i++) {
            bool shouldDraw = activeNotes[i].active || (oneFrameMode.value() && activeNotes[i].justHit);
            if (shouldDraw) {
                // Map MIDI note to Y position (piano range: 21-108, 88 keys)
                float notePos = fl::map_range<float, float>(activeNotes[i].midiNote, 21.0f, 108.0f, 0.0f, 1.0f);
                notePos = fl::clamp(notePos, 0.0f, 1.0f);
                int yPos = notePos * (MATRIX_HEIGHT - 1);

                // Draw new column at x=0 (left side)
                CRGB color = getNoteColor(activeNotes[i].midiNote);
                uint8_t ledBrightness = fl::map_range<uint8_t, uint8_t>(activeNotes[i].velocity, 0, 127, 100, 255);
                color.fadeToBlackBy(255 - ledBrightness);

                // Draw a single dot at yPos
                int idx = XY(0, yPos);
                if (idx >= 0) {
                    // Blend if multiple notes at same position
                    leds[idx] += color;
                }
            }
            // Clear justHit flag after drawing
            activeNotes[i].justHit = false;
        }
    } else {
        // Monophonic mode: draw single note
        bool shouldDraw = noteIsOn || (oneFrameMode.value() && noteJustHit);
        if (shouldDraw) {
            // Map MIDI note to Y position (piano range: 21-108, 88 keys)
            float notePos = fl::map_range<float, float>(currentMIDINote, 21.0f, 108.0f, 0.0f, 1.0f);
            notePos = fl::clamp(notePos, 0.0f, 1.0f);
            int yPos = notePos * (MATRIX_HEIGHT - 1);

            // Draw new column at x=0 (left side)
            CRGB color = getNoteColor(currentMIDINote);
            uint8_t ledBrightness = fl::map_range<uint8_t, uint8_t>(currentVelocity, 0, 127, 100, 255);
            color.fadeToBlackBy(255 - ledBrightness);

            // Draw a single dot at yPos
            int idx = XY(0, yPos);
            if (idx >= 0) {
                leds[idx] = color;
            }
        }
        // Clear justHit flag after drawing
        noteJustHit = false;
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

// Setup pitch engine callbacks
void setupPitchEngineCallbacks() {
    if (!pitchEngine) return;

    pitchEngine->onNoteOn = [](uint8_t note, uint8_t velocity) {
        anyNoteDetected = true;
        lastNoteTime = millis();

        char noteName[8];
        getNoteName(note, noteName);

        if (polyphonicMode.value()) {
            // Polyphonic mode: add to active notes
            int slot = -1;
            // First, check if note is already active (update velocity)
            for (int i = 0; i < numActiveNotes; i++) {
                if (activeNotes[i].active && activeNotes[i].midiNote == note) {
                    activeNotes[i].velocity = velocity;
                    slot = i;
                    break;
                }
            }
            // If not found, add to first available slot
            if (slot == -1) {
                for (int i = 0; i < 16; i++) {
                    if (!activeNotes[i].active) {
                        activeNotes[i].midiNote = note;
                        activeNotes[i].velocity = velocity;
                        activeNotes[i].active = !oneFrameMode.value();  // Only keep active if not in one-frame mode
                        activeNotes[i].justHit = true;
                        if (i >= numActiveNotes) {
                            numActiveNotes = i + 1;
                        }
                        slot = i;
                        break;
                    }
                }
            } else {
                // Note was already active, mark as just hit
                activeNotes[slot].justHit = true;
            }

            Serial.print("Note ON (poly): ");
            Serial.print(noteName);
            Serial.print(" (MIDI: ");
            Serial.print(note);
            Serial.print(", vel: ");
            Serial.print(velocity);
            Serial.print(") - Active notes: ");
            int count = 0;
            for (int i = 0; i < 16; i++) {
                if (activeNotes[i].active) count++;
            }
            Serial.println(count);
        } else {
            // Monophonic mode
            currentMIDINote = note;
            currentVelocity = velocity;
            noteIsOn = !oneFrameMode.value();  // Only keep on if not in one-frame mode
            noteJustHit = true;

            Serial.print("Note ON: ");
            Serial.print(noteName);
            Serial.print(" (MIDI: ");
            Serial.print(note);
            Serial.print(", vel: ");
            Serial.print(velocity);
            Serial.println(")");
        }
    };

    pitchEngine->onNoteOff = [](uint8_t note) {
        lastNoteTime = millis();

        char noteName[8];
        getNoteName(note, noteName);

        // In one-frame mode, note offs are ignored (notes already inactive)
        if (!oneFrameMode.value()) {
            if (polyphonicMode.value()) {
                // Polyphonic mode: remove from active notes
                for (int i = 0; i < 16; i++) {
                    if (activeNotes[i].active && activeNotes[i].midiNote == note) {
                        activeNotes[i].active = false;
                        break;
                    }
                }
                // Compact the array
                while (numActiveNotes > 0 && !activeNotes[numActiveNotes - 1].active) {
                    numActiveNotes--;
                }

                Serial.print("Note OFF (poly): ");
                Serial.print(noteName);
                Serial.print(" - Active notes: ");
                int count = 0;
                for (int i = 0; i < 16; i++) {
                    if (activeNotes[i].active) count++;
                }
                Serial.println(count);
            } else {
                // Monophonic mode
                noteIsOn = false;

                Serial.print("Note OFF: ");
                Serial.print(noteName);
                Serial.println();
            }
        }
    };
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Sound to MIDI Histogram");
    Serial.println("=======================");

    // Initialize LEDs with rectangular screenmap for visualization
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
            SoundToMIDI cfg = pitchEngine->config();
            cfg.confidence_threshold = value;
            pitchEngine->setConfig(cfg);
        }
    });

    semitoneThreshold.onChanged([](float value) {
        if (pitchEngine) {
            SoundToMIDI cfg = pitchEngine->config();
            cfg.note_change_semitone_threshold = (int)value;
            pitchEngine->setConfig(cfg);
        }
    });

    noteChangeHoldFrames.onChanged([](float value) {
        if (pitchEngine) {
            SoundToMIDI cfg = pitchEngine->config();
            cfg.note_change_hold_frames = (int)value;
            pitchEngine->setConfig(cfg);
        }
    });

    medianFilterSize.onChanged([](float value) {
        if (pitchEngine) {
            SoundToMIDI cfg = pitchEngine->config();
            cfg.median_filter_size = (int)value;
            pitchEngine->setConfig(cfg);
        }
    });

    testWhite.onPressed([]() {
        FL_WARN("Test White button pressed - setting all LEDs to white");
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB(255, 255, 255);
        }
        FastLED.show();
        Serial.println("Button pressed - All LEDs set to white (255, 255, 255)");
    });

    testWhite.onReleased([]() {
        FL_WARN("Test White button released");
        Serial.println("Button released");
    });

    polyphonicMode.onChanged([](bool value) {
        if (pitchEngine) {
            // Get current config and delete old engine
            SoundToMIDI cfg = pitchEngine->config();
            delete pitchEngine;

            // Create new engine with appropriate type
            if (value) {
                pitchEngine = new SoundToMIDIPoly(cfg);
            } else {
                pitchEngine = new SoundToMIDIMono(cfg);
            }

            // Re-setup callbacks for the new engine
            setupPitchEngineCallbacks();

            // Clear active notes when switching modes
            numActiveNotes = 0;
            for (int i = 0; i < 16; i++) {
                activeNotes[i].active = false;
                activeNotes[i].justHit = false;
            }
            noteIsOn = false;
            noteJustHit = false;

            Serial.print("Polyphonic mode ");
            Serial.println(value ? "ENABLED" : "DISABLED");
        }
    });

    // Initialize pitch detection with stability settings
    pitchConfig.sample_rate_hz = SAMPLE_RATE;
    pitchConfig.confidence_threshold = confidenceThreshold.value();
    pitchConfig.note_change_semitone_threshold = semitoneThreshold.as_int();
    pitchConfig.note_change_hold_frames = noteChangeHoldFrames.as_int();
    pitchConfig.median_filter_size = medianFilterSize.as_int();

    // Create engine based on polyphonic mode
    if (polyphonicMode.value()) {
        pitchEngine = new SoundToMIDIPoly(pitchConfig);
    } else {
        pitchEngine = new SoundToMIDIMono(pitchConfig);
    }

    // Initialize active notes array
    for (int i = 0; i < 16; i++) {
        activeNotes[i].active = false;
        activeNotes[i].justHit = false;
    }

    // Setup pitch engine callbacks
    setupPitchEngineCallbacks();

    Serial.println("Setup complete!");
    Serial.println("Sing or play an instrument to see sound to MIDI conversion.");
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

    // Draw histogram visualization at fixed frame rate
    EVERY_N_MILLISECONDS(16) {  // ~60 FPS
        // Skip drawing histogram if test white button is pressed
        if (testWhite.isPressed()) {
            FL_WARN("Test White button is pressed - skipping histogram draw");
            // Keep LEDs white while button is held
            for (int i = 0; i < NUM_LEDS; i++) {
                leds[i] = CRGB(255, 255, 255);
            }
            FastLED.show();
        } else {
            drawHistogram();
            FastLED.show();
        }
    }
}

FL_DISABLE_WARNING_POP

#endif

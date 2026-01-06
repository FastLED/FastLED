/// @file    Audio.ino
/// @brief   Audio visualization example with XY mapping
/// @example Audio.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.  
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

/*
This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

#include <Arduino.h>
#include <FastLED.h>

#include "fl/audio.h"
#include "fl/downscale.h"
#include "fl/draw_visitor.h"
#include "fl/fft.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "fl/raster.h"
#include "fl/time_alpha.h"
#include "fl/ui.h"
#include "fl/xypath.h"
#include "fl/unused.h"
#include "fl/fx/time.h"
#include "fl/stl/function.h"

// Sketch.
#include "fx_audio.h"

#include "fl/stl/cstring.h"
using namespace fl;

#define HEIGHT 128
#define WIDTH 128
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE false
#define TIME_ANIMATION 1000 // ms
#define PIN_DATA 3

UITitle title("Simple control of an xy path");
UIDescription description("This is more of a test for new features.");
UICheckbox enableVolumeVis("Enable volume visualization", false);
UICheckbox enableRMS("Enable RMS visualization", false);
UICheckbox enableFFT("Enable FFT visualization", true);
UICheckbox enablePitchDetect("Enable pitch detection", false);
UICheckbox freeze("Freeze frame", false);
UIButton advanceFrame("Advance frame");
UISlider decayTimeSeconds("Fade time Seconds", .1, 0, 4, .02);
UISlider attackTimeSeconds("Attack time Seconds", .1, 0, 4, .02);
UISlider outputTimeSec("outputTimeSec", .17, 0, 2, .01);

UIAudio audio("Audio");
UISlider fadeToBlack("Fade to black by", 5, 0, 20, 1);

// Group related UI elements using UIGroup template multi-argument constructor
UIGroup visualizationControls("Visualization", enableVolumeVis, enableRMS, enableFFT, enablePitchDetect);
UIGroup audioProcessingControls("Audio Processing", decayTimeSeconds, attackTimeSeconds, outputTimeSec);
UIGroup generalControls("General Controls", freeze, advanceFrame, fadeToBlack);

MaxFadeTracker audioFadeTracker(attackTimeSeconds.value(),
                                decayTimeSeconds.value(), outputTimeSec.value(),
                                44100);

CRGB framebuffer[NUM_LEDS];
XYMap frameBufferXY(WIDTH, HEIGHT, IS_SERPINTINE);

CRGB leds[NUM_LEDS / 4]; // Downscaled buffer
XYMap ledsXY(WIDTH / 2, HEIGHT / 2,
             IS_SERPINTINE); // Framebuffer is regular rectangle LED matrix.

FFTBins fftOut(WIDTH); // 2x width due to super sampling.

// CRGB framebuffer[NUM_LEDS];
// CRGB framebuffer[WIDTH_2X * HEIGHT_2X];  // 2x super sampling.
// XYMap frameBufferXY(WIDTH, HEIGHT, IS_SERPINTINE);  // LED output, serpentine
// as is common for LED matrices. XYMap xyMap_2X(WIDTH_2X, HEIGHT_2X, false); //
// Framebuffer is regular rectangle LED matrix.

int x = 0;
int y = 0;
bool triggered = false;

SoundLevelMeter soundLevelMeter(.0, 0.0);

// Pitch detection engine
SoundToMIDI pitchConfig;
SoundToMIDIEngine* pitchEngine = nullptr;
uint8_t currentMIDINote = 0;
bool noteIsOn = false;

float rms(Slice<const int16_t> data) {
    double sumSq = 0.0;
    const int N = data.size();
    for (int i = 0; i < N; ++i) {
        int32_t x32 = int32_t(data[i]);
        sumSq += x32 * x32;
    }
    float rms = fl::sqrt(float(sumSq) / N);
    return rms;
}

void setup() {
    Serial.begin(115200);
    // auto screenmap = frameBufferXY.toScreenMap();
    // screenmap.setDiameter(.2);
    // FastLED.addLeds<NEOPIXEL, 2>(framebuffer,
    // NUM_LEDS).setScreenMap(screenmap);
    auto screenmap = ledsXY.toScreenMap();
    screenmap.setDiameter(.2);

    decayTimeSeconds.onChanged([](float value) {
        audioFadeTracker.setDecayTime(value);
        FASTLED_WARN("Fade time seconds: " << value);
    });
    attackTimeSeconds.onChanged([](float value) {
        audioFadeTracker.setAttackTime(value);
        FASTLED_WARN("Attack time seconds: " << value);
    });
    outputTimeSec.onChanged([](float value) {
        audioFadeTracker.setOutputTime(value);
        FASTLED_WARN("Output time seconds: " << value);
    });

    // Initialize pitch detection
    pitchConfig.sample_rate_hz = 44100.0f;
    pitchEngine = new SoundToMIDIEngine(pitchConfig);
    pitchEngine->onNoteOn = [](uint8_t note, uint8_t velocity) {
        currentMIDINote = note;
        noteIsOn = true;
        Serial.print("Note ON: ");
        Serial.print(note);
        Serial.print(" vel: ");
        Serial.println(velocity);
    };
    pitchEngine->onNoteOff = [](uint8_t note) {
        noteIsOn = false;
        Serial.print("Note OFF: ");
        Serial.println(note);
    };

    FastLED.addLeds<NEOPIXEL, PIN_DATA>(leds, ledsXY.getTotal())
        .setScreenMap(screenmap);
}

void shiftUp() {
    // fade each led by 1%
    if (fadeToBlack.as_int()) {

        for (int i = 0; i < NUM_LEDS; ++i) {
            auto &c = framebuffer[i];
            c.fadeToBlackBy(fadeToBlack.as_int());
        }
    }

    for (int y = HEIGHT - 1; y > 0; --y) {
        CRGB* row1 = &framebuffer[frameBufferXY(0, y)];
        CRGB* row2 = &framebuffer[frameBufferXY(0, y - 1)];
        fl::memcopy(row1, row2, WIDTH * sizeof(CRGB));
    }
    CRGB* row = &framebuffer[frameBufferXY(0, 0)];
    fl::memfill(row, 0, sizeof(CRGB) * WIDTH);
}


bool doFrame() {
    if (!freeze) {
        return true;
    }
    if (advanceFrame.isPressed()) {
        return true;
    }
    return false;
}

void loop() {
    if (triggered) {
        FASTLED_WARN("Triggered");
    }

    // x = pointX.as_int();
    y = HEIGHT / 2;

    bool do_frame = doFrame();

    while (AudioSample sample = audio.next()) {
        if (!do_frame) {
            continue;
        }

        // Process pitch detection if enabled
        if (enablePitchDetect && pitchEngine) {
            // Convert int16_t samples to float for pitch detection
            static float floatBuffer[512];
            size_t numSamples = fl::fl_min(sample.pcm().size(), (size_t)512);
            for (size_t i = 0; i < numSamples; i++) {
                floatBuffer[i] = sample.pcm()[i] / 32768.0f;
            }
            pitchEngine->processFrame(floatBuffer, numSamples);
        }

        float fade = audioFadeTracker(sample.pcm().data(), sample.pcm().size());
        shiftUp();
        // FASTLED_WARN("Audio sample size: " << sample.pcm().size());
        soundLevelMeter.processBlock(sample.pcm());
        // FASTLED_WARN("")
        auto dbfs = soundLevelMeter.getDBFS();
        FASTLED_UNUSED(dbfs);
        // FASTLED_WARN("getDBFS: " << dbfs);
        int32_t max = 0;
        for (size_t i = 0; i < sample.pcm().size(); ++i) {
            int32_t x = ABS(sample.pcm()[i]);
            if (x > max) {
                max = x;
            }
        }
        float anim =
            fl::map_range<float, float>(max, 0.0f, 32768.0f, 0.0f, 1.0f);
        anim = fl::clamp(anim, 0.0f, 1.0f);

        x = fl::map_range<float, float>(anim, 0.0f, 1.0f, 0.0f, WIDTH - 1);
        // FASTLED_WARN("x: " << x);

        // fft.run(sample.pcm(), &fftOut);
        sample.fft(&fftOut);

        // FASTLED_ASSERT(fftOut.bins_raw.size() == WIDTH_2X,
        //                "FFT bins size mismatch");

        if (enableFFT) {
            auto max_x = fftOut.bins_raw.size() - 1;
            FASTLED_UNUSED(max_x);
            for (size_t i = 0; i < fftOut.bins_raw.size(); ++i) {
                auto x = i;
                auto v = fftOut.bins_db[i];
                // Map audio intensity to a position in the heat palette (0-255)
                v = fl::map_range<float, float>(v, 45, 70, 0, 1.f);
                v = fl::clamp(v, 0.0f, 1.0f);
                uint8_t heatIndex =
                    fl::map_range<float, uint8_t>(v, 0, 1, 0, 255);

                // FASTLED_WARN(v);

                // Use FastLED's built-in HeatColors palette
                auto c = ColorFromPalette(HeatColors_p, heatIndex);
                c.fadeToBlackBy(255 - heatIndex);
                framebuffer[frameBufferXY(x, 0)] = c;
                // FASTLED_WARN("y: " << i << " b: " << b);
            }
        }

        if (enableVolumeVis) {
            framebuffer[frameBufferXY(x, HEIGHT / 2)] = CRGB(0, 255, 0);
        }

        if (enableRMS) {
            float rms = sample.rms();
            FASTLED_WARN("RMS: " << rms);
            rms = fl::map_range<float, float>(rms, 0.0f, 32768.0f, 0.0f, 1.0f);
            rms = fl::clamp(rms, 0.0f, 1.0f) * WIDTH;
            framebuffer[frameBufferXY(rms, HEIGHT * 3 / 4)] = CRGB(0, 0, 255);
        }

        // Display pitch detection result
        if (enablePitchDetect && noteIsOn) {
            // Map MIDI note to position (common range: 40-88)
            float notePos = fl::map_range<float, float>(currentMIDINote, 40.0f, 88.0f, 0.0f, 1.0f);
            notePos = fl::clamp(notePos, 0.0f, 1.0f);
            uint16_t note_x = notePos * (WIDTH - 1);
            uint16_t h = HEIGHT / 8;
            // magenta color for pitch
            framebuffer[frameBufferXY(note_x, h)] = CRGB(255, 0, 255);
        }

        if (true) {
            uint16_t fade_width = fade * (WIDTH - 1);
            uint16_t h = HEIGHT / 4;
            // yellow
            int index = frameBufferXY(fade_width, h);
            auto c = CRGB(255, 255, 0);
            framebuffer[index] = c;
        }
    }

    // now downscale the framebuffer to the led matrix
    downscale(framebuffer, frameBufferXY, leds, ledsXY);

    FastLED.show();
}

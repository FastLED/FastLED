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

#include "fl/sketch_macros.h"

#if !SKETCH_HAS_LOTS_OF_MEMORY

// Memory-constrained version for Arduino Uno, TeensyLC, etc.
// Simplified audio visualization with smaller matrix and basic FFT

#include "fl/audio.h"
#include "fl/fft.h"
#include "fl/math.h"
#include "fl/ui.h"
#include "fl/xypath.h"
#include "fx.h"

using namespace fl;

#define CONSTRAINED_HEIGHT 8
#define CONSTRAINED_WIDTH 16
#define CONSTRAINED_NUM_LEDS ((CONSTRAINED_WIDTH) * (CONSTRAINED_HEIGHT))
#define IS_SERPINTINE false

UITitle title("Audio Visualization (Memory Efficient)");
UIDescription description("Simplified audio visualization for memory-constrained platforms.");
UICheckbox enableFFT("Enable FFT visualization", true);
UISlider fadeToBlack("Fade to black by", 5, 0, 20, 1);

UIAudio audio("Audio");

CRGB leds[CONSTRAINED_NUM_LEDS];
XYMap ledsXY(CONSTRAINED_WIDTH, CONSTRAINED_HEIGHT, IS_SERPINTINE);

FFTBins fftOut(CONSTRAINED_WIDTH); // Match width for FFT bins

MaxFadeTracker audioFadeTracker(0.1, 0.1, 0.17, 44100);

void setup() {
    Serial.begin(115200);
    
    auto screenmap = ledsXY.toScreenMap();
    screenmap.setDiameter(.2);
    
    FastLED.addLeds<NEOPIXEL, 2>(leds, CONSTRAINED_NUM_LEDS).setScreenMap(screenmap);
    
    FL_WARN("Audio visualization setup complete (memory efficient version)");
}

void shiftUp() {
    // Fade each LED
    if (fadeToBlack.as_int()) {
        for (int i = 0; i < CONSTRAINED_NUM_LEDS; ++i) {
            leds[i].fadeToBlackBy(fadeToBlack.as_int());
        }
    }

    // Shift rows up
    for (int y = CONSTRAINED_HEIGHT - 1; y > 0; --y) {
        for (int x = 0; x < CONSTRAINED_WIDTH; ++x) {
            leds[ledsXY(x, y)] = leds[ledsXY(x, y - 1)];
        }
    }
    
    // Clear bottom row
    for (int x = 0; x < CONSTRAINED_WIDTH; ++x) {
        leds[ledsXY(x, 0)] = CRGB::Black;
    }
}

void loop() {
    while (AudioSample sample = audio.next()) {
        shiftUp();
        
        // Process audio with fade tracker
        float fade = audioFadeTracker(sample.pcm().data(), sample.pcm().size());
        
        if (enableFFT) {
            // Run FFT
            sample.fft(&fftOut);
            
            // Display FFT results on bottom row
            for (int i = 0; i < CONSTRAINED_WIDTH && i < static_cast<int>(fftOut.bins_raw.size()); ++i) {
                float v = fftOut.bins_db[i];
                v = fl::map_range<float, float>(v, 45, 70, 0, 1.0f);
                v = fl::clamp(v, 0.0f, 1.0f);
                
                uint8_t heatIndex = fl::map_range<float, uint8_t>(v, 0, 1, 0, 255);
                CRGB color = ColorFromPalette(HeatColors_p, heatIndex);
                color.fadeToBlackBy(255 - heatIndex);
                
                leds[ledsXY(i, 0)] = color;
            }
        } else {
            // Simple volume visualization
            int16_t maxSample = 0;
            for (size_t i = 0; i < sample.pcm().size(); ++i) {
                int16_t s = abs(sample.pcm()[i]);
                if (s > maxSample) maxSample = s;
            }
            
            float volume = fl::map_range<float, float>(maxSample, 0.0f, 32768.0f, 0.0f, 1.0f);
            volume = fl::clamp(volume, 0.0f, 1.0f);
            int volumeWidth = volume * CONSTRAINED_WIDTH;
            
            for (int x = 0; x < volumeWidth && x < CONSTRAINED_WIDTH; ++x) {
                leds[ledsXY(x, 0)] = CRGB(0, 255, 0);
            }
        }
        
        // Show fade tracker result
        if (fade > 0.0f) {
            int fadeX = fl::clamp(int(fade * CONSTRAINED_WIDTH), 0, CONSTRAINED_WIDTH - 1);
            leds[ledsXY(fadeX, CONSTRAINED_HEIGHT / 2)] = CRGB(255, 255, 0);
        }
    }

    FastLED.show();
}

#else

// Full-featured version for platforms with more memory

#include "fl/audio.h"
#include "fl/downscale.h"
#include "fl/draw_visitor.h"
#include "fl/fft.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include "fl/raster.h"
#include "fl/time_alpha.h"
#include "fl/ui.h"
#include "fl/xypath.h"
#include "fx.h"
#include "fx/time.h"

// Sketch.
#include "fl/function.h"

using namespace fl;

#define HEIGHT 128
#define WIDTH 128
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE false
#define TIME_ANIMATION 1000 // ms

UITitle title("Simple control of an xy path");
UIDescription description("This is more of a test for new features.");
UICheckbox enableVolumeVis("Enable volume visualization", false);
UICheckbox enableRMS("Enable RMS visualization", false);
UICheckbox enableFFT("Enable FFT visualization", true);
UICheckbox freeze("Freeze frame", false);
UIButton advanceFrame("Advance frame");
UISlider decayTimeSeconds("Fade time Seconds", .1, 0, 4, .02);
UISlider attackTimeSeconds("Attack time Seconds", .1, 0, 4, .02);
UISlider outputTimeSec("outputTimeSec", .17, 0, 2, .01);

UIAudio audio("Audio");
UISlider fadeToBlack("Fade to black by", 5, 0, 20, 1);

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

float rms(Slice<const int16_t> data) {
    double sumSq = 0.0;
    const int N = data.size();
    for (int i = 0; i < N; ++i) {
        int32_t x32 = int32_t(data[i]);
        sumSq += x32 * x32;
    }
    float rms = sqrt(float(sumSq) / N);
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
        FL_WARN("Fade time seconds: " << value);
    });
    attackTimeSeconds.onChanged([](float value) {
        audioFadeTracker.setAttackTime(value);
        FL_WARN("Attack time seconds: " << value);
    });
    outputTimeSec.onChanged([](float value) {
        audioFadeTracker.setOutputTime(value);
        FL_WARN("Output time seconds: " << value);
    });
    FastLED.addLeds<NEOPIXEL, 2>(leds, ledsXY.getTotal())
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
        memcpy(row1, row2, WIDTH * sizeof(CRGB));
    }
    CRGB* row = &framebuffer[frameBufferXY(0, 0)];
    memset(row, 0, sizeof(CRGB) * WIDTH);
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
        FL_WARN("Triggered");
    }
    // fl::clear(framebuffer);
    // fl::clear(framebuffer);

    static uint32_t frame = 0;

    // x = pointX.as_int();
    y = HEIGHT / 2;

    bool do_frame = doFrame();

    while (AudioSample sample = audio.next()) {
        if (!do_frame) {
            continue;
        }
        float fade = audioFadeTracker(sample.pcm().data(), sample.pcm().size());
        shiftUp();
        // FL_WARN("Audio sample size: " << sample.pcm().size());
        soundLevelMeter.processBlock(sample.pcm());
        // FL_WARN("")
        auto dbfs = soundLevelMeter.getDBFS();
        // FL_WARN("getDBFS: " << dbfs);
        int32_t max = 0;
        for (int i = 0; i < sample.pcm().size(); ++i) {
            int32_t x = ABS(sample.pcm()[i]);
            if (x > max) {
                max = x;
            }
        }
        float anim =
            fl::map_range<float, float>(max, 0.0f, 32768.0f, 0.0f, 1.0f);
        anim = fl::clamp(anim, 0.0f, 1.0f);

        x = fl::map_range<float, float>(anim, 0.0f, 1.0f, 0.0f, WIDTH - 1);
        // FL_WARN("x: " << x);

        // fft.run(sample.pcm(), &fftOut);
        sample.fft(&fftOut);

        // FASTLED_ASSERT(fftOut.bins_raw.size() == WIDTH_2X,
        //                "FFT bins size mismatch");

        if (enableFFT) {
            auto max_x = fftOut.bins_raw.size() - 1;
            for (int i = 0; i < fftOut.bins_raw.size(); ++i) {
                auto x = i;
                auto v = fftOut.bins_db[i];
                // Map audio intensity to a position in the heat palette (0-255)
                v = fl::map_range<float, float>(v, 45, 70, 0, 1.f);
                v = fl::clamp(v, 0.0f, 1.0f);
                uint8_t heatIndex =
                    fl::map_range<float, uint8_t>(v, 0, 1, 0, 255);

                // FL_WARN(v);

                // Use FastLED's built-in HeatColors palette
                auto c = ColorFromPalette(HeatColors_p, heatIndex);
                c.fadeToBlackBy(255 - heatIndex);
                framebuffer[frameBufferXY(x, 0)] = c;
                // FL_WARN("y: " << i << " b: " << b);
            }
        }

        if (enableVolumeVis) {
            framebuffer[frameBufferXY(x, HEIGHT / 2)] = CRGB(0, 255, 0);
        }

        if (enableRMS) {
            float rms = sample.rms();
            FL_WARN("RMS: " << rms);
            rms = fl::map_range<float, float>(rms, 0.0f, 32768.0f, 0.0f, 1.0f);
            rms = fl::clamp(rms, 0.0f, 1.0f) * WIDTH;
            framebuffer[frameBufferXY(rms, HEIGHT * 3 / 4)] = CRGB(0, 0, 255);
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

#endif  // SKETCH_HAS_LOTS_OF_MEMORY

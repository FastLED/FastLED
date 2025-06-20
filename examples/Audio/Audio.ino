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

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else

#include <Arduino.h>
#include <FastLED.h>

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

UITitle title("Audio Reactive Analysis Demo");
UIDescription description("Manual audio reactive demo with FL_WARN output for analysis.");
UICheckbox enableVolumeVis("Enable volume visualization", false);
UICheckbox enableRMS("Enable RMS visualization", false);
UICheckbox enableFFT("Enable FFT visualization", true);
UICheckbox enableManualDemo("Enable Manual Audio Demo", true);
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

// Manual Audio Demo Variables
struct AudioReactiveState {
    float peak_level = 0.0f;
    float rms_level = 0.0f;
    float smoothed_level = 0.0f;
    float bass_level = 0.0f;
    float mid_level = 0.0f;
    float treble_level = 0.0f;
    float beat_detection = 0.0f;
    uint32_t sample_count = 0;
    uint32_t last_print_time = 0;
};

AudioReactiveState audioState;

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

void analyzeAudioFrequencies(const FFTBins& fft, AudioReactiveState& state) {
    // Analyze different frequency bands
    int bassEnd = fft.bins_raw.size() * 0.1;    // 0-10% = bass
    int midEnd = fft.bins_raw.size() * 0.4;     // 10-40% = mid
    // 40-100% = treble
    
    float bassSum = 0.0f;
    float midSum = 0.0f;
    float trebleSum = 0.0f;
    
    // Bass frequencies
    for (int i = 0; i < bassEnd; i++) {
        bassSum += fft.bins_db[i];
    }
    
    // Mid frequencies
    for (int i = bassEnd; i < midEnd; i++) {
        midSum += fft.bins_db[i];
    }
    
    // Treble frequencies
    for (int i = midEnd; i < fft.bins_raw.size(); i++) {
        trebleSum += fft.bins_db[i];
    }
    
    // Average the bands
    state.bass_level = bassSum / bassEnd;
    state.mid_level = midSum / (midEnd - bassEnd);
    state.treble_level = trebleSum / (fft.bins_raw.size() - midEnd);
    
    // Simple beat detection based on bass energy change
    static float lastBassLevel = 0.0f;
    float bassChange = state.bass_level - lastBassLevel;
    state.beat_detection = (bassChange > 5.0f) ? 1.0f : 0.0f;
    lastBassLevel = state.bass_level;
}

void printAudioAnalysis(const AudioReactiveState& state) {
    uint32_t currentTime = millis();
    
    // Print analysis every 100ms to avoid spam
    if (currentTime - state.last_print_time < 100) {
        return;
    }
    
    FL_WARN("=== AUDIO REACTIVE ANALYSIS ===");
    FL_WARN("Sample #: " << state.sample_count);
    FL_WARN("Peak Level: " << state.peak_level << " (0.0-1.0)");
    FL_WARN("RMS Level: " << state.rms_level << " (0.0-1.0)");
    FL_WARN("Smoothed Level: " << state.smoothed_level << " (0.0-1.0)");
    FL_WARN("Bass Level: " << state.bass_level << " dB");
    FL_WARN("Mid Level: " << state.mid_level << " dB");
    FL_WARN("Treble Level: " << state.treble_level << " dB");
    FL_WARN("Beat Detection: " << (state.beat_detection > 0.5f ? "BEAT!" : "---"));
    
    // Convert to visual indicators
    int peakBars = (int)(state.peak_level * 20);
    int rmsBars = (int)(state.rms_level * 20);
    int smoothBars = (int)(state.smoothed_level * 20);
    
    FL_WARN("Peak   [" << String(peakBars, '#').c_str() << "]");
    FL_WARN("RMS    [" << String(rmsBars, '#').c_str() << "]");
    FL_WARN("Smooth [" << String(smoothBars, '#').c_str() << "]");
    FL_WARN("================================");
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
        
    FL_WARN("Audio Reactive Demo Started!");
    FL_WARN("Enable 'Manual Audio Demo' to see audio analysis output");
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
        
        // Manual Audio Demo Analysis
        if (enableManualDemo) {
            audioState.sample_count++;
            
            // Calculate peak level
            int32_t peak = 0;
            for (int i = 0; i < sample.pcm().size(); ++i) {
                int32_t x = ABS(sample.pcm()[i]);
                if (x > peak) {
                    peak = x;
                }
            }
            audioState.peak_level = fl::map_range<float, float>(peak, 0.0f, 32768.0f, 0.0f, 1.0f);
            audioState.peak_level = fl::clamp(audioState.peak_level, 0.0f, 1.0f);
            
            // Calculate RMS level
            audioState.rms_level = sample.rms();
            audioState.rms_level = fl::map_range<float, float>(audioState.rms_level, 0.0f, 32768.0f, 0.0f, 1.0f);
            audioState.rms_level = fl::clamp(audioState.rms_level, 0.0f, 1.0f);
            
            // Get smoothed level from fade tracker
            audioState.smoothed_level = audioFadeTracker(sample.pcm().data(), sample.pcm().size());
            
            // Analyze FFT frequencies
            sample.fft(&fftOut);
            analyzeAudioFrequencies(fftOut, audioState);
            
            // Print analysis
            printAudioAnalysis(audioState);
            audioState.last_print_time = millis();
        }
        
        // Original visualization code (unchanged)
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

        // FL_ASSERT(fftOut.bins_raw.size() == WIDTH_2X,
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

#endif  // __AVR__

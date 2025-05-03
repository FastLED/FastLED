

/*
This demo is best viewed using the FastLED compiler.
Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

#include <Arduino.h>
#include <FastLED.h>

#include "fl/audio.h"
#include "fl/draw_visitor.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include "fl/raster.h"
#include "fl/time_alpha.h"
#include "fl/ui.h"
#include "fl/xypath.h"
#include "fx/time.h"
#include "fl/fft.h"

// Sketch.
#include "fl/function.h"

using namespace fl;

#define HEIGHT 64
#define WIDTH 64
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true
#define TIME_ANIMATION 1000 // ms

CRGB leds[NUM_LEDS];
XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);
UITitle title("Simple control of an xy path");
UIDescription description("This is more of a test for new features.");
UICheckbox enableVolumeVis("Enable volume visualization", false);

UIAudio audio("Audio");
UISlider fadeToBlack("Fade to black by", 7, 0, 40, 1);

FFTBins fftOut(64);

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
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(screenmap);
}



void shiftUp() {

    // fade each led by 1%
    for (int y = HEIGHT - 1; y > 0; --y) {
        for (int x = 0; x < WIDTH; ++x) {
            auto& c = leds[xyMap(x, y)];
            c.fadeToBlackBy(fadeToBlack.as_int());
        }
    }

    for (int y = HEIGHT - 1; y > 0; --y) {
        for (int x = 0; x < WIDTH; ++x) {
            leds[xyMap(x, y)] = leds[xyMap(x, y - 1)];
        }
    }
    for (int x = 0; x < WIDTH; ++x) {
        leds[xyMap(x, 0)] = CRGB(0, 0, 0);
    }
}

void loop() {
    if (triggered) {
        FASTLED_WARN("Triggered");
    }
    //fl::clear(leds);
    
    // x = pointX.as_int();
    y = HEIGHT / 2;

    while (AudioSample sample = audio.next()) {
        shiftUp();
        //FASTLED_WARN("Audio sample size: " << sample.pcm().size());
        soundLevelMeter.processBlock(sample.pcm());
        // FASTLED_WARN("")
        auto dbfs = soundLevelMeter.getDBFS();
        //FASTLED_WARN("getDBFS: " << dbfs);
        int32_t max = 0;
        for (int i = 0; i < sample.pcm().size(); ++i) {
            int32_t x = ABS(sample.pcm()[i]);
            if (x > max) {
                max = x;
            }
        }
        float anim = fl::map_range<float, float>(max, 0.0f, 32768.0f, 0.0f, 1.0f);
        anim = fl::clamp(anim, 0.0f, 1.0f);

        x = fl::map_range<float, float>(anim, 0.0f, 1.0f, 0.0f, WIDTH - 1);
        // FASTLED_WARN("x: " << x);

        // fft.run(sample.pcm(), &fftOut);
        sample.fft(&fftOut);

        for (int i = 0; i < fftOut.bins_raw.size(); ++i) {
            auto x = i;
            auto v = fftOut.bins_db[i];
            // Map audio intensity to a position in the heat palette (0-255)
            v = fl::map_range<float, float>(v, 45, 70, 0, 1.f);
            v = fl::clamp(v, 0.0f, 1.0f);
            uint8_t heatIndex = fl::map_range<float, uint8_t>(v, 0, 1, 0, 255);

            //FASTLED_WARN(v);
            
            // Use FastLED's built-in HeatColors palette
            auto c = ColorFromPalette(HeatColors_p, heatIndex);
            c.fadeToBlackBy(255 - heatIndex);
            leds[xyMap(x, 0)] = c;
            //FASTLED_WARN("y: " << i << " b: " << b);
        }

        if (enableVolumeVis) {
            leds[xyMap(x, y)] = CRGB(0, 255, 0);
        }

        
    }



    //leds[xyMap(WIDTH/2, 0)] = CRGB(0, 255, 0);


    FastLED.show();
}

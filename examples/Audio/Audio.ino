

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

// Sketch.
#include "fl/function.h"
#include "src/wave.h"
#include "src/xypaths.h"

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

// UIButton trigger("My Trigger");
UISlider pointX("Point X", WIDTH / 2.0f, 0.0f, WIDTH - 1, 1.0f);
UISlider pointY("Point Y", HEIGHT / 2.0f, 0.0f, HEIGHT - 1, 1.0f);

UIButton button("second trigger");
UIAudio audio("Audio");

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
void loop() {
    if (triggered) {
        FASTLED_WARN("Triggered");
    }
    fl::clear(leds);

    // soundLevelMeter.processBlock(audio.next()->pcm().data(),
    // audio.next()->pcm().size());
    triggered = button.clicked();
    // x = pointX.as_int();
    y = pointY.as_int();

    Ptr<const AudioSample> sample = audio.next();
    if (sample) {
        soundLevelMeter.processBlock(sample->pcm());
        // FASTLED_WARN("")
        auto dbfs = soundLevelMeter.getDBFS();
        FASTLED_WARN("getDBFS: " << dbfs);

        const float mind = -60.0f; // choose your quiet‐floor
        float anim = (dbfs - mind) / (0.0f - mind);
        // clamp into [0,1]
        if (anim < 0.0f)
            anim = 0.0f;
        else if (anim > 1.0f)
            anim = 1.0f;

        // float anim  = powf(10.0f, dbfs * 0.05f);
        FASTLED_WARN("anim: " << anim);

        x = fl::map_range<float, float>(anim, 0.0f, 1.0f, 0.0f, WIDTH - 1);
        FASTLED_WARN("x: " << x);

        FASTLED_WARN("rms: " << rms(sample->pcm()));
    }

    leds[xyMap(x, y)] = CRGB(255, 0, 0);

    FastLED.show();
}

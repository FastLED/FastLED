/// @file    SdCard.ino
/// @brief   Demonstrates playing a video on FastLED.
/// @author  Zach Vorhies
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.


#ifdef __AVR__
void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
}
#else

#include "FastLED.h"
#include "Arduino.h"

#include "fx/2d/noisepalette.h"
// #include "fx/2d/animartrix.hpp"
#include "fx/fx_engine.h"
#include "fx/video.h"
#include "fl/file_system.h"
#include "fl/ui.h"
#include "fl/screenmap.h"
#include "fl/file_system.h"


using namespace fl;


#define LED_PIN 2
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define FPS 60
#define CHIP_SELECT_PIN 5



#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 32
#define NUM_VIDEO_FRAMES 2  // enables interpolation with > 1 frame.


#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define IS_SERPINTINE true


UITitle title("SDCard Demo - Mapped Video");
UIDescription description("Video data is streamed off of a SD card and displayed on a LED strip. The video data is mapped to the LED strip using a ScreenMap.");


CRGB leds[NUM_LEDS];
ScreenMap screenMap;

FileSystem filesystem;
Video video;
Video video2;

UISlider videoSpeed("Video Speed", 1.0f, -1, 2.0f, 0.01f);
UINumberField whichVideo("Which Video", 0, 0, 1);


bool gError = false;

void setup() {
    Serial.begin(115200);
    Serial.println("Sketch setup");
    if (!filesystem.beginSd(CHIP_SELECT_PIN)) {
        Serial.println("Failed to initialize file system.");
    }

    // Note that data/ is a special directory used by our wasm compiler. Any data
    // is placed in it will be included in the files.json file which the browser will
    // use to stream the file asynchroniously in during the runtime. For a real sd card
    // just place all this in the /data/ directory of the SD card to get matching
    // behavior.
    video = filesystem.openVideo("data/video.rgb", NUM_LEDS, FPS, 2);
    if (!video) {
      FASTLED_WARN("Failed to instantiate video");
      gError = true;
      return;
    }
    video2 = filesystem.openVideo("data/color_line_bubbles.rgb", NUM_LEDS, FPS, 2);
    if (!video2) {
      FASTLED_WARN("Failed to instantiate video2");
      gError = true;
      return;
    }
    ScreenMap screenMap;
    bool ok = filesystem.readScreenMap("data/screenmap.json", "strip1", &screenMap);
    if (!ok) {
      Serial.println("Failed to read screen map");
      gError = true;
      return;
    }

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenMap);
    FastLED.setBrightness(96);
    Serial.println("FastLED setup done");
}

void loop() {
    static bool s_first = true;
    if (s_first) {
      s_first = false;
      Serial.println("First loop.");
    }
    if (gError) {
      EVERY_N_SECONDS(1) {
        FASTLED_WARN("No loop because an error occured.");
      }
      return;
    }
    Video& vid = !bool(whichVideo.value()) ? video : video2;
    vid.setTimeScale(videoSpeed);
    uint32_t now = millis();
    vid.draw(now, leds);
    FastLED.show();
}

#endif

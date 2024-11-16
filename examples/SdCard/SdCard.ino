


#ifdef __AVR__
void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
}
#else

#include <FastLED.h>
#include "Arduino.h"


#include "fx/2d/noisepalette.hpp"
// #include "fx/2d/animartrix.hpp"
#include "fx/fx_engine.h"
#include "fx/video.h"
#include "file_system.h"
#include "ui.h"
#include "screenmap.h"
#include "file_system.h"


#include "screenmap.json.h"  // const char JSON_SCREEN_MAP[] = { ... }


#define LED_PIN 2
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define FPS 30
#define CHIP_SELECT_PIN 5


#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 32


#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define IS_SERPINTINE true


Title title("SDCard Demo - Mapped Video");
Description description("Video data is streamed off of a SD card and displayed on a LED strip. The video data is mapped to the LED strip using a ScreenMap.");


CRGB leds[NUM_LEDS];
ScreenMap screenMap;

FileSystem filesystem;
Video video;


void setup() {
    Serial.begin(115200);
    delay(1000); // sanity delay
    if (!filesystem.beginSd(CHIP_SELECT_PIN)) {
        Serial.println("Failed to initialize file system.");
    }
    // JSON_SCREEN_MAP
    FixedMap<Str, ScreenMap, 16> screenMaps;
    ScreenMap::ParseJson(JSON_SCREEN_MAP, &screenMaps);
    ScreenMap screenMap = screenMaps["strip1"];

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenMap);
    FastLED.setBrightness(96);
    //fxEngine.addFx(animartrix);
    video = filesystem.openVideo("data/video.dat", NUM_LEDS, FPS);
    if (!video) {
      Serial.println("Failed to instantiate video");
    }
}

void loop() {
    uint32_t now = millis();
    video.draw(now, leds);
    FastLED.show();
}

#endif

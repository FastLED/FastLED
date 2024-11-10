


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

#include "file_system.h"

#define LED_PIN 2
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define FPS 30
#define CHIP_SELECT_PIN 5


#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 32


#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];
XYMap xyMap(MATRIX_WIDTH, MATRIX_HEIGHT, IS_SERPINTINE);  // No serpentine

FileSystem filesystem;
Video video;


void setup() {
    Serial.begin(115200);
    delay(1000); // sanity delay
    if (!filesystem.beginSd(CHIP_SELECT_PIN)) {
        Serial.println("Failed to initialize file system.");
    }
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(xyMap);
    FastLED.setBrightness(96);
    //fxEngine.addFx(animartrix);
    video = filesystem.openVideo("data/video.dat", NUM_LEDS, FPS);
    if (!video) {
      Serial.println("Failed to instantiate video");
    }
}

void loop() {
    uint32_t now = millis();

    // fxEngine.draw(millis(), leds);
    video.draw(now, leds);

    EVERY_N_SECONDS(1){
        Serial.println("Drawing");
        for (int i = 0; i < NUM_LEDS;) {
            int nleft = MIN(8, NUM_LEDS - i);
            for (int j = 0; j < nleft; j++) {
                CRGB c = leds[i + j];
                Serial.print("(");
                Serial.print(c.r);
                Serial.print(", ");
                Serial.print(c.g);
                Serial.print(", ");
                Serial.print(c.b);
                Serial.print(")  ");
            }
            Serial.println();
            i += nleft;
        }
    }
    FastLED.show();
}

#endif

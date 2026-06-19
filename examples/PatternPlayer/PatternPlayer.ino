// @filter: (memory is large)

#include <FastLED.h>
#include "fl/video/pattern_player.h"

#define LED_PIN 2
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 1024
#define CHIP_SELECT_PIN 5

CRGB leds[NUM_LEDS];
fl::PatternPlayer player("data/video.fled");

void setup() {
    Serial.begin(115200);
    if (!player.begin(CHIP_SELECT_PIN, NUM_LEDS, 30)) {
        Serial.println("Failed to start player");
        return;
    }
    if (player.hasScreenMap()) {
        FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
            .setCorrection(TypicalLEDStrip)
            .setScreenMap(player.getScreenMap());
    } else {
        FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
            .setCorrection(TypicalLEDStrip);
    }
    FastLED.setBrightness(96);
}

void loop() {
    if (player.hasNewFrame()) {
        player.drawFrame(leds, NUM_LEDS);
        FastLED.show();
    }
}
